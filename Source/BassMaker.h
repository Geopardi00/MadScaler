#pragma once

#include "Progression.h"

#include <cstdint>
#include <vector>

// BassMaker: basslines that follow a set of chords. Plain C++ with no JUCE
// dependency.
namespace madscaler
{
    // New styles go at the end, since saved projects store the number.
    enum class BassStyle
    {
        wholeNotes,     // the root held for the whole chord
        quarterRoots,   // the root on every beat
        pumping,        // the root on every eighth
        rootFifth,      // root and fifth on alternate beats
        rootFifthOctave,// root, fifth, octave, fifth
        octaves,        // root and the octave above, in eighths
        walking,        // chord and scale notes leading to the next chord
        syncopated,     // 3+3+2: root, root, fifth
        offbeatOctave,  // disco: root on the beat, octave on the off-beat
        dotted          // root on 1, the "and" of 2, and the fifth on 4
    };

    const std::vector<const char*>& getBassStyleNames();

    struct BassSettings
    {
        BassStyle style = BassStyle::rootFifth;
        int lowNote = 36;          // the bass stays from here to about an octave and a half up
        int keyPitchClass = 0;     // scale for walking and passing notes
        int scaleIndex = 0;        // index into getMelodyScales()
        int velocity = 100;
        int gate = 1;              // 0 staccato, 1 normal, 2 legato
        int swing = 0;             // 0 none ... 3 heavy
        int humanise = 0;          // 0 none ... 3 loose
        bool leadIn = false;       // a passing note into each chord change
        uint32_t seed = 1;
    };

    // One pass of the bassline over the chords, in beats from the start.
    std::vector<TimedNote> generateBass (const std::vector<ProgressionSlot>& chords, const BassSettings& settings);

    // Nudges timing and velocity a little, the same way for the same seed.
    // Notes stay inside 0..totalBeats. amount: 0 none ... 3 loose.
    void humaniseNotes (std::vector<TimedNote>& notes, int amount, double totalBeats, uint32_t seed);

    // Pushes off-beat eighths (and off-beat sixteenths) late. amount 0 ... 3.
    void swingNotes (std::vector<TimedNote>& notes, int amount);
} // namespace madscaler
