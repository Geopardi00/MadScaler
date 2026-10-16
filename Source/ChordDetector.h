#pragma once

#include <string>
#include <vector>

// Chord and key detection for MadScaler. Plain C++ with no JUCE dependency.
namespace madscaler
{
    struct DetectedChord
    {
        bool valid = false;
        int rootPitchClass = 0;
        int bassPitchClass = 0;
        std::string name;       // e.g. "Am7", "C/E", "Gsus4"
        std::vector<int> notes; // the MIDI notes that were played, ascending
    };

    // Names the chord formed by a set of MIDI notes. Needs at least two distinct
    // pitch classes; returns an invalid result when nothing matches.
    DetectedChord detectChord (const std::vector<int>& midiNotes);

    // The same, with note names spelled for a key (Eb rather than D# in Bb major).
    DetectedChord detectChord (const std::vector<int>& midiNotes, int keyPitchClass, int scaleIndex);

    struct KeySuggestion
    {
        int keyPitchClass = 0;
        int scaleIndex = 0;  // index into getScales()
        int notesOutside = 0; // played pitch classes that are not in the scale
        std::string name;    // e.g. "A Natural Minor"
    };

    // Ranks keys and scales by how well they fit the played chords.
    // pitchClassWeights[pc] counts how often each pitch class was played;
    // chordRoots lists the detected chord roots in playing order.
    std::vector<KeySuggestion> suggestKeys (const std::vector<int>& pitchClassWeights,
                                            const std::vector<int>& chordRoots,
                                            size_t maxSuggestions);
} // namespace madscaler
