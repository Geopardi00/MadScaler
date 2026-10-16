#include "ChordDetector.h"
#include "ChordEngine.h"

#include <iostream>
#include <string>
#include <vector>

using namespace madscaler;

static int failures = 0;

static void expectChord (const std::vector<int>& notes, const std::string& expected)
{
    const auto chord = detectChord (notes);
    const auto actual = chord.valid ? chord.name : std::string ("(none)");
    if (actual != expected)
    {
        ++failures;
        std::cerr << "FAIL: expected " << expected << ", got " << actual << '\n';
    }
}

static void expectTopKey (const std::vector<std::vector<int>>& progression, const std::string& expected)
{
    std::vector<int> weights (12, 0);
    std::vector<int> roots;
    for (const auto& notes : progression)
    {
        const auto chord = detectChord (notes);
        for (auto note : notes)
            ++weights[(size_t) (note % 12)];
        if (chord.valid)
            roots.push_back (chord.rootPitchClass);
    }

    const auto keys = suggestKeys (weights, roots, 3);
    const auto actual = keys.empty() ? std::string ("(none)") : keys.front().name;
    if (actual != expected)
    {
        ++failures;
        std::cerr << "FAIL: expected key " << expected << ", got " << actual << '\n';
    }
}

int main()
{
    // Triads in root position and inversions.
    expectChord ({ 60, 64, 67 }, "C");
    expectChord ({ 57, 60, 64 }, "Am");
    expectChord ({ 64, 67, 72 }, "C/E");
    expectChord ({ 55, 60, 64 }, "C/G");
    expectChord ({ 59, 62, 65 }, "Bdim");
    expectChord ({ 60, 64, 68 }, "Caug");
    expectChord ({ 60, 65, 67 }, "Csus4");
    expectChord ({ 60, 62, 67 }, "Csus2");
    expectChord ({ 48, 55 }, "C5");

    // Sevenths and extensions, any octave or doubling.
    expectChord ({ 55, 59, 62, 65 }, "G7");
    expectChord ({ 59, 62, 65, 67 }, "G7/B");
    expectChord ({ 48, 60, 64, 67, 71 }, "Cmaj7");
    expectChord ({ 57, 60, 64, 67 }, "Am7");
    expectChord ({ 60, 64, 67, 69 }, "C6");
    expectChord ({ 62, 65, 69, 72, 76 }, "Dm9");
    expectChord ({ 59, 62, 65, 69 }, "Bm7b5");
    expectChord ({ 60, 64, 70 }, "C7");
    expectChord ({ 60, 62, 64, 67 }, "Cadd9");

    // Nothing to name.
    expectChord ({ 60 }, "(none)");
    expectChord ({ 60, 61 }, "(none)");

    // Key suggestions from progressions.
    expectTopKey ({ { 60, 64, 67 }, { 65, 69, 72 }, { 67, 71, 74 }, { 60, 64, 67 } }, "C Major");          // C F G C
    expectTopKey ({ { 57, 60, 64 }, { 62, 65, 69 }, { 64, 67, 71 }, { 57, 60, 64 } }, "A Natural Minor"); // Am Dm Em Am
    expectTopKey ({ { 57, 60, 64 }, { 62, 65, 69 }, { 64, 68, 71 }, { 57, 60, 64 } }, "A Harmonic Minor"); // Am Dm E Am
    expectTopKey ({ { 55, 59, 62 }, { 60, 64, 67 }, { 62, 66, 69 }, { 55, 59, 62 } }, "G Major");          // G C D G

    if (failures == 0)
        std::cout << "All chord detector tests passed\n";

    return failures == 0 ? 0 : 1;
}
