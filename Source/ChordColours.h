#pragma once

#include <string>
#include <vector>

// Chord colours for MadScaler: richer or borrowed versions of a diatonic chord,
// secondary dominants, next-chord suggestions and Circle of Fifths chords.
// Plain C++ with no JUCE dependency.
namespace madscaler
{
    struct ColourChord
    {
        std::string name;       // e.g. "Dm9"
        std::string label;      // what it is, e.g. "9", "from Minor", "V7/ii"
        int rootPitchClass = 0;
        std::vector<int> notes; // root-position close voicing, ascending MIDI notes
    };

    struct ColourGroup
    {
        std::string title; // "Extensions", "Borrowed", "Secondary"
        std::vector<ColourChord> chords;
    };

    // Colours for the chord on a scale degree. baseNote is the MIDI note of the
    // key's tonic in the chosen octave, as for buildDiatonicChord. seventhChords
    // picks 7th or triad versions of the borrowed chords.
    std::vector<ColourGroup> getChordColours (int keyPitchClass, int scaleIndex, int degree,
                                              bool seventhChords, int baseNote);

    // Scale degrees that commonly follow the chord on a degree, most common first.
    std::vector<int> suggestNextDegrees (int scaleIndex, int degree);

    // Major and minor triads for the Circle of Fifths. Position 0 is C (or Am),
    // each step clockwise adds a fifth.
    int circlePitchClass (int position, bool minor);
    ColourChord buildCircleChord (int position, bool minor, int baseNote);

    // True when every note of the chord is in the scale.
    bool isChordInScale (const std::vector<int>& notes, int keyPitchClass, int scaleIndex);
} // namespace madscaler
