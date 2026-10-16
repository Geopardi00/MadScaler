#include "ChordColours.h"
#include "ChordEngine.h"

#include <algorithm>
#include <array>
#include <utility>

namespace madscaler
{
    namespace
    {
        int wrap (int value) { return ((value % 12) + 12) % 12; }

        std::vector<int> pitchClassSet (const std::vector<int>& notes)
        {
            std::vector<int> pcs;
            for (auto note : notes)
                pcs.push_back (wrap (note));
            std::sort (pcs.begin(), pcs.end());
            pcs.erase (std::unique (pcs.begin(), pcs.end()), pcs.end());
            return pcs;
        }

        std::vector<int> notesFrom (int rootNote, const std::vector<int>& intervals)
        {
            std::vector<int> notes;
            for (auto interval : intervals)
                notes.push_back (std::clamp (rootNote + interval, 0, 127));
            std::sort (notes.begin(), notes.end());
            return notes;
        }

        std::string triadSuffix (int third, int fifth)
        {
            if (third == 4 && fifth == 7) return "";
            if (third == 3 && fifth == 7) return "m";
            if (third == 3 && fifth == 6) return "dim";
            if (third == 4 && fifth == 8) return "aug";
            return "?";
        }

        // Names an extended chord from its 7th-chord suffix. Extensions are listed
        // as {number, semitones away from the natural extension}: natural ones
        // raise the number (m7 -> m9), altered ones are added in brackets.
        std::string extendedSuffix (const std::string& seventhSuffix, const std::vector<std::pair<int, int>>& extensions)
        {
            const auto sevenAt = seventhSuffix.find ('7');
            if (sevenAt == std::string::npos)
                return {};

            int top = 7;
            bool natural = true;
            std::vector<std::string> extras;

            for (const auto& [number, offset] : extensions)
            {
                if (offset == 0 && natural)
                {
                    top = number;
                    continue;
                }

                natural = false;
                extras.push_back ((offset < 0 ? "b" : offset > 0 ? "#" : "") + std::to_string (number));
            }

            auto suffix = seventhSuffix;
            suffix.replace (sevenAt, 1, std::to_string (top));

            if (! extras.empty())
            {
                suffix += "(";
                for (size_t i = 0; i < extras.size(); ++i)
                    suffix += (i > 0 ? "," : "") + extras[i];
                suffix += ")";
            }

            return suffix;
        }

        struct ParallelScale
        {
            int scaleIndex;
            const char* shortName;
        };

        // Scales to borrow from, most common first. Indexes into getScales().
        const std::array<ParallelScale, 8> parallelScales { {
            { 1, "Minor" }, { 0, "Major" }, { 4, "Dorian" }, { 7, "Mixolydian" },
            { 6, "Lydian" }, { 5, "Phrygian" }, { 2, "Harm. minor" }, { 3, "Mel. minor" },
        } };

        // Roman numeral with b or # when the root sits off the major scale.
        std::string borrowedRoman (int keyPitchClass, int degree, const Chord& chord)
        {
            static const std::array<int, 7> major { 0, 2, 4, 5, 7, 9, 11 };
            const auto offset = wrap (chord.rootPitchClass - keyPitchClass) - major[(size_t) degree];
            const std::string accidental = offset < 0 ? "b" : offset > 0 ? "#" : "";
            return accidental + chord.roman;
        }
    } // namespace

    std::vector<ColourGroup> getChordColours (int keyPitchClass, int scaleIndex, int degree,
                                              bool seventhChords, int baseNote)
    {
        const auto& scales = getScales();
        scaleIndex = std::clamp (scaleIndex, 0, (int) scales.size() - 1);
        const auto& intervals = scales[(size_t) scaleIndex].intervals;
        const auto length = (int) intervals.size();
        degree = std::clamp (degree, 0, length - 1);

        auto semitones = [&] (int step) { return intervals[(size_t) (step % length)] + 12 * (step / length); };

        const int root = semitones (degree);
        auto above = [&] (int steps) { return semitones (degree + steps) - root; };

        const int second = above (1), third = above (2), fourth = above (3), fifth = above (4);
        const int sixth = above (5), seventh = above (6), ninth = above (8), eleventh = above (10), thirteenth = above (12);

        const auto rootPitchClass = wrap (keyPitchClass + root);
        const auto rootNote = baseNote + root;
        const auto triad = triadSuffix (third, fifth);
        const auto seventhSuffix = getQualitySuffix (identifyQuality ({ third, fifth, seventh }));
        const bool majorOrMinor = triad == "" || triad == "m";

        std::vector<ColourGroup> groups;
        std::vector<std::vector<int>> seen;

        // Chord roots are spelled for the key; secondary chords for the chord they lead to.
        int spellingKey = keyPitchClass, spellingScale = scaleIndex;
        auto rootName = [&] (int pitchClass) { return spellNote (pitchClass, spellingKey, spellingScale); };

        auto add = [&] (ColourGroup& group, const std::string& suffix, const std::string& label,
                        int chordRoot, const std::vector<int>& notes)
        {
            const auto pcs = pitchClassSet (notes);
            if (std::find (seen.begin(), seen.end(), pcs) != seen.end())
                return;

            seen.push_back (pcs);
            group.chords.push_back ({ rootName (chordRoot) + suffix, label, chordRoot, notes });
        };

        // The plain diatonic chord counts as seen, so borrowed chords that match it are skipped.
        seen.push_back (pitchClassSet (buildDiatonicChord (keyPitchClass, scaleIndex, degree,
                                                           seventhChords ? ChordSize::seventh : ChordSize::triad,
                                                           baseNote).notes));

        ColourGroup extensions { "Extensions", {} };

        if (majorOrMinor && sixth == 9)
            add (extensions, triad + "6", "6", rootPitchClass, notesFrom (rootNote, { 0, third, fifth, sixth }));

        if (seventhSuffix != "?")
            add (extensions, seventhSuffix, "7", rootPitchClass, notesFrom (rootNote, { 0, third, fifth, seventh }));

        const auto ninthSuffix = extendedSuffix (seventhSuffix, { { 9, ninth - 14 } });
        if (! ninthSuffix.empty() && ninth - 14 >= -1)
            add (extensions, ninthSuffix, "9", rootPitchClass, notesFrom (rootNote, { 0, third, fifth, seventh, ninth }));

        if (majorOrMinor && ninth == 14)
            add (extensions, triad + "add9", "add9", rootPitchClass, notesFrom (rootNote, { 0, third, fifth, ninth }));

        // A natural 11th over a major 3rd clashes, so major chords only get a #11.
        const bool usableEleventh = eleventh - third != 13 && eleventh - 17 >= 0;
        const auto eleventhSuffix = extendedSuffix (seventhSuffix, { { 9, ninth - 14 }, { 11, eleventh - 17 } });
        if (! eleventhSuffix.empty() && usableEleventh)
            add (extensions, eleventhSuffix, "11", rootPitchClass,
                 notesFrom (rootNote, { 0, third, fifth, seventh, ninth, eleventh }));

        // 13th chords leave out the 11th, as usually played.
        const auto thirteenthSuffix = extendedSuffix (seventhSuffix, { { 9, ninth - 14 }, { 13, thirteenth - 21 } });
        if (! thirteenthSuffix.empty() && thirteenth - 21 >= -1)
            add (extensions, thirteenthSuffix, "13", rootPitchClass,
                 notesFrom (rootNote, { 0, third, fifth, seventh, ninth, thirteenth }));

        if (second == 2 && fifth == 7)
            add (extensions, "sus2", "sus2", rootPitchClass, notesFrom (rootNote, { 0, 2, 7 }));

        if (fourth == 5 && fifth == 7)
            add (extensions, "sus4", "sus4", rootPitchClass, notesFrom (rootNote, { 0, 5, 7 }));

        groups.push_back (std::move (extensions));

        ColourGroup borrowed { "Borrowed", {} };

        for (const auto& parallel : parallelScales)
        {
            if (parallel.scaleIndex == scaleIndex)
                continue;

            const auto chord = buildDiatonicChord (keyPitchClass, parallel.scaleIndex, degree,
                                                   seventhChords ? ChordSize::seventh : ChordSize::triad, baseNote);
            if (chord.quality == ChordQuality::unknown)
                continue;

            const auto suffix = getQualitySuffix (chord.quality);
            add (borrowed, suffix, borrowedRoman (keyPitchClass, degree, chord) + " " + parallel.shortName,
                 chord.rootPitchClass, chord.notes);
        }

        groups.push_back (std::move (borrowed));

        ColourGroup secondary { "Secondary", {} };

        if (triad == "" || triad == "m" || triad == "aug")
        {
            const auto targetQuality = identifyQuality ({ third, fifth });
            const auto target = getRomanNumeral (degree, targetQuality);

            // Spelled as in the major key of the target: A7 and C#dim7 lead to Dm.
            spellingKey = rootPitchClass;
            spellingScale = 0;

            // Roots sit in the octave above the tonic, like the diatonic chords.
            auto rootNoteFor = [&] (int pitchClass) { return baseNote + wrap (pitchClass - keyPitchClass); };

            const auto dominant = wrap (rootPitchClass + 7);
            add (secondary, "7", "V7/" + target, dominant, notesFrom (rootNoteFor (dominant), { 0, 4, 7, 10 }));

            const auto leading = wrap (rootPitchClass + 11);
            add (secondary, "dim7", "vii\xC2\xB0" "7/" + target, leading, notesFrom (rootNoteFor (leading), { 0, 3, 6, 9 }));

            const auto tritoneSub = wrap (rootPitchClass + 1);
            add (secondary, "7", "subV7/" + target, tritoneSub, notesFrom (rootNoteFor (tritoneSub), { 0, 4, 7, 10 }));
        }

        groups.push_back (std::move (secondary));
        return groups;
    }

    std::vector<int> suggestNextDegrees (int scaleIndex, int degree)
    {
        // Common moves in major-type and minor-type keys, as 0-based degrees.
        static const std::array<std::vector<int>, 7> majorMoves { {
            { 3, 4, 5 }, // I   -> IV, V, vi
            { 4, 6, 0 }, // ii  -> V, vii, I
            { 5, 3 },    // iii -> vi, IV
            { 4, 0, 1 }, // IV  -> V, I, ii
            { 0, 5, 3 }, // V   -> I, vi, IV
            { 3, 1, 4 }, // vi  -> IV, ii, V
            { 0, 2 },    // vii -> I, iii
        } };

        static const std::array<std::vector<int>, 7> minorMoves { {
            { 5, 3, 6 }, // i   -> VI, iv, VII
            { 4, 0 },    // ii  -> v/V, i
            { 5, 3 },    // III -> VI, iv
            { 4, 0, 6 }, // iv  -> v/V, i, VII
            { 0, 5 },    // v   -> i, VI
            { 6, 3, 2 }, // VI  -> VII, iv, III
            { 2, 0 },    // VII -> III, i
        } };

        const auto& scales = getScales();
        scaleIndex = std::clamp (scaleIndex, 0, (int) scales.size() - 1);
        const bool minor = scales[(size_t) scaleIndex].intervals[2] == 3;
        return (minor ? minorMoves : majorMoves)[(size_t) std::clamp (degree, 0, 6)];
    }

    int circlePitchClass (int position, bool minor)
    {
        return wrap (position * 7 + (minor ? 9 : 0));
    }

    ColourChord buildCircleChord (int position, bool minor, int baseNote)
    {
        const auto root = circlePitchClass (position, minor);
        ColourChord chord;
        chord.rootPitchClass = root;
        // Flat side of the circle (Db to F, and their relative minors) uses flats.
        chord.name = getNoteName (root, ((position % 12) + 12) % 12 >= 7) + (minor ? "m" : "");
        chord.label = minor ? "minor" : "major";
        chord.notes = notesFrom (baseNote + root, { 0, minor ? 3 : 4, 7 });
        return chord;
    }

    bool isChordInScale (const std::vector<int>& notes, int keyPitchClass, int scaleIndex)
    {
        return std::all_of (notes.begin(), notes.end(),
                            [&] (int note) { return isInScale (note, keyPitchClass, scaleIndex); });
    }
} // namespace madscaler
