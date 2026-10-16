#include "ChordEngine.h"

#include <algorithm>

namespace madscaler
{
    const std::vector<Scale>& getScales()
    {
        static const std::vector<Scale> scales {
            { "Major",           { 0, 2, 4, 5, 7, 9, 11 } },
            { "Natural Minor",   { 0, 2, 3, 5, 7, 8, 10 } },
            { "Harmonic Minor",  { 0, 2, 3, 5, 7, 8, 11 } },
            { "Melodic Minor",   { 0, 2, 3, 5, 7, 9, 11 } },
            { "Dorian",          { 0, 2, 3, 5, 7, 9, 10 } },
            { "Phrygian",        { 0, 1, 3, 5, 7, 8, 10 } },
            { "Lydian",          { 0, 2, 4, 6, 7, 9, 11 } },
            { "Mixolydian",      { 0, 2, 4, 5, 7, 9, 10 } },
            { "Locrian",         { 0, 1, 3, 5, 6, 8, 10 } },
            { "Harmonic Major",  { 0, 2, 4, 5, 7, 8, 11 } },
            { "Phrygian Dominant", { 0, 1, 4, 5, 7, 8, 10 } },
        };
        return scales;
    }

    const std::array<const char*, 12>& getNoteNames()
    {
        static const std::array<const char*, 12> names { "C", "C#", "D", "D#", "E", "F",
                                                         "F#", "G", "G#", "A", "A#", "B" };
        return names;
    }

    static int wrapPitchClass (int value)
    {
        return ((value % 12) + 12) % 12;
    }

    static int clampScaleIndex (int scaleIndex)
    {
        return std::clamp (scaleIndex, 0, (int) getScales().size() - 1);
    }

    std::string getNoteName (int pitchClass)
    {
        return getNoteNames()[(size_t) wrapPitchClass (pitchClass)];
    }

    std::vector<int> getScalePitchClasses (int keyPitchClass, int scaleIndex)
    {
        std::vector<int> result;
        for (auto interval : getScales()[(size_t) clampScaleIndex (scaleIndex)].intervals)
            result.push_back (wrapPitchClass (keyPitchClass + interval));
        return result;
    }

    bool isInScale (int midiNote, int keyPitchClass, int scaleIndex)
    {
        const auto pcs = getScalePitchClasses (keyPitchClass, scaleIndex);
        return std::find (pcs.begin(), pcs.end(), wrapPitchClass (midiNote)) != pcs.end();
    }

    ChordQuality identifyQuality (const std::vector<int>& intervalsAboveRoot)
    {
        using Q = ChordQuality;

        struct Shape
        {
            std::vector<int> intervals;
            Q quality;
        };

        static const std::vector<Shape> shapes {
            { { 4, 7 }, Q::major },
            { { 3, 7 }, Q::minor },
            { { 3, 6 }, Q::diminished },
            { { 4, 8 }, Q::augmented },
            { { 4, 7, 11 }, Q::major7 },
            { { 4, 7, 10 }, Q::dominant7 },
            { { 3, 7, 10 }, Q::minor7 },
            { { 3, 6, 10 }, Q::halfDiminished7 },
            { { 3, 6, 9 }, Q::diminished7 },
            { { 3, 7, 11 }, Q::minorMajor7 },
            { { 4, 8, 11 }, Q::augmentedMajor7 },
        };

        for (const auto& shape : shapes)
            if (shape.intervals == intervalsAboveRoot)
                return shape.quality;

        return Q::unknown;
    }

    std::string getQualitySuffix (ChordQuality quality)
    {
        switch (quality)
        {
            case ChordQuality::major:           return "";
            case ChordQuality::minor:           return "m";
            case ChordQuality::diminished:      return "dim";
            case ChordQuality::augmented:       return "aug";
            case ChordQuality::major7:          return "maj7";
            case ChordQuality::dominant7:       return "7";
            case ChordQuality::minor7:          return "m7";
            case ChordQuality::halfDiminished7: return "m7b5";
            case ChordQuality::diminished7:     return "dim7";
            case ChordQuality::minorMajor7:     return "m(maj7)";
            case ChordQuality::augmentedMajor7: return "aug(maj7)";
            case ChordQuality::unknown:         break;
        }
        return "?";
    }

    std::string getRomanNumeral (int degree, ChordQuality quality)
    {
        static const std::array<const char*, 7> upper { "I", "II", "III", "IV", "V", "VI", "VII" };
        static const std::array<const char*, 7> lower { "i", "ii", "iii", "iv", "v", "vi", "vii" };

        const auto index = (size_t) std::clamp (degree, 0, 6);

        switch (quality)
        {
            case ChordQuality::major:           return upper[index];
            case ChordQuality::augmented:       return std::string (upper[index]) + "+";
            case ChordQuality::minor:           return lower[index];
            case ChordQuality::diminished:      return std::string (lower[index]) + "\xC2\xB0"; // degree sign
            case ChordQuality::major7:          return std::string (upper[index]) + "maj7";
            case ChordQuality::dominant7:       return std::string (upper[index]) + "7";
            case ChordQuality::augmentedMajor7: return std::string (upper[index]) + "+maj7";
            case ChordQuality::minor7:          return std::string (lower[index]) + "7";
            case ChordQuality::minorMajor7:     return std::string (lower[index]) + "(maj7)";
            case ChordQuality::halfDiminished7: return std::string (lower[index]) + "\xC3\xB8" "7"; // o-slash
            case ChordQuality::diminished7:     return std::string (lower[index]) + "\xC2\xB0" "7";
            case ChordQuality::unknown:         break;
        }
        return upper[index];
    }

    Chord buildDiatonicChord (int keyPitchClass, int scaleIndex, int degree, ChordSize size, int baseNote)
    {
        const auto& intervals = getScales()[(size_t) clampScaleIndex (scaleIndex)].intervals;
        const auto scaleLength = (int) intervals.size();
        degree = std::clamp (degree, 0, scaleLength - 1);

        // Semitones above the tonic for a scale step that may run past the octave.
        auto semitonesForStep = [&] (int step)
        {
            return intervals[(size_t) (step % scaleLength)] + 12 * (step / scaleLength);
        };

        const int noteCount = size == ChordSize::seventh ? 4 : 3;
        const int rootOffset = semitonesForStep (degree);

        Chord chord;
        chord.degree = degree;
        chord.rootPitchClass = wrapPitchClass (keyPitchClass + rootOffset);

        std::vector<int> intervalsAboveRoot;

        for (int i = 0; i < noteCount; ++i)
        {
            const int offset = semitonesForStep (degree + 2 * i);
            chord.notes.push_back (std::clamp (baseNote + offset, 0, 127));

            if (i > 0)
                intervalsAboveRoot.push_back (offset - rootOffset);
        }

        chord.quality = identifyQuality (intervalsAboveRoot);
        chord.name = getNoteName (chord.rootPitchClass) + getQualitySuffix (chord.quality);
        chord.roman = getRomanNumeral (degree, chord.quality);
        return chord;
    }
} // namespace madscaler
