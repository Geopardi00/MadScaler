#include "ChordColours.h"

#include <algorithm>
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

static const ColourChord* find (const std::vector<ColourGroup>& groups, const std::string& title, const std::string& name)
{
    for (const auto& group : groups)
        if (group.title == title)
            for (const auto& chord : group.chords)
                if (chord.name == name)
                    return &chord;
    return nullptr;
}

static std::string names (const std::vector<ColourGroup>& groups, const std::string& title)
{
    std::string result;
    for (const auto& group : groups)
        if (group.title == title)
            for (const auto& chord : group.chords)
                result += (result.empty() ? "" : " ") + chord.name;
    return result;
}

int main()
{
    // C major, octave 4 (tonic C4 = 60), triads.
    const auto one = getChordColours (0, 0, 0, false, 60);
    expect (names (one, "Extensions") == "C6 Cmaj7 Cmaj9 Cadd9 Cmaj13 Csus2 Csus4",
            "I extensions: " + names (one, "Extensions"));

    if (const auto* chord = find (one, "Extensions", "Cmaj9"))
        expect (chord->notes == std::vector<int> { 60, 64, 67, 71, 74 }, "Cmaj9 notes");
    else
        expect (false, "Cmaj9 present");

    if (const auto* chord = find (one, "Extensions", "Cmaj13"))
        expect (chord->notes == std::vector<int> { 60, 64, 67, 71, 74, 81 }, "Cmaj13 leaves out the 11th");
    else
        expect (false, "Cmaj13 present");

    const auto two = getChordColours (0, 0, 1, false, 60);
    expect (names (two, "Extensions") == "Dm6 Dm7 Dm9 Dmadd9 Dm11 Dm13 Dsus2 Dsus4",
            "ii extensions: " + names (two, "Extensions"));
    expect (names (two, "Secondary") == "A7 C#dim7 Eb7", "ii secondary: " + names (two, "Secondary"));

    if (const auto* chord = find (two, "Secondary", "A7"))
    {
        expect (chord->label == "V7/ii", "V7/ii label");
        expect (chord->notes == std::vector<int> { 69, 73, 76, 79 }, "A7 sits above the tonic");
    }

    const auto three = getChordColours (0, 0, 2, false, 60);
    expect (find (three, "Extensions", "Em7(b9)") != nullptr, "iii gets a b9: " + names (three, "Extensions"));
    expect (find (three, "Extensions", "Esus4") != nullptr, "Esus4");
    expect (find (three, "Extensions", "Esus2") == nullptr, "no Esus2 (F is a half step)");

    const auto seven = getChordColours (0, 0, 6, false, 60);
    expect (find (seven, "Extensions", "Bm7b5") != nullptr, "vii half-diminished");
    expect (names (seven, "Secondary").empty(), "no secondary dominants of a diminished chord");

    // Borrowed chords in C major.
    const auto six = getChordColours (0, 0, 5, false, 60);
    if (const auto* chord = find (six, "Borrowed", "Ab"))
        expect (chord->label == "bVI Minor", "bVI from minor, label: " + chord->label);
    else
        expect (false, "bVI borrowed from minor: " + names (six, "Borrowed"));

    const auto four = getChordColours (0, 0, 3, false, 60);
    expect (find (four, "Borrowed", "Fm") != nullptr, "iv from minor");
    expect (find (four, "Borrowed", "F") == nullptr, "the diatonic chord is not repeated");

    const auto fourSevenths = getChordColours (0, 0, 3, true, 60);
    expect (find (fourSevenths, "Borrowed", "Fm7") != nullptr, "borrowed sevenths: " + names (fourSevenths, "Borrowed"));

    // No duplicate chords across groups.
    for (const auto* groups : { &one, &two, &three, &four, &six, &seven })
    {
        std::vector<std::string> all;
        for (const auto& group : *groups)
            for (const auto& chord : group.chords)
                all.push_back (chord.name);
        auto sorted = all;
        std::sort (sorted.begin(), sorted.end());
        expect (std::adjacent_find (sorted.begin(), sorted.end()) == sorted.end(), "no duplicates");
    }

    // A minor (natural minor, index 1): VI and VII are borrowed from major as vi/vii.
    const auto minorOne = getChordColours (9, 1, 0, false, 57);
    expect (find (minorOne, "Borrowed", "A") != nullptr, "I borrowed from major in A minor");

    // Spelling follows the key: F major and C minor use flats.
    expect (names (getChordColours (5, 0, 3, false, 65), "Extensions").rfind ("Bb6", 0) == 0, "IV of F is Bb");
    expect (find (getChordColours (0, 1, 5, false, 60), "Extensions", "Abmaj7") != nullptr, "VI of C minor is Ab");

    // Next chords.
    expect (suggestNextDegrees (0, 4) == std::vector<int> { 0, 5, 3 }, "V goes to I, vi, IV");
    expect (suggestNextDegrees (1, 0) == std::vector<int> { 5, 3, 6 }, "minor i goes to VI, iv, VII");

    // Circle of Fifths.
    expect (circlePitchClass (1, false) == 7, "G is one step clockwise");
    expect (circlePitchClass (11, false) == 5, "F is one step anticlockwise");
    expect (circlePitchClass (0, true) == 9, "Am is the relative minor of C");
    const auto em = buildCircleChord (1, true, 48);
    expect (em.name == "Em" && em.notes == std::vector<int> { 52, 55, 59 }, "Em circle chord");
    expect (isChordInScale (em.notes, 0, 0), "Em is in C major");
    expect (! isChordInScale (buildCircleChord (2, false, 48).notes, 0, 0), "D major is not in C major");

    if (failures == 0)
        std::cout << "All colour tests passed\n";
    return failures == 0 ? 0 : 1;
}
