#include "PatternEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace madscaler
{
    void PatternEngine::reset()
    {
        numChordNotes = 0;
        numSounding = 0;
        numPending = 0;
        blockStart = 0;
        renderedTo = 0;
        beatsAtBlockStart = 0.0;
        freeBeats = 0.0;
        hostWasPlaying = false;
        lastHostBeats = 0.0;
        lastStepIndex = -1;
        arpPosition = 0;
    }

    void PatternEngine::beginBlock (double sampleRate, double bpm, bool hostPlaying, double ppqAtBlockStart)
    {
        bpm = bpm > 1.0 ? bpm : 120.0;
        this->sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
        samplesPerBeat = std::max (1.0, sampleRate * 60.0 / bpm);
        renderedTo = blockStart;

        if (hostPlaying)
        {
            beatsAtBlockStart = ppqAtBlockStart;

            // Transport started, jumped or looped: line the step counter up with
            // the new position, so a step that falls exactly here still plays.
            if (! hostWasPlaying || std::abs (ppqAtBlockStart - lastHostBeats) > 0.01)
                lastStepIndex = (int64_t) std::ceil (ppqAtBlockStart / settings.stepBeats - 1.0e-9) - 1;
        }
        else
        {
            beatsAtBlockStart = freeBeats;
        }

        hostWasPlaying = hostPlaying;
    }

    void PatternEngine::endBlock (int numSamples, std::vector<PatternEvent>& out)
    {
        renderTo (blockStart + numSamples, out);

        const auto beatsAtEnd = beatsAtBlockStart + numSamples / samplesPerBeat;
        freeBeats = beatsAtEnd;
        lastHostBeats = beatsAtEnd;
        blockStart += numSamples;
        renderedTo = blockStart;
    }

    double PatternEngine::beatsAt (int64_t time) const
    {
        return beatsAtBlockStart + (double) (time - blockStart) / samplesPerBeat;
    }

    int64_t PatternEngine::timeOfBeat (double beat) const
    {
        return blockStart + (int64_t) std::ceil ((beat - beatsAtBlockStart) * samplesPerBeat - 1.0e-6);
    }

    int64_t PatternEngine::stepIndexAt (int64_t time) const
    {
        return (int64_t) std::floor (beatsAt (time) / settings.stepBeats + 1.0e-9);
    }

    int64_t PatternEngine::nextStepTime() const
    {
        return timeOfBeat ((double) (lastStepIndex + 1) * settings.stepBeats);
    }

    bool PatternEngine::isStrum() const
    {
        return settings.mode == PatternMode::strumUp || settings.mode == PatternMode::strumDown;
    }

    void PatternEngine::emit (int64_t time, bool noteOn, int note, std::vector<PatternEvent>& out)
    {
        PatternEvent event;
        event.sampleOffset = (int) std::max<int64_t> (0, time - blockStart);
        event.noteOn = noteOn;
        event.note = note;
        event.velocity = noteOn ? std::clamp (settings.velocity, 1, 127) : 0;
        out.push_back (event);
    }

    void PatternEngine::releaseAll (int64_t time, std::vector<PatternEvent>& out)
    {
        for (int i = 0; i < numSounding; ++i)
            emit (time, false, sounding[(size_t) i].note, out);

        numSounding = 0;
        numPending = 0;
    }

    void PatternEngine::allNotesOff (int sampleOffset, std::vector<PatternEvent>& out)
    {
        const auto time = std::max (blockStart + sampleOffset, renderedTo);
        releaseAll (time, out);
        numChordNotes = 0;
    }

    int PatternEngine::sequenceLength() const
    {
        const auto length = numChordNotes * std::clamp (settings.octaves, 1, 4);
        if (settings.mode == PatternMode::arpUpDown && length > 1)
            return 2 * length - 2;
        return length;
    }

    int PatternEngine::sequenceNote (int position)
    {
        const auto octaves = std::clamp (settings.octaves, 1, 4);
        const auto length = numChordNotes * octaves;
        if (length <= 0)
            return -1;

        int index = 0;

        switch (settings.mode)
        {
            case PatternMode::arpDown:
                index = length - 1 - position % length;
                break;

            case PatternMode::arpUpDown:
            {
                const auto cycle = sequenceLength();
                const auto p = position % cycle;
                index = p < length ? p : cycle - p;
                break;
            }

            case PatternMode::arpRandom:
                randomState = randomState * 1664525u + 1013904223u;
                index = (int) ((randomState >> 8) % (uint32_t) length);
                break;

            case PatternMode::arpUp:
            case PatternMode::strumUp:
            case PatternMode::strumDown:
            case PatternMode::pulse:
                index = position % length;
                break;
        }

        auto note = chord[(size_t) (index % numChordNotes)] + 12 * (index / numChordNotes);
        while (note > 127)
            note -= 12;
        return note;
    }

    void PatternEngine::playStep (int64_t time, std::vector<PatternEvent>& out)
    {
        if (numChordNotes == 0)
            return;

        const auto stepSamples = settings.stepBeats * samplesPerBeat;
        const auto offTime = time + std::max<int64_t> (1, (int64_t) (std::clamp (settings.gate, 0.05f, 1.0f) * stepSamples));

        auto start = [&] (int note)
        {
            if (note < 0)
                return;

            // Retrigger a note that is somehow still sounding instead of stacking it.
            for (int i = 0; i < numSounding; ++i)
                if (sounding[(size_t) i].note == note)
                {
                    emit (time, false, note, out);
                    sounding[(size_t) i] = sounding[(size_t) --numSounding];
                    break;
                }

            if (numSounding < (int) sounding.size())
            {
                emit (time, true, note, out);
                sounding[(size_t) numSounding++] = { note, offTime };
            }
        };

        if (settings.mode == PatternMode::pulse)
        {
            for (int i = 0; i < numChordNotes; ++i)
                start (chord[(size_t) i]);
        }
        else
        {
            start (sequenceNote (arpPosition++));
        }
    }

    void PatternEngine::startStrum (int64_t time)
    {
        const auto strumGapSamples = (int64_t) (std::clamp (settings.strumMs, 0.0, 500.0) * 0.001 * sampleRate);

        numPending = 0;
        for (int i = 0; i < numChordNotes; ++i)
        {
            const auto index = settings.mode == PatternMode::strumDown ? numChordNotes - 1 - i : i;
            pending[(size_t) numPending++] = { chord[(size_t) index], time + (int64_t) i * strumGapSamples };
        }
    }

    void PatternEngine::setChord (const int* notes, int numNotes, int sampleOffset, std::vector<PatternEvent>& out)
    {
        const auto time = std::max (blockStart + sampleOffset, renderedTo);
        renderTo (time, out);

        const auto wasActive = numChordNotes > 0;

        numChordNotes = std::clamp (numNotes, 0, maxChordNotes);
        for (int i = 0; i < numChordNotes; ++i)
            chord[(size_t) i] = std::clamp (notes[i], 0, 127);
        std::sort (chord.begin(), chord.begin() + numChordNotes);

        if (numChordNotes == 0)
        {
            releaseAll (time, out);
            return;
        }

        if (isStrum())
        {
            releaseAll (time, out);
            startStrum (time);
            renderTo (time + 1, out); // the first strummed note sounds right away
            return;
        }

        if (wasActive)
            return; // the running arpeggio or pulse carries on with the new notes

        // Starting from silence: play the first step right away, then follow the grid.
        releaseAll (time, out);
        arpPosition = 0;

        if (hostWasPlaying)
        {
            lastStepIndex = stepIndexAt (time);
            const auto stepSamples = settings.stepBeats * samplesPerBeat;
            if ((double) (nextStepTime() - time) < 0.3 * stepSamples)
                ++lastStepIndex; // too close to the next grid step: let this one stand in for it
        }
        else
        {
            // Free-running: restart the internal clock so the pattern starts now.
            beatsAtBlockStart = -(double) (time - blockStart) / samplesPerBeat;
            lastStepIndex = 0;
        }

        playStep (time, out);
    }

    void PatternEngine::renderTo (int64_t target, std::vector<PatternEvent>& out)
    {
        const auto patterned = ! isStrum() && numChordNotes > 0;

        for (;;)
        {
            auto next = std::numeric_limits<int64_t>::max();

            for (int i = 0; i < numSounding; ++i)
                if (sounding[(size_t) i].offTime >= 0)
                    next = std::min (next, sounding[(size_t) i].offTime);

            for (int i = 0; i < numPending; ++i)
                next = std::min (next, pending[(size_t) i].onTime);

            const auto stepTime = patterned ? nextStepTime() : std::numeric_limits<int64_t>::max();
            next = std::min (next, stepTime);

            if (next >= target)
                break;

            const auto time = std::max (next, renderedTo);

            // Note-offs first, so a note ending exactly where the next begins is released.
            for (int i = 0; i < numSounding;)
            {
                const auto& s = sounding[(size_t) i];
                if (s.offTime >= 0 && s.offTime <= next)
                {
                    emit (time, false, s.note, out);
                    sounding[(size_t) i] = sounding[(size_t) --numSounding];
                }
                else
                {
                    ++i;
                }
            }

            for (int i = 0; i < numPending;)
            {
                if (pending[(size_t) i].onTime <= next)
                {
                    if (numSounding < (int) sounding.size())
                    {
                        emit (time, true, pending[(size_t) i].note, out);
                        sounding[(size_t) numSounding++] = { pending[(size_t) i].note, -1 };
                    }
                    // Keep the strum order: shift the rest down.
                    for (int j = i; j < numPending - 1; ++j)
                        pending[(size_t) j] = pending[(size_t) j + 1];
                    --numPending;
                }
                else
                {
                    ++i;
                }
            }

            if (stepTime <= next)
            {
                ++lastStepIndex;
                playStep (time, out);
            }

            renderedTo = time;
        }

        renderedTo = std::max (renderedTo, target);
    }
} // namespace madscaler
