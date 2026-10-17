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

    // The order the arpeggio plays the chord's notes in. New styles go at the
    // end, since saved projects store the number.
    enum class ArpStyle
    {
        up,
        down,
        upDown,
        alberti,      // low, high, middle, high
        pinky,        // the lowest note between the others ("Pedal low")
        random,
        downUp,
        upDownRepeat, // up and down, playing the top and bottom notes twice
        pedalHigh,    // the highest note between the others
        converge,     // outside in: low, high, 2nd low, 2nd high...
        diverge,      // inside out, from the middle
        skip,         // 1 3 2 4 3 5...
        steps,        // 1 2 3, 2 3 4, 3 4 5...
        octaves,      // each note, then the same note an octave up
        randomWalk    // moves to a neighbouring note, up or down
    };

    const std::vector<const char*>& getArpStyleNames();

    // Which notes the arpeggio uses.
    enum class ArpNotes
    {
        chord,
        add9,
        add7,
        add6,
        rootFifth
    };

    const std::vector<const char*>& getArpNotesNames();

    // How notes are made louder and softer.
    enum class Accent
    {
        beats,      // on the beat a bit louder
        even,
        everyThird, // every third note, across the beat
        rising      // grows louder through each chord
    };

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

        int gate = 1;           // 0 staccato, 1 normal, 2 legato
        int swing = 0;          // 0 none, 1 light, 2 medium, 3 heavy
        int humanise = 0;       // 0 none ... 3 loose timing and velocity
        Accent accent = Accent::beats;

        // Arpeggio only.
        int arpOctaves = 2;     // 1-4, upwards from lowNote
        ArpNotes arpNotes = ArpNotes::chord;
        int variation = 0;      // 0 none ... 3 wild: rests, repeats, octave jumps, neighbours
        bool bassNote = false;  // hold the chord's root below the arpeggio
        bool keepFlowing = false; // carry on through chord changes instead of restarting
    };

    // One pass of a melody or arpeggio over the chords, in beats from the start.
    std::vector<TimedNote> generateMelody (const std::vector<ProgressionSlot>& chords, const MelodySettings& settings);
} // namespace madscaler
