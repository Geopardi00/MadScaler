#include "ChordDetector.h"
#include "ChordEngine.h"

#include <algorithm>
#include <array>

namespace madscaler
{
    namespace
    {
        struct ChordShape
        {
            const char* suffix;
            std::vector<int> intervals; // pitch classes above the root, root (0) included
            int priority;               // lower wins when two readings fit equally well
        };

        // Common chord shapes. When a chord can be read more than one way (C6 vs
        // Am7/C) the lower priority and a root in the bass decide.
        const std::vector<ChordShape>& getShapes()
        {
            static const std::vector<ChordShape> shapes {
                { "",        { 0, 4, 7 },          0 },
                { "m",       { 0, 3, 7 },          0 },
                { "dim",     { 0, 3, 6 },          1 },
                { "aug",     { 0, 4, 8 },          2 },
                { "sus4",    { 0, 5, 7 },          2 },
                { "sus2",    { 0, 2, 7 },          3 },
                { "5",       { 0, 7 },             4 },
                { "maj7",    { 0, 4, 7, 11 },      0 },
                { "7",       { 0, 4, 7, 10 },      0 },
                { "m7",      { 0, 3, 7, 10 },      0 },
                { "m7b5",    { 0, 3, 6, 10 },      1 },
                { "dim7",    { 0, 3, 6, 9 },       1 },
                { "m(maj7)", { 0, 3, 7, 11 },      2 },
                { "6",       { 0, 4, 7, 9 },       2 },
                { "m6",      { 0, 3, 7, 9 },       2 },
                { "7sus4",   { 0, 5, 7, 10 },      2 },
                { "add9",    { 0, 2, 4, 7 },       2 },
                { "madd9",   { 0, 2, 3, 7 },       2 },
                { "aug7",    { 0, 4, 8, 10 },      3 },
                { "maj9",    { 0, 2, 4, 7, 11 },   1 },
                { "9",       { 0, 2, 4, 7, 10 },   1 },
                { "m9",      { 0, 2, 3, 7, 10 },   1 },
                { "7b9",     { 0, 1, 4, 7, 10 },   2 },
                { "7#9",     { 0, 3, 4, 7, 10 },   2 },
                { "11",      { 0, 2, 5, 7, 10 },   3 },
                { "m11",     { 0, 2, 3, 5, 7, 10 }, 2 },
                { "13",      { 0, 2, 4, 7, 9, 10 }, 3 },
                // Shapes without a fifth, common in voicings.
                { "7",       { 0, 4, 10 },         5 },
                { "maj7",    { 0, 4, 11 },         5 },
                { "m7",      { 0, 3, 10 },         5 },
            };
            return shapes;
        }

        int wrap (int value) { return ((value % 12) + 12) % 12; }
    } // namespace

    DetectedChord detectChord (const std::vector<int>& midiNotes)
    {
        return detectChord (midiNotes, -1, 0);
    }

    DetectedChord detectChord (const std::vector<int>& midiNotes, int keyPitchClass, int scaleIndex)
    {
        auto name = [&] (int pitchClass)
        {
            return keyPitchClass < 0 ? getNoteName (pitchClass) : spellNote (pitchClass, keyPitchClass, scaleIndex);
        };

        DetectedChord result;
        if (midiNotes.empty())
            return result;

        result.notes = midiNotes;
        std::sort (result.notes.begin(), result.notes.end());

        std::array<bool, 12> present {};
        for (auto note : result.notes)
            present[(size_t) wrap (note)] = true;

        const auto numPitchClasses = (int) std::count (present.begin(), present.end(), true);
        if (numPitchClasses < 2)
            return result;

        const int bass = wrap (result.notes.front());

        int bestScore = -1;

        for (int root = 0; root < 12; ++root)
        {
            if (! present[(size_t) root])
                continue;

            for (const auto& shape : getShapes())
            {
                if ((int) shape.intervals.size() != numPitchClasses)
                    continue;

                const bool matches = std::all_of (shape.intervals.begin(), shape.intervals.end(),
                                                  [&] (int interval) { return present[(size_t) wrap (root + interval)]; });
                if (! matches)
                    continue;

                // Higher is better: a root in the bass matters most, then the shape's priority.
                const int score = (root == bass ? 100 : 0) + (10 - shape.priority);

                if (score > bestScore)
                {
                    bestScore = score;
                    result.valid = true;
                    result.rootPitchClass = root;
                    result.bassPitchClass = bass;
                    result.name = name (root) + shape.suffix;
                    if (root != bass)
                        result.name += "/" + name (bass);
                }
            }
        }

        return result;
    }

    std::vector<KeySuggestion> suggestKeys (const std::vector<int>& pitchClassWeights,
                                            const std::vector<int>& chordRoots,
                                            size_t maxSuggestions)
    {
        std::vector<KeySuggestion> suggestions;

        int totalWeight = 0;
        for (size_t pc = 0; pc < 12 && pc < pitchClassWeights.size(); ++pc)
            totalWeight += pitchClassWeights[pc];

        if (totalWeight == 0)
            return suggestions;

        struct Candidate
        {
            KeySuggestion suggestion;
            int score;
        };

        std::vector<Candidate> candidates;
        const auto& scales = getScales();

        for (int scale = 0; scale < (int) scales.size(); ++scale)
        {
            for (int key = 0; key < 12; ++key)
            {
                int inside = 0, outsideWeight = 0, outsideCount = 0;

                for (int pc = 0; pc < 12 && pc < (int) pitchClassWeights.size(); ++pc)
                {
                    const auto weight = pitchClassWeights[(size_t) pc];
                    if (weight == 0)
                        continue;

                    if (isInScale (pc, key, scale))
                        inside += weight;
                    else
                    {
                        outsideWeight += weight;
                        ++outsideCount;
                    }
                }

                // Fit dominates. Tonic chords at the start and end of a progression,
                // and the plain major and minor scales, break ties.
                int score = inside * 100 - outsideWeight * 300;

                if (! chordRoots.empty())
                {
                    if (chordRoots.front() == key) score += 40;
                    if (chordRoots.back() == key)  score += 30;
                    score += 10 * (int) std::count (chordRoots.begin(), chordRoots.end(), key);
                }

                if (scale == 0 || scale == 1)
                    score += 25;

                Candidate candidate;
                candidate.suggestion.keyPitchClass = key;
                candidate.suggestion.scaleIndex = scale;
                candidate.suggestion.notesOutside = outsideCount;
                candidate.suggestion.name = spellNote (key, key, scale) + " " + scales[(size_t) scale].name;
                candidate.score = score;
                candidates.push_back (candidate);
            }
        }

        std::stable_sort (candidates.begin(), candidates.end(),
                          [] (const Candidate& a, const Candidate& b) { return a.score > b.score; });

        for (size_t i = 0; i < candidates.size() && suggestions.size() < maxSuggestions; ++i)
            suggestions.push_back (candidates[i].suggestion);

        return suggestions;
    }
} // namespace madscaler
