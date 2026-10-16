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

    if (failures == 0)
        std::cout << "All melody tests passed\n";
    return failures == 0 ? 0 : 1;
}
