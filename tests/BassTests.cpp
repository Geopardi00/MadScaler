#include "BassMaker.h"
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

int main()
{
    // C Am F G, four beats each.
    const std::vector<ProgressionSlot> pop {
        { "C",  { 60, 64, 67 }, 4.0 },
        { "Am", { 57, 60, 64 }, 4.0 },
        { "F",  { 53, 57, 60 }, 4.0 },
        { "G",  { 55, 59, 62 }, 4.0 },
    };
    const int roots[] { 36, 45, 41, 43 }; // C2 A2 F2 G2 from lowNote 36

    BassSettings settings;
    settings.style = BassStyle::wholeNotes;
    auto whole = generateBass (pop, settings);
    expect (whole.size() == 4, "whole notes: one per chord");
    for (size_t i = 0; i < whole.size() && i < 4; ++i)
        expect (whole[i].note == roots[i] && std::abs (whole[i].startBeat - 4.0 * (double) i) < 1.0e-9,
                "whole note " + std::to_string (i) + " is the root: " + std::to_string (whole[i].note));

    settings.style = BassStyle::rootFifth;
    const auto rootFifth = generateBass (pop, settings);
    expect (rootFifth.size() == 16, "root and fifth: quarters");
    expect (rootFifth.size() > 1 && rootFifth[0].note == 36 && rootFifth[1].note == 43, "C then G");
    expect (rootFifth.size() > 5 && rootFifth[4].note == 45 && rootFifth[5].note == 52, "A then E");

    settings.style = BassStyle::pumping;
    expect (generateBass (pop, settings).size() == 32, "pumping eighths");

    settings.style = BassStyle::octaves;
    const auto octaves = generateBass (pop, settings);
    expect (octaves.size() == 32 && octaves[1].note == 48, "octave jumps go up an octave");

    // Every style stays inside the chords, low and sensible.
    for (int style = 0; style < (int) getBassStyleNames().size(); ++style)
    {
        settings.style = (BassStyle) style;
        for (int lead = 0; lead < 2; ++lead)
        {
            settings.leadIn = lead == 1;
            const auto notes = generateBass (pop, settings);
            const std::string name = std::string (getBassStyleNames()[(size_t) style]) + (lead ? " with lead-in" : "");
            expect (! notes.empty(), name + " makes notes");
            for (const auto& note : notes)
            {
                expect (note.startBeat >= 0.0 && note.startBeat + note.lengthBeats <= 16.0 + 1.0e-6, name + " stays in time");
                expect (note.note >= 28 && note.note <= 60, name + " stays low: " + std::to_string (note.note));
            }
            // The first note of each chord is its root (except lead-ins, which come last).
            for (int c = 0; c < 4; ++c)
                for (const auto& note : notes)
                    if (std::abs (note.startBeat - 4.0 * c) < 1.0e-9)
                        expect (note.note % 12 == roots[c] % 12, name + " starts chord " + std::to_string (c) + " on its root");
        }
    }
    settings.leadIn = false;

    // Walking bass: four quarters per chord, the last a step from the next root.
    settings.style = BassStyle::walking;
    for (uint32_t seed = 1; seed < 20; ++seed)
    {
        settings.seed = seed;
        const auto walk = generateBass (pop, settings);
        expect (walk.size() == 16, "walking: quarters");
        if (walk.size() == 16)
            for (int c = 0; c < 4; ++c)
            {
                const auto last = walk[(size_t) (c * 4 + 3)].note;
                const auto next = roots[(c + 1) % 4];
                const auto distance = std::min ({ std::abs (last - next), std::abs (last - next - 12), std::abs (last - next + 12) });
                expect (distance == 1 || distance == 7 || distance == 5, "walking leads into the next root, seed " + std::to_string (seed)
                        + " chord " + std::to_string (c) + ": " + std::to_string (last));
            }
    }

    // Lead-in: a passing note half a beat before each change.
    settings.style = BassStyle::wholeNotes;
    settings.leadIn = true;
    const auto lead = generateBass (pop, settings);
    expect (lead.size() == 8, "whole notes with lead-ins: " + std::to_string (lead.size()));
    if (lead.size() == 8)
    {
        expect (std::abs (lead[1].startBeat - 3.5) < 1.0e-9, "lead-in on 3.5");
        expect (std::abs (lead[1].note - 33) == 1 || std::abs (lead[1].note - 45) == 1, "lead-in is a half step from A: " + std::to_string (lead[1].note));
        expect (lead[0].startBeat + lead[0].lengthBeats <= 3.5 + 1.0e-9, "the held root makes room");
    }
    settings.leadIn = false;

    // Humanise moves things a little, the same way every time.
    settings.style = BassStyle::quarterRoots;
    const auto straight = generateBass (pop, settings);
    settings.humanise = 3;
    const auto loose = generateBass (pop, settings);
    const auto again = generateBass (pop, settings);
    bool moved = false, same = loose.size() == again.size();
    for (size_t i = 0; i < loose.size() && i < straight.size(); ++i)
    {
        moved = moved || std::abs (loose[i].startBeat - straight[i].startBeat) > 1.0e-6 || loose[i].velocity != straight[i].velocity;
        expect (std::abs (loose[i].startBeat - straight[i].startBeat) <= 0.035 + 1.0e-9, "humanise stays close");
        expect (loose[i].startBeat >= 0.0, "humanise never goes before the start");
    }
    for (size_t i = 0; same && i < loose.size(); ++i)
        same = std::abs (loose[i].startBeat - again[i].startBeat) < 1.0e-12 && loose[i].velocity == again[i].velocity;
    expect (moved, "humanise changes timing or velocity");
    expect (same, "humanise repeats with the same seed");
    settings.humanise = 0;

    // Swing and gate.
    settings.style = BassStyle::pumping;
    settings.swing = 3;
    const auto swung = generateBass (pop, settings);
    expect (swung.size() > 1 && std::abs (swung[1].startBeat - 0.665) < 1.0e-6, "swing pushes the off-beat");
    settings.swing = 0;
    settings.gate = 0;
    const auto staccato = generateBass (pop, settings);
    expect (! staccato.empty() && std::abs (staccato[0].lengthBeats - 0.25) < 1.0e-9, "staccato halves");

    // Melody humanise.
    MelodySettings melody;
    melody.humanise = 2;
    const auto human = generateMelody (pop, melody);
    for (const auto& note : human)
        expect (note.startBeat >= 0.0 && note.startBeat + note.lengthBeats <= 16.0 + 1.0e-6, "humanised melody stays in time");

    expect (generateBass ({}, settings).empty(), "no chords, no bass");

    if (failures == 0)
        std::cout << "All bass tests passed\n";
    return failures == 0 ? 0 : 1;
}
