#include "ChordEngine.h"

#include <iostream>
#include <string>
#include <vector>

using namespace madscaler;

static int failures = 0;

template <typename A, typename B>
static void expectEqual (const A& actual, const B& expected, const std::string& what)
{
    if (! (actual == expected))
    {
        ++failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

static std::vector<std::string> chordNames (int key, int scale, ChordSize size)
{
    std::vector<std::string> names;
    for (int degree = 0; degree < 7; ++degree)
        names.push_back (buildDiatonicChord (key, scale, degree, size, 60 + key).name);
    return names;
}

static std::vector<std::string> romanNumerals (int key, int scale, ChordSize size)
{
    std::vector<std::string> numerals;
    for (int degree = 0; degree < 7; ++degree)
        numerals.push_back (buildDiatonicChord (key, scale, degree, size, 60 + key).roman);
    return numerals;
}

static int scaleIndex (const std::string& name)
{
    const auto& scales = getScales();
    for (size_t i = 0; i < scales.size(); ++i)
        if (name == scales[i].name)
            return (int) i;
    std::cerr << "Unknown scale " << name << '\n';
    ++failures;
    return 0;
}

int main()
{
    const int major = scaleIndex ("Major");
    const int minor = scaleIndex ("Natural Minor");
    const int harmonicMinor = scaleIndex ("Harmonic Minor");
    const int dorian = scaleIndex ("Dorian");

    expectEqual (chordNames (0, major, ChordSize::triad),
                 std::vector<std::string> { "C", "Dm", "Em", "F", "G", "Am", "Bdim" },
                 "C major triads");

    expectEqual (chordNames (0, major, ChordSize::seventh),
                 std::vector<std::string> { "Cmaj7", "Dm7", "Em7", "Fmaj7", "G7", "Am7", "Bm7b5" },
                 "C major sevenths");

    expectEqual (chordNames (9, minor, ChordSize::triad),
                 std::vector<std::string> { "Am", "Bdim", "C", "Dm", "Em", "F", "G" },
                 "A minor triads");

    expectEqual (chordNames (9, harmonicMinor, ChordSize::seventh),
                 std::vector<std::string> { "Am(maj7)", "Bm7b5", "Caug(maj7)", "Dm7", "E7", "Fmaj7", "G#dim7" },
                 "A harmonic minor sevenths");

    expectEqual (chordNames (2, dorian, ChordSize::triad),
                 std::vector<std::string> { "Dm", "Em", "F", "G", "Am", "Bdim", "C" },
                 "D dorian triads");

    expectEqual (chordNames (7, major, ChordSize::triad),
                 std::vector<std::string> { "G", "Am", "Bm", "C", "D", "Em", "F#dim" },
                 "G major triads");

    expectEqual (romanNumerals (0, major, ChordSize::triad),
                 std::vector<std::string> { "I", "ii", "iii", "IV", "V", "vi", "vii\xC2\xB0" },
                 "C major roman numerals");

    // Voicing: root position, stacked upwards from the tonic octave.
    expectEqual (buildDiatonicChord (0, major, 0, ChordSize::triad, 60).notes,
                 std::vector<int> { 60, 64, 67 }, "C major I notes");
    expectEqual (buildDiatonicChord (0, major, 4, ChordSize::seventh, 60).notes,
                 std::vector<int> { 67, 71, 74, 77 }, "C major V7 notes");
    expectEqual (buildDiatonicChord (0, major, 6, ChordSize::triad, 60).notes,
                 std::vector<int> { 71, 74, 77 }, "C major vii notes");

    // Every scale must give a recognised quality on every degree.
    for (int s = 0; s < (int) getScales().size(); ++s)
        for (int key = 0; key < 12; ++key)
            for (int degree = 0; degree < 7; ++degree)
                for (auto size : { ChordSize::triad, ChordSize::seventh })
                {
                    const auto chord = buildDiatonicChord (key, s, degree, size, 48 + key);
                    if (chord.quality == ChordQuality::unknown && size == ChordSize::triad)
                    {
                        ++failures;
                        std::cerr << "FAIL: unknown triad in " << getScales()[(size_t) s].name
                                  << " degree " << degree << '\n';
                    }
                    for (auto note : chord.notes)
                        if (! isInScale (note, key, s))
                        {
                            ++failures;
                            std::cerr << "FAIL: out-of-scale note in " << getScales()[(size_t) s].name << '\n';
                        }
                }

    // Spelling in sharp and flat keys.
    expectEqual (keyUsesFlats (0, 0), false, "C major uses no flats");
    expectEqual (keyUsesFlats (5, 0), true, "F major uses flats");
    expectEqual (keyUsesFlats (0, 1), true, "C minor uses flats");
    expectEqual (keyUsesFlats (9, 2), false, "A harmonic minor uses sharps");
    expectEqual (spellNote (10, 5, 0), std::string ("Bb"), "Bb in F major");
    expectEqual (spellNote (8, 0, 0), std::string ("Ab"), "borrowed bVI in C is Ab");
    expectEqual (spellNote (6, 0, 0), std::string ("F#"), "#4 in C is F#");
    expectEqual (spellNote (8, 9, 2), std::string ("G#"), "leading tone of A minor is G#");
    expectEqual (buildDiatonicChord (10, 0, 3, ChordSize::triad, 58).name, std::string ("Eb"), "IV of Bb major is Eb");

    if (failures == 0)
        std::cout << "All chord engine tests passed\n";

    return failures == 0 ? 0 : 1;
}
