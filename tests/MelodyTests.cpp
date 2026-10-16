#include "MelodyMaker.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <string>

using namespace madscaler;

static int failures = 0;

static void expect (bool condition, const std::string& what)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

static bool inScale (int note, int key, const MelodyScale& scale)
{
    const auto pc = ((note - key) % 12 + 12) % 12;
    return std::find (scale.intervals.begin(), scale.intervals.end(), pc) != scale.intervals.end();
}

int main()
{
    // C Am F G, four beats each.
    const std::vector<ProgressionSlot> pop {
        { "C",  { 60, 64, 67 }, 4.0 },
        { "Am", { 57, 60, 64 }, 4.0 },
        { "F",  { 53, 57, 60 }, 4.0 },
        { "G",  { 55, 59, 62 }, 4.0 },
    };

    const auto fits = suggestMelodyScales (pop, 6);
    expect (fits.size() == 6, "six suggestions");
    expect (! fits.empty() && fits.front().name == "C Major" && fits.front().notesOutside == 0,
            "C Major fits C Am F G best: " + (fits.empty() ? std::string() : fits.front().name));
    expect (std::any_of (fits.begin(), fits.end(), [] (const ScaleFit& f) { return f.name == "A Natural Minor"; }),
            "A minor is suggested too");

    const std::vector<ProgressionSlot> mixo { { "C", { 60, 64, 67 }, 4.0 }, { "Bb", { 58, 62, 65 }, 4.0 },
                                             { "F", { 53, 57, 60 }, 4.0 }, { "C", { 60, 64, 67 }, 4.0 } };
    const auto mixoFits = suggestMelodyScales (mixo, 3);
    expect (! mixoFits.empty() && mixoFits.front().name == "C Mixolydian", "C Bb F C suggests C Mixolydian: "
            + (mixoFits.empty() ? std::string() : mixoFits.front().name));

    const auto& scales = getMelodyScales();
    expect (scales.size() == 14 && std::string (scales[12].name) == "Minor Pentatonic", "melody scales");

    // Melodies: in the scale, in the range, chord tones on chord changes, repeatable by seed.
    for (int rhythm = 0; rhythm < (int) getRhythmPatterns().size(); ++rhythm)
    {
        for (uint32_t seed = 1; seed <= 5; ++seed)
        {
            MelodySettings settings;
            settings.rhythm = rhythm;
            settings.seed = seed;
            settings.density = (int) seed % 3;
            const auto melody = generateMelody (pop, settings);
            const std::string tag = std::string (getRhythmPatterns()[(size_t) rhythm].name) + " seed " + std::to_string (seed);

            expect (! melody.empty(), "melody has notes: " + tag);
            double lastStart = -1.0;
            for (const auto& note : melody)
            {
                expect (note.note >= 60 && note.note <= 79, "in range: " + tag);
                expect (inScale (note.note, 0, scales[0]), "in C major: " + tag);
                expect (note.startBeat > lastStart && note.startBeat < 16.0, "ordered and inside: " + tag);
                expect (note.startBeat + note.lengthBeats <= 16.0 + 1.0e-9, "ends in time: " + tag);
                lastStart = note.startBeat;

                if (std::fmod (note.startBeat, 4.0) < 1.0e-9)
                {
                    static const std::set<int> chordPcs[] { { 0, 4, 7 }, { 9, 0, 4 }, { 5, 9, 0 }, { 7, 11, 2 } };
                    const auto& pcs = chordPcs[(int) (note.startBeat / 4.0)];
                    expect (pcs.count (note.note % 12) == 1, "chord tone on the chord change: " + tag);
                }
            }

            if (! melody.empty())
                expect (melody.back().note % 12 == 7, "ends on the root of G: " + tag);

            const auto again = generateMelody (pop, settings);
            expect (again.size() == melody.size() && std::equal (again.begin(), again.end(), melody.begin(),
                        [] (const TimedNote& a, const TimedNote& b) { return a.note == b.note && a.startBeat == b.startBeat; }),
                    "same seed, same melody: " + tag);
        }
    }

    MelodySettings eighths;
    eighths.rhythm = 1;
    eighths.density = 2;
    const auto a = generateMelody (pop, eighths);
    eighths.seed = 2;
    const auto b = generateMelody (pop, eighths);
    bool differs = a.size() != b.size();
    for (size_t i = 0; ! differs && i < a.size(); ++i)
        differs = a[i].note != b[i].note;
    expect (differs, "a new seed gives a new melody");
    expect (a.size() == 32, "busy eighths fill every eighth: " + std::to_string (a.size()));

    // Arpeggios: chord notes only, restarting on each chord.
    MelodySettings arp;
    arp.mode = MelodyMode::arpeggio;
    arp.rhythm = 2; // sixteenths
    arp.lowNote = 60;
    arp.highNote = 84;
    const auto up = generateMelody (pop, arp);
    expect (up.size() == 64, "sixteenth arp over 16 beats: " + std::to_string (up.size()));
    if (up.size() == 64)
    {
        expect (up[0].note == 60 && up[1].note == 64 && up[2].note == 67 && up[3].note == 72, "C arp goes up");
        expect (up[16].note == 60 && up[17].note == 64 && up[18].note == 69, "Am arp restarts in range: "
                + std::to_string (up[16].note) + " " + std::to_string (up[17].note) + " " + std::to_string (up[18].note));
        for (size_t i = 0; i < 16; ++i)
            expect (std::set<int> { 0, 4, 7 }.count (up[i].note % 12) == 1, "C arp uses C chord notes");
    }

    arp.arpStyle = ArpStyle::alberti;
    const auto alberti = generateMelody (pop, arp);
    if (alberti.size() >= 4)
        expect (alberti[0].note < alberti[2].note && alberti[2].note < alberti[1].note && alberti[1].note == alberti[3].note,
                "alberti is low, high, middle, high");

    arp.arpStyle = ArpStyle::pinky;
    const auto pinky = generateMelody (pop, arp);
    if (pinky.size() >= 6)
        expect (pinky[0].note == pinky[2].note && pinky[2].note == pinky[4].note && pinky[1].note < pinky[3].note,
                "pinky returns to the lowest note");

    // Every arp style uses only the chord's notes and is the same for the same seed.
    arp.rhythm = 2;
    for (int style = 0; style < (int) getArpStyleNames().size(); ++style)
    {
        arp.arpStyle = (ArpStyle) style;
        const auto first = generateMelody (pop, arp);
        const auto again = generateMelody (pop, arp);
        expect (first.size() == 64, std::string ("style ") + getArpStyleNames()[(size_t) style] + " fills the sixteenths");
        bool same = first.size() == again.size();
        for (size_t i = 0; same && i < first.size(); ++i)
            same = first[i].note == again[i].note;
        expect (same, std::string ("style ") + getArpStyleNames()[(size_t) style] + " repeats with the same seed");
        for (size_t i = 0; i < std::min<size_t> (16, first.size()); ++i)
            expect (std::set<int> { 0, 4, 7 }.count (first[i].note % 12) == 1,
                    std::string ("style ") + getArpStyleNames()[(size_t) style] + " uses C chord notes");
    }

    arp.arpStyle = ArpStyle::converge; // 60 64 67 72 76 79 -> outside in
    const auto converge = generateMelody (pop, arp);
    expect (converge.size() > 3 && converge[0].note == 60 && converge[1].note == 79 && converge[2].note == 64 && converge[3].note == 76,
            "outside in goes 60 79 64 76");

    arp.arpStyle = ArpStyle::octaves;
    const auto pairs = generateMelody (pop, arp);
    expect (pairs.size() > 3 && pairs[1].note == pairs[0].note + 12 && pairs[2].note == 64, "octave pairs: note, octave, next note");

    arp.arpStyle = ArpStyle::up;
    arp.arpOctaves = 1;
    const auto oneOctave = generateMelody (pop, arp);
    expect (oneOctave.size() > 3 && oneOctave[3].note == 60, "one octave of C wraps after three notes");
    arp.arpOctaves = 3;
    const auto threeOctaves = generateMelody (pop, arp);
    expect (threeOctaves.size() > 8 && threeOctaves[6].note == 84 && threeOctaves[8].note == 91, "three octaves of C reach 91");
    arp.arpOctaves = 2;

    arp.arpNotes = ArpNotes::add9;
    const auto ninth = generateMelody (pop, arp);
    expect (ninth.size() > 1 && ninth[1].note == 62, "add 9th puts D in the C arpeggio");
    arp.arpNotes = ArpNotes::rootFifth;
    const auto fifths = generateMelody (pop, arp);
    expect (fifths.size() > 2 && fifths[1].note == 67 && fifths[2].note == 72, "root and 5th skips the third");
    arp.arpNotes = ArpNotes::chord;

    arp.bassNote = true;
    const auto withBass = generateMelody (pop, arp);
    expect (withBass.size() == 68 && withBass[0].note == 48 && withBass[0].lengthBeats > 3.5, "bass note holds C2 under the C arpeggio");
    arp.bassNote = false;

    arp.keepFlowing = true;
    const auto flowing = generateMelody (pop, arp);
    expect (flowing.size() == 64 && flowing[16].note == 76, "keep flowing carries on through the chord change");
    arp.keepFlowing = false;

    arp.gate = 0;
    const auto staccato = generateMelody (pop, arp);
    expect (! staccato.empty() && std::abs (staccato[0].lengthBeats - 0.125) < 1.0e-6, "staccato halves the length");
    arp.gate = 1;

    arp.rhythm = 1; // eighths
    arp.swing = 3;
    const auto swung = generateMelody (pop, arp);
    expect (swung.size() > 1 && std::abs (swung[1].startBeat - 0.665) < 1.0e-6 && swung[1].startBeat + swung[1].lengthBeats < 1.0 + 1.0e-6,
            "heavy swing moves the off-beat eighth late");
    arp.swing = 0;
    arp.rhythm = 2;

    arp.variation = 3;
    const auto varied = generateMelody (pop, arp);
    bool variedDiffers = varied.size() != up.size();
    for (size_t i = 0; ! variedDiffers && i < varied.size(); ++i)
        variedDiffers = varied[i].note != up[i].note || std::abs (varied[i].startBeat - up[i].startBeat) > 1.0e-6;
    expect (variedDiffers, "wild variation changes the arpeggio");
    for (const auto& note : varied)
        if (note.startBeat < 4.0)
            expect (std::set<int> { 0, 4, 7 }.count (note.note % 12) == 1, "variation keeps to the chord's notes");
    expect (! varied.empty() && varied[0].note == 60, "variation keeps the first note of the chord");

    for (int r = 0; r < (int) getRhythmPatterns().size(); ++r)
    {
        arp.rhythm = r;
        const auto notes = generateMelody (pop, arp);
        expect (! notes.empty(), std::string ("rhythm ") + getRhythmPatterns()[(size_t) r].name + " makes notes");
        for (const auto& note : notes)
            expect (note.startBeat >= 0.0 && note.startBeat + note.lengthBeats <= 16.0 + 1.0e-6,
                    std::string ("rhythm ") + getRhythmPatterns()[(size_t) r].name + " stays inside the chords");
    }

    if (failures == 0)
        std::cout << "All melody tests passed\n";
    return failures == 0 ? 0 : 1;
}
