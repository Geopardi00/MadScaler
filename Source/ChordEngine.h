#pragma once

#include <array>
#include <string>
#include <vector>

// Music theory core for MadScaler: scales, diatonic chords and chord naming.
// Plain C++ with no JUCE dependency so it can be unit tested on its own.
namespace madscaler
{
    enum class ChordSize
    {
        triad,
        seventh
    };

    enum class ChordQuality
    {
        major,
        minor,
        diminished,
        augmented,
        major7,
        dominant7,
        minor7,
        halfDiminished7,
        diminished7,
        minorMajor7,
        augmentedMajor7,
        unknown
    };

    struct Scale
    {
        const char* name;
        std::vector<int> intervals; // semitones above the root, ascending, starting at 0
    };

    struct Chord
    {
        int degree = 0;         // 0-based scale degree
        int rootPitchClass = 0; // 0 = C ... 11 = B
        ChordQuality quality = ChordQuality::unknown;
        std::vector<int> notes; // MIDI note numbers, ascending
        std::string name;       // e.g. "Am7"
        std::string roman;      // e.g. "vi7"
    };

    // Every scale the picker offers, in display order. Only seven-note scales,
    // so every scale has seven diatonic chords.
    const std::vector<Scale>& getScales();

    // Note names using sharps: "C", "C#", "D" ...
    const std::array<const char*, 12>& getNoteNames();

    std::string getNoteName (int pitchClass);

    // Pitch classes (0-11) of the scale built on the given key.
    std::vector<int> getScalePitchClasses (int keyPitchClass, int scaleIndex);

    bool isInScale (int midiNote, int keyPitchClass, int scaleIndex);

    // Builds the diatonic chord on a scale degree by stacking scale thirds.
    // The chord root sits in the octave of baseNote (a MIDI note number of the
    // key's tonic, for example 60 for C4), so higher degrees sit above the tonic.
    Chord buildDiatonicChord (int keyPitchClass, int scaleIndex, int degree, ChordSize size, int baseNote);

    // Identifies a chord quality from semitone intervals above the root.
    ChordQuality identifyQuality (const std::vector<int>& intervalsAboveRoot);

    std::string getQualitySuffix (ChordQuality quality);

    std::string getRomanNumeral (int degree, ChordQuality quality);
} // namespace madscaler
