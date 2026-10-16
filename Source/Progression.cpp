#include "Progression.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace madscaler
{
    double Progression::totalBeats() const
    {
        double total = 0.0;
        for (const auto& slot : slots)
            total += std::max (0.0, slot.lengthBeats);
        return total;
    }

    int Progression::slotAt (double beat) const
    {
        const auto total = totalBeats();
        if (slots.empty() || total <= 0.0)
            return -1;

        auto position = std::fmod (beat, total);
        if (position < 0.0)
            position += total;

        double start = 0.0;
        for (size_t i = 0; i < slots.size(); ++i)
        {
            start += std::max (0.0, slots[i].lengthBeats);
            if (position < start - 1.0e-9)
                return (int) i;
        }
        return (int) slots.size() - 1;
    }

    double Progression::slotStart (int index) const
    {
        double start = 0.0;
        for (int i = 0; i < index && i < (int) slots.size(); ++i)
            start += std::max (0.0, slots[(size_t) i].lengthBeats);
        return start;
    }

    std::vector<TimedNote> renderProgression (const Progression& progression, int velocity,
                                              const PatternSettings* pattern, double bpm)
    {
        std::vector<TimedNote> result;

        if (pattern == nullptr)
        {
            double start = 0.0;
            for (const auto& slot : progression.slots)
            {
                for (auto note : slot.notes)
                    result.push_back ({ start, slot.lengthBeats, note, velocity });
                start += slot.lengthBeats;
            }
            return result;
        }

        // Run the real pattern engine offline at 1000 "samples" per beat.
        constexpr double samplesPerBeat = 1000.0;
        bpm = bpm > 1.0 ? bpm : 120.0;
        const auto sampleRate = samplesPerBeat * bpm / 60.0;
        PatternEngine engine;
        engine.reset();
        auto settings = *pattern;
        settings.velocity = velocity;
        engine.setSettings (settings);

        std::vector<PatternEvent> events;
        std::map<int, double> openNotes;
        const int blockSize = 100;
        const auto totalSamples = (long long) std::llround (progression.totalBeats() * samplesPerBeat);

        int currentSlot = -1;
        for (long long blockStart = 0; blockStart <= totalSamples; blockStart += blockSize)
        {
            events.clear();
            engine.beginBlock (sampleRate, bpm, true, (double) blockStart / samplesPerBeat);

            // Slot changes inside this block, in order.
            for (int offset = 0; offset < blockSize; ++offset)
            {
                const auto time = blockStart + offset;
                const auto slot = time >= totalSamples ? -1 : progression.slotAt ((double) time / samplesPerBeat);
                if (slot != currentSlot)
                {
                    currentSlot = slot;
                    if (slot >= 0)
                    {
                        const auto& notes = progression.slots[(size_t) slot].notes;
                        engine.setChord (notes.data(), (int) notes.size(), offset, events);
                    }
                    else
                    {
                        engine.setChord (nullptr, 0, offset, events);
                    }
                }
            }

            engine.endBlock (blockSize, events);

            for (const auto& event : events)
            {
                const auto beat = (double) (blockStart + event.sampleOffset) / samplesPerBeat;
                if (event.noteOn)
                {
                    openNotes[event.note] = beat;
                    result.push_back ({ beat, 0.0, event.note, event.velocity });
                }
                else if (openNotes.count (event.note) != 0)
                {
                    const auto startBeat = openNotes[event.note];
                    openNotes.erase (event.note);
                    for (auto it = result.rbegin(); it != result.rend(); ++it)
                        if (it->note == event.note && it->lengthBeats == 0.0 && it->startBeat == startBeat)
                        {
                            it->lengthBeats = std::max (beat - startBeat, 1.0e-3);
                            break;
                        }
                }
            }

            if (currentSlot < 0 && ! engine.isSounding() && blockStart >= totalSamples)
                break;
        }

        result.erase (std::remove_if (result.begin(), result.end(),
                                      [] (const TimedNote& n) { return n.lengthBeats <= 0.0; }),
                      result.end());
        std::sort (result.begin(), result.end(),
                   [] (const TimedNote& a, const TimedNote& b) { return a.startBeat != b.startBeat ? a.startBeat < b.startBeat : a.note < b.note; });
        return result;
    }
} // namespace madscaler
