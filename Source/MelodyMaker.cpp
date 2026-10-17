#include "MelodyMaker.h"
#include "ChordDetector.h"
#include "ChordEngine.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>

namespace madscaler
{
    namespace
    {
        int wrap (int value) { return ((value % 12) + 12) % 12; }

        constexpr int majorPentatonic = 11;
        constexpr int minorPentatonic = 12;
        constexpr int blues = 13;

        struct ChordSpan
        {
            double start = 0.0, end = 0.0;
            std::set<int> pitchClasses;
            int root = 0;
            const ProgressionSlot* slot = nullptr;
        };

        std::vector<ChordSpan> spansFor (const std::vector<ProgressionSlot>& chords)
        {
            std::vector<ChordSpan> spans;
            double position = 0.0;
            for (const auto& chord : chords)
            {
                if (chord.notes.empty() || chord.lengthBeats <= 0.0)
                    continue;

                ChordSpan span;
                span.start = position;
                span.end = position + chord.lengthBeats;
                span.slot = &chord;
                for (auto note : chord.notes)
                    span.pitchClasses.insert (wrap (note));

                const auto detected = detectChord (chord.notes);
                span.root = detected.valid ? detected.rootPitchClass : wrap (chord.notes.front());
                spans.push_back (span);
                position = span.end;
            }
            return spans;
        }

        const ChordSpan& spanAt (const std::vector<ChordSpan>& spans, double beat)
        {
            for (const auto& span : spans)
                if (beat < span.end - 1.0e-9)
                    return span;
            return spans.back();
        }
    } // namespace

    const std::vector<MelodyScale>& getMelodyScales()
    {
        static const std::vector<MelodyScale> scales = []
        {
            std::vector<MelodyScale> result;
            for (const auto& scale : getScales())
                result.push_back ({ scale.name, scale.intervals });
            result.push_back ({ "Major Pentatonic", { 0, 2, 4, 7, 9 } });
            result.push_back ({ "Minor Pentatonic", { 0, 3, 5, 7, 10 } });
            result.push_back ({ "Blues", { 0, 3, 5, 6, 7, 10 } });
            return result;
        }();
        return scales;
    }

    std::vector<ScaleFit> suggestMelodyScales (const std::vector<ProgressionSlot>& chords, size_t maxSuggestions)
    {
        const auto spans = spansFor (chords);
        if (spans.empty())
            return {};

        const auto& scales = getMelodyScales();
        std::vector<ScaleFit> fits;

        for (int key = 0; key < 12; ++key)
        {
            for (int scaleIndex = 0; scaleIndex < (int) scales.size(); ++scaleIndex)
            {
                std::set<int> scalePcs;
                for (auto interval : scales[(size_t) scaleIndex].intervals)
                    scalePcs.insert (wrap (key + interval));

                ScaleFit fit;
                fit.keyPitchClass = key;
                fit.scaleIndex = scaleIndex;

                int rootsOnKey = 0;
                for (const auto& span : spans)
                {
                    for (auto pc : span.pitchClasses)
                        fit.notesOutside += scalePcs.count (pc) == 0 ? 1 : 0;
                    rootsOnKey += span.root == key ? 1 : 0;
                }

                // Fewer clashing notes matters most, then a tonic the chords
                // start or end on, then how common the scale is.
                fit.score = -100 * fit.notesOutside + 8 * rootsOnKey;
                if (spans.front().root == key) fit.score += 30;
                if (spans.back().root == key)  fit.score += 20;

                if (scaleIndex == 0 || scaleIndex == 1)                                 fit.score += 15;
                else if (scaleIndex == majorPentatonic || scaleIndex == minorPentatonic) fit.score += 10;
                else if (scaleIndex == blues)                                           fit.score += 6;
                else if (scaleIndex >= 4 && scaleIndex <= 7)                            fit.score += 5;

                const auto spellingScale = scaleIndex < majorPentatonic ? scaleIndex
                                                                        : (scaleIndex == majorPentatonic ? 0 : 1);
                fit.name = spellNote (key, key, spellingScale) + " " + scales[(size_t) scaleIndex].name;
                fits.push_back (fit);
            }
        }

        std::stable_sort (fits.begin(), fits.end(), [] (const ScaleFit& a, const ScaleFit& b) { return a.score > b.score; });
        if (fits.size() > maxSuggestions)
            fits.resize (maxSuggestions);
        return fits;
    }

    const std::vector<RhythmPattern>& getRhythmPatterns()
    {
        static const std::vector<RhythmPattern> patterns {
            { "Quarters",        1.0, { { 0.0, 1.0 } } },
            { "Eighths",         1.0, { { 0.0, 0.5 }, { 0.5, 0.5 } } },
            { "Sixteenths",      1.0, { { 0.0, 0.25 }, { 0.25, 0.25 }, { 0.5, 0.25 }, { 0.75, 0.25 } } },
            { "Triplets",        1.0, { { 0.0, 1.0 / 3.0 }, { 1.0 / 3.0, 1.0 / 3.0 }, { 2.0 / 3.0, 1.0 / 3.0 } } },
            { "Dotted eighths",  3.0, { { 0.0, 0.75 }, { 0.75, 0.75 }, { 1.5, 0.75 }, { 2.25, 0.75 } } },
            { "Tresillo 3+3+2",  2.0, { { 0.0, 0.75 }, { 0.75, 0.75 }, { 1.5, 0.5 } } },
            { "Gallop",          1.0, { { 0.0, 0.5 }, { 0.5, 0.25 }, { 0.75, 0.25 } } },
            { "Reverse gallop",  1.0, { { 0.0, 0.25 }, { 0.25, 0.25 }, { 0.5, 0.5 } } },
            { "Offbeats",        1.0, { { 0.5, 0.5 } } },
            { "Syncopated pop",  4.0, { { 0.0, 0.75 }, { 0.75, 0.75 }, { 1.5, 1.0 }, { 2.5, 0.5 }, { 3.0, 1.0 } } },
            { "Pop hook",        4.0, { { 0.0, 0.5 }, { 0.5, 0.5 }, { 1.0, 1.0 }, { 2.5, 0.5 }, { 3.0, 1.0 } } },
            { "Long and short",  4.0, { { 0.0, 2.0 }, { 2.0, 1.0 }, { 3.0, 1.0 } } },
            { "Half notes",      4.0, { { 0.0, 2.0 }, { 2.0, 2.0 } } },
            { "Waltz",           3.0, { { 0.0, 1.0 }, { 1.0, 1.0 }, { 2.0, 1.0 } } },
            { "Eighth and two sixteenths", 1.0, { { 0.0, 0.5 }, { 0.5, 0.25 }, { 0.75, 0.25 } } },
            { "Two sixteenths and eighth", 1.0, { { 0.0, 0.25 }, { 0.25, 0.25 }, { 0.5, 0.5 } } },
            { "Shuffle",         1.0, { { 0.0, 2.0 / 3.0 }, { 2.0 / 3.0, 1.0 / 3.0 } } },
            { "Sextuplets",      1.0, { { 0.0, 1.0 / 6.0 }, { 1.0 / 6.0, 1.0 / 6.0 }, { 2.0 / 6.0, 1.0 / 6.0 },
                                        { 3.0 / 6.0, 1.0 / 6.0 }, { 4.0 / 6.0, 1.0 / 6.0 }, { 5.0 / 6.0, 1.0 / 6.0 } } },
            { "Clave 3-2",       8.0, { { 0.0, 1.5 }, { 1.5, 1.5 }, { 3.0, 2.0 }, { 5.0, 1.0 }, { 6.0, 2.0 } } },
            { "Pulse with gaps", 1.0, { { 0.0, 0.25 }, { 0.5, 0.25 }, { 0.75, 0.25 } } },
            { "Groups of 3 (16ths)", 3.0, { { 0.0, 0.75 }, { 0.75, 0.75 }, { 1.5, 0.75 }, { 2.25, 0.75 } } },
        };
        return patterns;
    }

    const std::vector<const char*>& getArpStyleNames()
    {
        static const std::vector<const char*> names { "Up", "Down", "Up and down", "Alberti", "Pedal low", "Random",
                                                      "Down and up", "Up and down, ends twice", "Pedal high",
                                                      "Outside in", "Inside out", "Skip (1 3 2 4)",
                                                      "Steps (1 2 3, 2 3 4)", "Octave pairs", "Random walk" };
        return names;
    }

    const std::vector<const char*>& getArpNotesNames()
    {
        static const std::vector<const char*> names { "Chord notes", "Add 9th", "Add 7th", "Add 6th", "Root and 5th" };
        return names;
    }

    namespace
    {
        // The position in the arpeggio's notes (0 = lowest) for a step of a style.
        int arpIndex (ArpStyle style, int step, int n, int& walk, std::mt19937& rng)
        {
            auto pick = [&rng] (int count) { return std::uniform_int_distribution<int> (0, std::max (0, count - 1)) (rng); };
            if (n <= 1)
                return 0;

            switch (style)
            {
                case ArpStyle::up:   return step % n;
                case ArpStyle::down: return n - 1 - step % n;
                case ArpStyle::upDown:
                {
                    const auto period = 2 * n - 2;
                    const auto phase = step % period;
                    return phase < n ? phase : period - phase;
                }
                case ArpStyle::downUp:
                {
                    const auto period = 2 * n - 2;
                    const auto phase = step % period;
                    return n - 1 - (phase < n ? phase : period - phase);
                }
                case ArpStyle::upDownRepeat:
                {
                    const auto phase = step % (2 * n);
                    return phase < n ? phase : 2 * n - 1 - phase;
                }
                case ArpStyle::alberti:
                {
                    const int shape[] { 0, n - 1, n / 2, n - 1 };
                    return shape[step % 4];
                }
                case ArpStyle::pinky:     return step % 2 == 0 ? 0 : 1 + (step / 2) % (n - 1);
                case ArpStyle::pedalHigh: return step % 2 == 0 ? n - 1 : (step / 2) % (n - 1);
                case ArpStyle::converge:
                {
                    const auto phase = step % n;
                    return phase % 2 == 0 ? phase / 2 : n - 1 - phase / 2;
                }
                case ArpStyle::diverge:
                {
                    // Middle, then alternately above and below it.
                    const auto phase = step % n;
                    const auto middle = (n - 1) / 2;
                    const auto distance = (phase + 1) / 2;
                    const auto index = phase % 2 == 1 ? middle + distance : middle - distance;
                    return std::clamp (index, 0, n - 1);
                }
                case ArpStyle::skip:
                {
                    // 0 2 1 3 2 4 ... within the notes, then again.
                    const auto pairs = std::max (1, n - 2);
                    const auto phase = step % (2 * pairs);
                    return phase / 2 + (phase % 2 == 0 ? 0 : 2);
                }
                case ArpStyle::steps:
                {
                    const auto groups = std::max (1, n - 2);
                    const auto group = (step / 3) % groups;
                    return std::min (n - 1, group + step % 3);
                }
                case ArpStyle::octaves:   return (step / 2) % n; // the octave is added by the caller
                case ArpStyle::random:    return pick (n);
                case ArpStyle::randomWalk:
                {
                    if (step == 0)
                        walk = pick (n);
                    else
                        walk += pick (2) == 0 ? -1 : 1;
                    if (walk < 0) walk = 1;
                    if (walk >= n) walk = n - 2;
                    return walk;
                }
            }
            return 0;
        }

        // Scale notes added to the chord for the arpeggio's colour.
        std::set<int> arpPitchClasses (const std::set<int>& chord, int root, ArpNotes notes, const std::set<int>& scalePcs)
        {
            auto firstInScale = [&scalePcs, root] (std::initializer_list<int> intervals)
            {
                for (auto interval : intervals)
                    if (scalePcs.count (wrap (root + interval)) > 0)
                        return wrap (root + interval);
                return -1;
            };

            auto result = chord;
            int extra = -1;
            switch (notes)
            {
                case ArpNotes::chord:     break;
                case ArpNotes::add9:      extra = firstInScale ({ 2, 1, 3 }); break;
                case ArpNotes::add7:      extra = firstInScale ({ 10, 11, 9 }); break;
                case ArpNotes::add6:      extra = firstInScale ({ 9, 8 }); break;
                case ArpNotes::rootFifth:
                {
                    result = { root };
                    const auto fifth = chord.count (wrap (root + 7)) > 0 ? wrap (root + 7)
                                     : chord.count (wrap (root + 6)) > 0 ? wrap (root + 6)
                                                                          : wrap (root + 7);
                    result.insert (fifth);
                    break;
                }
            }
            if (extra >= 0)
                result.insert (extra);
            return result;
        }
    } // namespace

    std::vector<TimedNote> generateMelody (const std::vector<ProgressionSlot>& chords, const MelodySettings& settings)
    {
        const auto spans = spansFor (chords);
        const auto& patterns = getRhythmPatterns();
        const auto& scales = getMelodyScales();
        if (spans.empty())
            return {};

        const auto& pattern = patterns[(size_t) std::clamp (settings.rhythm, 0, (int) patterns.size() - 1)];
        const auto& scale = scales[(size_t) std::clamp (settings.scaleIndex, 0, (int) scales.size() - 1)];
        const auto low = std::clamp (std::min (settings.lowNote, settings.highNote), 0, 127);
        const auto high = std::clamp (std::max (settings.lowNote, settings.highNote), 0, 127);
        const auto total = spans.back().end;

        std::mt19937 rng (settings.seed);
        auto chance = [&rng] (double probability) { return std::uniform_real_distribution<double> (0.0, 1.0) (rng) < probability; };
        auto pick = [&rng] (int count) { return std::uniform_int_distribution<int> (0, std::max (0, count - 1)) (rng); };

        // Every hit of the rhythm, cut at chord changes so no note hangs over the next chord.
        struct Hit
        {
            double start, length;
        };

        std::vector<Hit> hits;
        for (double bar = 0.0; bar < total - 1.0e-9; bar += pattern.lengthBeats)
        {
            for (const auto& [offset, length] : pattern.hits)
            {
                const auto start = bar + offset;
                if (start >= total - 1.0e-9)
                    break;
                const auto& span = spanAt (spans, start);
                hits.push_back ({ start, std::min (length, span.end - start) });
            }
        }

        std::vector<TimedNote> notes;
        auto velocityFor = [&] (double start, int index, const ChordSpan& span)
        {
            const bool onBeat = std::abs (start - std::round (start)) < 1.0e-6;
            int shape = 0;
            switch (settings.accent)
            {
                case Accent::beats:      shape = onBeat ? 8 : -4; break;
                case Accent::even:       shape = 0; break;
                case Accent::everyThird: shape = index % 3 == 0 ? 12 : -6; break;
                case Accent::rising:
                    shape = (int) std::round (-24.0 + 36.0 * (start - span.start) / std::max (0.001, span.end - span.start));
                    break;
            }
            return std::clamp (settings.velocity + shape + pick (7) - 3, 1, 127);
        };

        const double gates[] { 0.5, settings.mode == MelodyMode::arpeggio ? 0.9 : 0.95, 1.0 };
        const auto gate = gates[std::clamp (settings.gate, 0, 2)];

        // Swing: notes on the off-beat eighth (and the off-beat sixteenths) come
        // later and get shorter, so they still end where they did.
        const double swingAmounts[] { 0.0, 0.12, 0.22, 0.33 };
        const auto swing = swingAmounts[std::clamp (settings.swing, 0, 3)];
        auto applySwing = [swing] (std::vector<TimedNote>& result)
        {
            if (swing <= 0.0)
                return;
            for (auto& note : result)
            {
                const auto fraction = note.startBeat - std::floor (note.startBeat + 1.0e-9);
                double shift = 0.0;
                if (std::abs (fraction - 0.5) < 1.0e-6)
                    shift = swing * 0.5;
                else if (std::abs (fraction - 0.25) < 1.0e-6 || std::abs (fraction - 0.75) < 1.0e-6)
                    shift = swing * 0.25;
                if (shift > 0.0)
                {
                    note.startBeat += shift;
                    note.lengthBeats = std::max (0.03, note.lengthBeats - shift);
                }
            }
        };

        std::set<int> scalePcs;
        for (auto interval : scale.intervals)
            scalePcs.insert (wrap (settings.keyPitchClass + interval));

        if (settings.mode == MelodyMode::arpeggio)
        {
            const ChordSpan* current = nullptr;
            std::vector<int> tones;
            int step = 0, walk = 0;
            const auto octaves = std::clamp (settings.arpOctaves, 1, 4);
            const double variation[] { 0.0, 0.1, 0.2, 0.35 };
            const auto amount = variation[std::clamp (settings.variation, 0, 3)];

            for (const auto& hit : hits)
            {
                const auto& span = spanAt (spans, hit.start);
                const bool chordStart = &span != current;
                if (chordStart)
                {
                    current = &span;
                    if (! settings.keepFlowing)
                        step = 0;

                    // The arpeggio's notes upwards from the bottom of the range,
                    // over the chosen number of octaves.
                    const auto pcs = arpPitchClasses (span.pitchClasses, span.root, settings.arpNotes, scalePcs);
                    const auto count = (size_t) octaves * pcs.size();
                    tones.clear();
                    for (int note = low; note <= 127 && tones.size() < count; ++note)
                        if (pcs.count (wrap (note)) > 0)
                            tones.push_back (note);
                    if (tones.empty())
                        tones.push_back (std::clamp (span.slot->notes.front(), low, high));

                    if (settings.bassNote)
                    {
                        auto bass = tones.front() - 12;
                        while (bass > 0 && wrap (bass) != span.root)
                            --bass;
                        if (bass >= 0)
                            notes.push_back ({ span.start, (span.end - span.start) * 0.98, bass,
                                               std::clamp (settings.velocity - 10, 1, 127) });
                    }
                }

                const auto n = (int) tones.size();
                auto index = std::clamp (arpIndex (settings.arpStyle, step, n, walk, rng), 0, n - 1);
                auto note = tones[(size_t) index];
                if (settings.arpStyle == ArpStyle::octaves && step % 2 == 1 && note + 12 <= 127)
                    note += 12;
                ++step;

                auto length = hit.length;
                const auto velocity = velocityFor (hit.start, step - 1, span);

                if (amount > 0.0 && ! chordStart)
                {
                    if (chance (amount * 0.5))
                        continue; // a rest

                    if (chance (amount * 0.4))
                        note = tones[(size_t) std::clamp (index + (chance (0.5) ? 1 : -1), 0, n - 1)];
                    else if (chance (amount * 0.4) && note + 12 <= 127)
                        note += 12;

                    if (length >= 0.25 - 1.0e-9 && chance (amount * 0.5))
                    {
                        // Repeat the note twice as fast.
                        const auto half = length / 2.0;
                        notes.push_back ({ hit.start, half * gate, note, velocity });
                        notes.push_back ({ hit.start + half, half * gate, note, std::max (1, velocity - 12) });
                        continue;
                    }
                }

                notes.push_back ({ hit.start, length * gate, note, velocity });
            }

            applySwing (notes);
            return notes;
        }

        // Melody: the scale's notes in the range.
        std::vector<int> pool;
        for (int note = low; note <= high; ++note)
            for (auto interval : scale.intervals)
                if (wrap (note - settings.keyPitchClass) == interval)
                    pool.push_back (note);

        if (pool.empty())
            return {};

        auto chordTones = [&] (const ChordSpan& span)
        {
            std::vector<int> result;
            for (auto note : pool)
                if (span.pitchClasses.count (wrap (note)) > 0)
                    result.push_back (note);
            return result.empty() ? pool : result;
        };

        auto nearestIndex = [&] (int note)
        {
            int best = 0;
            for (int i = 1; i < (int) pool.size(); ++i)
                if (std::abs (pool[(size_t) i] - note) < std::abs (pool[(size_t) best] - note))
                    best = i;
            return best;
        };

        // Picks a chord tone near the previous note, a little at random.
        auto chooseChordTone = [&] (const ChordSpan& span, int previous)
        {
            auto candidates = chordTones (span);
            std::stable_sort (candidates.begin(), candidates.end(),
                              [previous] (int a, int b) { return std::abs (a - previous) < std::abs (b - previous); });
            const auto choices = std::min ((int) candidates.size(), 3);
            const auto roll = pick (6);
            return candidates[(size_t) (roll < 3 ? 0 : roll < 5 ? std::min (1, choices - 1) : choices - 1)];
        };

        const auto centre = (low + high) / 2;
        int previous = chooseChordTone (spans.front(), centre);
        int direction = chance (0.5) ? 1 : -1;
        const double restChance = settings.density <= 0 ? 0.45 : settings.density == 1 ? 0.15 : 0.0;

        for (size_t h = 0; h < hits.size(); ++h)
        {
            const auto& hit = hits[h];
            const auto& span = spanAt (spans, hit.start);
            const bool chordStart = std::abs (hit.start - span.start) < 1.0e-6;
            const bool onBeat = std::abs (hit.start - std::round (hit.start)) < 1.0e-6;
            const bool last = h + 1 == hits.size();

            if (! chordStart && ! onBeat && ! last && chance (restChance))
                continue;

            int note = previous;
            auto length = hit.length;

            if (last)
            {
                // End the phrase on the last chord's root, held to the end.
                auto candidates = chordTones (span);
                std::vector<int> roots;
                for (auto candidate : candidates)
                    if (wrap (candidate) == span.root)
                        roots.push_back (candidate);
                if (roots.empty())
                    roots = candidates;
                note = *std::min_element (roots.begin(), roots.end(),
                                          [previous] (int a, int b) { return std::abs (a - previous) < std::abs (b - previous); });
                length = total - hit.start;
            }
            else if (chordStart || onBeat)
            {
                note = chooseChordTone (span, previous + direction * 2);
                if (chance (0.3))
                    direction = -direction;
            }
            else
            {
                // Step along the scale, sometimes skipping a note.
                auto index = nearestIndex (previous) + direction * (chance (0.2) ? 2 : 1);
                if (index < 0 || index >= (int) pool.size())
                {
                    direction = -direction;
                    index = std::clamp (nearestIndex (previous) + direction, 0, (int) pool.size() - 1);
                }
                note = pool[(size_t) index];
                if (chance (0.2))
                    direction = -direction;
            }

            // Drift back towards the middle of the range.
            if (note > centre + 6) direction = -1;
            if (note < centre - 6) direction = 1;

            previous = note;
            notes.push_back ({ hit.start, length * gate, note, velocityFor (hit.start, (int) notes.size(), span) });
        }

        applySwing (notes);
        return notes;
    }
} // namespace madscaler
