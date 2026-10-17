#include "BassMaker.h"
#include "ChordDetector.h"
#include "MelodyMaker.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>

namespace madscaler
{
    namespace
    {
        int wrap (int value) { return ((value % 12) + 12) % 12; }

        struct BassChord
        {
            double start = 0.0, end = 0.0;
            int root = 0;       // MIDI note of the root in the bass range
            int fifth = 7;      // MIDI note, above the root
            std::set<int> pitchClasses;
        };

        // The note with pitch class pc nearest to the middle of the range.
        int placeInRange (int pc, int low)
        {
            auto note = low + wrap (pc - low);
            return note;
        }
    } // namespace

    const std::vector<const char*>& getBassStyleNames()
    {
        static const std::vector<const char*> names { "Whole notes", "Roots on beats", "Pumping eighths", "Root and fifth",
                                                      "Root, fifth, octave", "Octave jumps", "Walking", "Syncopated 3+3+2",
                                                      "Disco octaves", "Dotted groove" };
        return names;
    }

    void swingNotes (std::vector<TimedNote>& notes, int amount)
    {
        const double amounts[] { 0.0, 0.12, 0.22, 0.33 };
        const auto swing = amounts[std::clamp (amount, 0, 3)];
        if (swing <= 0.0)
            return;

        for (auto& note : notes)
        {
            const auto fraction = note.startBeat - std::floor (note.startBeat + 1.0e-9);
            double shift = 0.0;
            if (std::abs (fraction - 0.5) < 1.0e-6)
                shift = swing * 0.5;
            else if (std::abs (fraction - 0.25) < 1.0e-6 || std::abs (fraction - 0.75) < 1.0e-6)
                shift = swing * 0.25;
            if (shift > 0.0)
            {
                note.startBeat += shift;
                note.lengthBeats = std::max (0.03, note.lengthBeats - shift);
            }
        }
    }

    void humaniseNotes (std::vector<TimedNote>& notes, int amount, double totalBeats, uint32_t seed)
    {
        const double timing[] { 0.0, 0.01, 0.02, 0.035 };
        const int velocity[] { 0, 5, 10, 16 };
        const auto level = std::clamp (amount, 0, 3);
        if (level == 0)
            return;

        std::mt19937 rng (seed * 2654435761u + 17u);
        std::uniform_real_distribution<double> shift (-timing[level], timing[level]);
        std::uniform_int_distribution<int> loudness (-velocity[level], velocity[level]);

        for (auto& note : notes)
        {
            const auto end = note.startBeat + note.lengthBeats;
            note.startBeat = std::clamp (note.startBeat + shift (rng), 0.0, std::max (0.0, totalBeats - 0.01));
            note.lengthBeats = std::max (0.02, std::min (end + shift (rng), totalBeats) - note.startBeat);
            note.velocity = std::clamp (note.velocity + loudness (rng), 1, 127);
        }
    }

    std::vector<TimedNote> generateBass (const std::vector<ProgressionSlot>& slots, const BassSettings& settings)
    {
        const auto low = std::clamp (settings.lowNote, 0, 100);
        const auto& scales = getMelodyScales();
        const auto& scale = scales[(size_t) std::clamp (settings.scaleIndex, 0, (int) scales.size() - 1)];
        std::set<int> scalePcs;
        for (auto interval : scale.intervals)
            scalePcs.insert (wrap (settings.keyPitchClass + interval));

        std::vector<BassChord> chords;
        double position = 0.0;
        for (const auto& slot : slots)
        {
            if (slot.notes.empty() || slot.lengthBeats <= 0.0)
                continue;

            BassChord chord;
            chord.start = position;
            chord.end = position + slot.lengthBeats;
            for (auto note : slot.notes)
                chord.pitchClasses.insert (wrap (note));

            const auto detected = detectChord (slot.notes);
            const auto rootPc = detected.valid ? detected.rootPitchClass : wrap (slot.notes.front());
            chord.root = placeInRange (rootPc, low);

            int fifthInterval = 7;
            if (chord.pitchClasses.count (wrap (rootPc + 7)) == 0)
            {
                if (chord.pitchClasses.count (wrap (rootPc + 6)) > 0) fifthInterval = 6;
                else if (chord.pitchClasses.count (wrap (rootPc + 8)) > 0) fifthInterval = 8;
            }
            chord.fifth = chord.root + fifthInterval;
            chords.push_back (chord);
            position = chord.end;
        }

        if (chords.empty())
            return {};

        const auto total = chords.back().end;
        std::mt19937 rng (settings.seed);
        auto pick = [&rng] (int count) { return std::uniform_int_distribution<int> (0, std::max (0, count - 1)) (rng); };

        const double gates[] { 0.5, 0.9, 1.0 };
        const auto gate = gates[std::clamp (settings.gate, 0, 2)];

        std::vector<TimedNote> notes;
        auto add = [&] (const BassChord& chord, double offset, double length, int note, int accent)
        {
            const auto start = chord.start + offset;
            if (start >= chord.end - 1.0e-9)
                return;
            length = std::min (length, chord.end - start);
            notes.push_back ({ start, length * gate, std::clamp (note, 0, 127), std::clamp (settings.velocity + accent, 1, 127) });
        };

        for (size_t c = 0; c < chords.size(); ++c)
        {
            const auto& chord = chords[c];
            const auto length = chord.end - chord.start;
            const auto root = chord.root, fifth = chord.fifth, octave = chord.root + 12;
            const auto nextRoot = c + 1 < chords.size() ? chords[c + 1].root : chords.front().root;

            switch (settings.style)
            {
                case BassStyle::wholeNotes:
                    add (chord, 0.0, length, root, 6);
                    break;

                case BassStyle::quarterRoots:
                    for (double b = 0.0; b < length - 1.0e-9; b += 1.0)
                        add (chord, b, 1.0, root, b == 0.0 ? 8 : 0);
                    break;

                case BassStyle::pumping:
                    for (double b = 0.0; b < length - 1.0e-9; b += 0.5)
                        add (chord, b, 0.5, root, std::fmod (b, 1.0) == 0.0 ? 6 : -6);
                    break;

                case BassStyle::rootFifth:
                    for (double b = 0.0; b < length - 1.0e-9; b += 1.0)
                        add (chord, b, 1.0, ((int) b % 2 == 0) ? root : fifth, b == 0.0 ? 8 : 0);
                    break;

                case BassStyle::rootFifthOctave:
                {
                    const int shape[] { root, fifth, octave, fifth };
                    for (double b = 0.0; b < length - 1.0e-9; b += 1.0)
                        add (chord, b, 1.0, shape[(int) b % 4], b == 0.0 ? 8 : 0);
                    break;
                }

                case BassStyle::octaves:
                    for (double b = 0.0; b < length - 1.0e-9; b += 0.5)
                        add (chord, b, 0.5, std::fmod (b, 1.0) == 0.0 ? root : octave, std::fmod (b, 1.0) == 0.0 ? 6 : -4);
                    break;

                case BassStyle::offbeatOctave:
                    for (double b = 0.0; b < length - 1.0e-9; b += 1.0)
                    {
                        add (chord, b, 0.5, root, 6);
                        add (chord, b + 0.5, 0.5, octave, -2);
                    }
                    break;

                case BassStyle::syncopated:
                    for (double bar = 0.0; bar < length - 1.0e-9; bar += 2.0)
                    {
                        add (chord, bar, 0.75, root, 8);
                        add (chord, bar + 0.75, 0.75, root, -2);
                        add (chord, bar + 1.5, 0.5, fifth, 0);
                    }
                    break;

                case BassStyle::dotted:
                    for (double bar = 0.0; bar < length - 1.0e-9; bar += 4.0)
                    {
                        add (chord, bar, 1.5, root, 8);
                        add (chord, bar + 1.5, 1.0, root, -2);
                        add (chord, bar + 3.0, 1.0, fifth, 0);
                    }
                    break;

                case BassStyle::walking:
                {
                    // Quarter notes: the root, then notes of the chord and scale
                    // that head for the next root, then a note a step away from it.
                    const auto beats = std::max (1, (int) std::floor (length + 1.0e-9));
                    auto target = nextRoot;
                    if (target - root > 6) target -= 12;
                    if (root - target > 6) target += 12;

                    std::vector<int> pool;
                    for (int n = low - 5; n <= low + 19; ++n)
                        if (chord.pitchClasses.count (wrap (n)) > 0 || scalePcs.count (wrap (n)) > 0)
                            pool.push_back (n);

                    int current = root;
                    for (int beat = 0; beat < beats; ++beat)
                    {
                        int note = current;
                        if (beat == 0)
                        {
                            note = root;
                        }
                        else if (beat == beats - 1)
                        {
                            // Approach: a half step below or above, or the fifth of the target.
                            const auto choice = pick (3);
                            note = choice == 0 ? target - 1 : choice == 1 ? target + 1 : target + 7 - (target + 7 > low + 19 ? 12 : 0);
                        }
                        else
                        {
                            // Move towards the target by chord tones where possible.
                            const int direction = target >= current ? 1 : -1;
                            int best = current;
                            for (int distance = 1; distance <= 5; ++distance)
                            {
                                const auto candidate = current + direction * distance;
                                const bool chordTone = chord.pitchClasses.count (wrap (candidate)) > 0;
                                if (std::find (pool.begin(), pool.end(), candidate) != pool.end() && (chordTone || distance <= 2))
                                {
                                    best = candidate;
                                    if (chordTone || pick (2) == 0)
                                        break;
                                }
                            }
                            note = best == current ? current + direction * 2 : best;
                        }
                        note = std::clamp (note, low - 5, low + 19);
                        add (chord, (double) beat, 1.0, note, beat == 0 ? 8 : 0);
                        current = note;
                    }
                    break;
                }
            }

            // A passing note into the next chord, on the last off-beat.
            if (settings.leadIn && settings.style != BassStyle::walking && length >= 1.0 && chords.size() > 1)
            {
                auto target = nextRoot;
                if (target - root > 6) target -= 12;
                if (root - target > 6) target += 12;
                if (target != root)
                {
                    const auto from = target > root ? target - 1 : target + 1;
                    const auto start = chord.end - 0.5;
                    notes.erase (std::remove_if (notes.begin(), notes.end(),
                                                 [start] (const TimedNote& n) { return n.startBeat >= start - 1.0e-9; }),
                                 notes.end());
                    for (auto& n : notes)
                        if (n.startBeat < start && n.startBeat + n.lengthBeats > start)
                            n.lengthBeats = start - n.startBeat;
                    notes.push_back ({ start, 0.5 * gate, std::clamp (from, 0, 127), std::clamp (settings.velocity - 4, 1, 127) });
                }
            }
        }

        swingNotes (notes, settings.swing);
        humaniseNotes (notes, settings.humanise, total, settings.seed);
        std::stable_sort (notes.begin(), notes.end(), [] (const TimedNote& a, const TimedNote& b) { return a.startBeat < b.startBeat; });
        return notes;
    }
} // namespace madscaler
