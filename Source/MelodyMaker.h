#pragma once

#include "Progression.h"

#include <cstdint>
#include <string>
#include <vector>

// MelodyMaker: suggests scales that fit a set of chords and invents melodies
// and rhythmic arpeggios over them. Plain C++ with no JUCE dependency.
namespace madscaler
{
    struct MelodyScale
    {
        const char* name;
        std::vector<int> intervals; // semitones above the root
    };

    // The seven-note scales of getScales() plus pentatonic and blues scales.
    const std::vector<MelodyScale>& getMelodyScales();

    struct ScaleFit
    {
        int keyPitchClass = 0;
        int scaleIndex = 0;   // index into getMelodyScales()
        int notesOutside = 0; // chord notes (counted per chord) that are not in the scale
        int score = 0;
        std::string name;     // e.g. "A Minor Pentatonic"
    };

    // Scales that suit the chords, best first.
    std::vector<ScaleFit> suggestMelodyScales (const std::vector<ProgressionSlot>& chords, size_t maxSuggestions);

    struct RhythmPattern
    {
        const char* name;
        double lengthBeats;                        // the pattern repeats every lengthBeats
        std::vector<std::pair<double, double>> hits; // {start, length} in beats
    };

    const std::vector<RhythmPattern>& getRhythmPatterns();

    enum class MelodyMode
    {
        melody,  // a tune: chord tones on strong beats, scale steps between
        arpeggio // the chord's notes one at a time
    };

    enum class ArpStyle
    {
        up,
        down,
        upDown,
        alberti, // low, high, middle, high
        pinky,   // the lowest note between rising notes
        random
    };

    const std::vector<const char*>& getArpStyleNames();

    struct MelodySettings
    {
        MelodyMode mode = MelodyMode::melody;
        int rhythm = 0;         // index into getRhythmPatterns()
        ArpStyle arpStyle = ArpStyle::up;
        int density = 1;        // 0 sparse, 1 medium, 2 busy (melody mode)
        int lowNote = 60;       // range of the melody
        int highNote = 79;
        int keyPitchClass = 0;  // scale the melody uses
        int scaleIndex = 0;     // index into getMelodyScales()
        int velocity = 100;
        uint32_t seed = 1;      // each seed gives a different melody
    };

    // One pass of a melody or arpeggio over the chords, in beats from the start.
    std::vector<TimedNote> generateMelody (const std::vector<ProgressionSlot>& chords, const MelodySettings& settings);
} // namespace madscaler
