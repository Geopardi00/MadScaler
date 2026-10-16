#include "Progression.h"

#include <cmath>
#include <iostream>
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

static bool near (double a, double b) { return std::abs (a - b) < 1.0e-6; }

int main()
{
    Progression progression;
    expect (progression.slotAt (0.0) == -1, "empty progression has no slot");

    progression.slots.push_back ({ "C", { 60, 64, 67 }, 4.0 });
    progression.slots.push_back ({ "Am", { 57, 60, 64 }, 2.0 });
    progression.slots.push_back ({ "F", { 53, 57, 60 }, 2.0 });

    expect (near (progression.totalBeats(), 8.0), "total length");
    expect (progression.slotAt (0.0) == 0, "starts on the first chord");
    expect (progression.slotAt (3.99) == 0, "first chord lasts four beats");
    expect (progression.slotAt (4.0) == 1, "second chord at beat 4");
    expect (progression.slotAt (6.5) == 2, "third chord at beat 6");
    expect (progression.slotAt (8.0) == 0, "loops back to the start");
    expect (progression.slotAt (13.0) == 1, "loops more than once");
    expect (progression.slotAt (-1.0) == 2, "negative positions wrap");
    expect (near (progression.slotStart (2), 6.0), "slot start");

    {
        const auto notes = renderProgression (progression, 90, nullptr);
        expect (notes.size() == 9, "held chords give one note per chord tone");
        expect (near (notes[3].startBeat, 4.0) && near (notes[3].lengthBeats, 2.0) && notes[3].note == 57,
                "Am starts at beat 4 for 2 beats");
        expect (notes[0].velocity == 90, "velocity applied");
    }

    {
        PatternSettings arp;
        arp.mode = PatternMode::arpUp;
        arp.stepBeats = 1.0;
        arp.gate = 0.5f;
        const auto notes = renderProgression (progression, 100, &arp);
        expect (notes.size() == 8, "a quarter-note arp over 8 beats has 8 notes");
        const int expectedNotes[] = { 60, 64, 67, 60, 57, 60, 53, 57 };
        for (size_t i = 0; i < notes.size() && i < 8; ++i)
        {
            expect (notes[i].note == expectedNotes[i], "arp note " + std::to_string (i));
            expect (near (notes[i].startBeat, (double) i), "arp step " + std::to_string (i) + " on the beat");
            expect (near (notes[i].lengthBeats, 0.5), "arp gate " + std::to_string (i));
        }
    }

    if (failures == 0)
        std::cout << "All progression tests passed\n";

    return failures == 0 ? 0 : 1;
}
