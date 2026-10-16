#pragma once

#include "PatternEngine.h"

#include <string>
#include <vector>

// A chord progression: an ordered list of chords, each held for a number of
// beats. Plain C++ with no JUCE dependency.
namespace madscaler
{
    struct ProgressionSlot
    {
        std::string name;
        std::vector<int> notes; // MIDI notes, ascending
        double lengthBeats = 4.0;
    };

    struct Progression
    {
        static constexpr int maxSlots = 16;

        std::vector<ProgressionSlot> slots;

        double totalBeats() const;

        // Index of the slot sounding at a beat position. Positions loop over
        // the whole progression. Returns -1 for an empty progression.
        int slotAt (double beat) const;

        // Beat (within one loop) where a slot starts.
        double slotStart (int index) const;
    };

    struct TimedNote
    {
        double startBeat = 0.0;
        double lengthBeats = 0.0;
        int note = 0;
        int velocity = 100;
    };

    // Renders one pass of the progression to notes, as MadScaler would play it:
    // chords held for their full length, or performed with a pattern when one
    // is given. Used for exporting MIDI.
    // bpm only matters for strums, whose gaps are in milliseconds.
    std::vector<TimedNote> renderProgression (const Progression& progression, int velocity,
                                              const PatternSettings* pattern, double bpm = 120.0);
} // namespace madscaler
