#include "Voicing.h"

#include <algorithm>
#include <climits>
#include <cstdlib>

namespace madscaler
{
    namespace
    {
        std::vector<int> invert (std::vector<int> notes, int inversion)
        {
            if (notes.empty())
                return notes;

            inversion = std::clamp (inversion, 0, (int) notes.size() - 1);
            for (int i = 0; i < inversion; ++i)
            {
                notes.front() += 12;
                std::rotate (notes.begin(), notes.begin() + 1, notes.end());
            }
            return notes;
        }

        std::vector<int> applyStyle (std::vector<int> notes, VoicingStyle style)
        {
            std::sort (notes.begin(), notes.end());
            const auto size = (int) notes.size();

            switch (style)
            {
                case VoicingStyle::close:
                    break;

                case VoicingStyle::open:
                    for (int i = 1; i < size; i += 2)
                        notes[(size_t) i] += 12;
                    break;

                case VoicingStyle::drop2:
                    if (size >= 3)
                        notes[(size_t) (size - 2)] -= 12;
                    break;

                case VoicingStyle::drop3:
                    if (size >= 4)
                        notes[(size_t) (size - 3)] -= 12;
                    else if (size >= 3)
                        notes[(size_t) (size - 2)] -= 12;
                    break;
            }

            std::sort (notes.begin(), notes.end());
            return notes;
        }

        std::vector<int> finish (std::vector<int> notes, int rootPitchClass, bool addBass)
        {
            if (addBass && ! notes.empty())
            {
                // The root in the octave below the lowest note of the voicing.
                int bass = notes.front() - 1;
                while (((bass % 12) + 12) % 12 != rootPitchClass)
                    --bass;
                bass -= bass > notes.front() - 12 ? 12 : 0;
                notes.insert (notes.begin(), bass);
            }

            for (auto& note : notes)
            {
                while (note < 0)   note += 12;
                while (note > 127) note -= 12;
            }

            std::sort (notes.begin(), notes.end());
            notes.erase (std::unique (notes.begin(), notes.end()), notes.end());
            return notes;
        }
    } // namespace

    int voiceLeadingDistance (const std::vector<int>& from, const std::vector<int>& to)
    {
        if (from.empty() || to.empty())
            return 0;

        int total = 0;
        for (auto note : to)
        {
            int nearest = INT_MAX;
            for (auto previous : from)
                nearest = std::min (nearest, std::abs (note - previous));
            total += nearest;
        }

        // Also count how far the old notes must travel, so that collapsing onto
        // one shared note does not look free.
        for (auto previous : from)
        {
            int nearest = INT_MAX;
            for (auto note : to)
                nearest = std::min (nearest, std::abs (note - previous));
            total += nearest;
        }

        return total;
    }

    std::vector<int> voiceChord (const std::vector<int>& rootPositionNotes,
                                 const VoicingSettings& settings,
                                 const std::vector<int>& previousNotes)
    {
        if (rootPositionNotes.empty())
            return {};

        const int rootPitchClass = ((rootPositionNotes.front() % 12) + 12) % 12;

        if (! settings.voiceLeading || previousNotes.empty())
            return finish (applyStyle (invert (rootPositionNotes, settings.inversion), settings.style),
                           rootPitchClass, settings.addBass);

        // Try every inversion an octave down, in place and an octave up, and keep
        // the one that moves least from the previous chord. The bass note, if any,
        // is left out of the comparison: it jumps with the root anyway.
        std::vector<int> previousUpper = previousNotes;
        if (settings.addBass && previousUpper.size() > 1)
            previousUpper.erase (previousUpper.begin());

        std::vector<int> best;
        int bestDistance = INT_MAX;
        const auto anchor = rootPositionNotes.front();

        for (int inversion = 0; inversion < (int) rootPositionNotes.size(); ++inversion)
        {
            for (int shift : { 0, -12, 12 })
            {
                auto candidate = applyStyle (invert (rootPositionNotes, inversion), settings.style);
                for (auto& note : candidate)
                    note += shift;

                // Stay near the chosen octave so chords do not drift away over time.
                if (std::abs (candidate.front() - anchor) > 9)
                    continue;

                const auto distance = voiceLeadingDistance (previousUpper, candidate);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    best = candidate;
                }
            }
        }

        if (best.empty())
            best = applyStyle (invert (rootPositionNotes, settings.inversion), settings.style);

        return finish (best, rootPitchClass, settings.addBass);
    }
} // namespace madscaler
