#pragma once

#include "Progression.h"

#include <string>
#include <vector>

// MadScaler's own library of chord progressions, tagged by style, mood and use.
// Progressions are written in Roman numerals relative to the key, so each one
// works in any key. Plain C++ with no JUCE dependency.
//
// Chord tokens: an optional b or # (relative to the major scale), a numeral
// (upper case = major, lower case = minor) and an optional suffix:
//   °  +  7  maj7  6  9  maj9  add9  11  13  sus2  sus4  7sus4  ø7  °7  5
// followed by an optional :beats length (default 4). Examples: "bVII",
// "ii7", "V7sus4:2", "iiø7", "imaj7" (minor-major 7th), "VI7" (A7 in C).
namespace madscaler
{
    struct LibraryProgression
    {
        const char* name;
        int scaleIndex;     // index into getScales(); loading sets this scale
        const char* tags;   // "|"-separated
        const char* chords; // space-separated chord tokens
    };

    struct TagGroup
    {
        const char* title; // "Style", "Mood", "Use"
        std::vector<const char*> tags;
    };

    struct ParsedChord
    {
        int rootOffset = 0;         // semitones above the tonic, 0-11
        std::vector<int> intervals; // semitones above the chord root, root (0) first
        std::string suffix;         // name suffix, e.g. "m7"
        double beats = 4.0;
    };

    const std::vector<LibraryProgression>& getLibrary();
    const std::vector<TagGroup>& getLibraryTagGroups();

    std::vector<std::string> getTags (const LibraryProgression& progression);
    bool hasTag (const LibraryProgression& progression, const std::string& tag);

    // Parses one chord token. Returns false for a token it does not understand.
    bool parseChordToken (const std::string& token, ParsedChord& result);

    // Splits a progression's chord string into tokens.
    std::vector<std::string> getChordTokens (const LibraryProgression& progression);

    // Builds the progression in a key: root-position chords with their roots in
    // the octave starting at baseNote (the MIDI note of the key's tonic).
    std::vector<ProgressionSlot> buildLibraryProgression (const LibraryProgression& progression,
                                                          int keyPitchClass, int baseNote);
} // namespace madscaler
