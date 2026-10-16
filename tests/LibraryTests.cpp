#include "ChordLibrary.h"
#include "ChordEngine.h"

#include <algorithm>
#include <iostream>
#include <map>
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

static std::string names (const std::vector<ProgressionSlot>& slots)
{
    std::string result;
    for (const auto& slot : slots)
        result += (result.empty() ? "" : " ") + slot.name;
    return result;
}

int main()
{
    ParsedChord chord;
    expect (parseChordToken ("bVII", chord) && chord.rootOffset == 10 && chord.suffix.empty(), "bVII");
    expect (parseChordToken ("ii7:2", chord) && chord.rootOffset == 2 && chord.suffix == "m7" && chord.beats == 2.0, "ii7:2");
    expect (parseChordToken ("V7sus4", chord) && chord.intervals == std::vector<int> { 0, 5, 7, 10 }, "V7sus4");
    expect (parseChordToken ("ii\xC3\xB8" "7", chord) && chord.suffix == "m7b5", "half-diminished");
    expect (parseChordToken ("vii\xC2\xB0", chord) && chord.rootOffset == 11 && chord.suffix == "dim", "vii dim");
    expect (parseChordToken ("imaj7", chord) && chord.suffix == "m(maj7)", "minor-major 7th");
    expect (parseChordToken ("#iv", chord) && chord.rootOffset == 6 && chord.suffix == "m", "#iv");
    expect (! parseChordToken ("X", chord), "rejects junk");
    expect (! parseChordToken ("Iv", chord), "rejects mixed case");
    expect (! parseChordToken ("Iblah", chord), "rejects unknown suffix");
    expect (! parseChordToken ("I:0", chord), "rejects zero length");

    // Every progression parses, uses known tags and fits the strip.
    std::set<std::string> knownTags;
    for (const auto& group : getLibraryTagGroups())
        for (const auto* tag : group.tags)
            knownTags.insert (tag);

    std::map<std::string, int> tagCounts;
    std::set<std::string> chordStrings, progressionNames;

    for (const auto& progression : getLibrary())
    {
        const std::string name = progression.name;
        expect (progressionNames.insert (name).second, "unique name: " + name);
        expect (chordStrings.insert (progression.chords).second, "unique chords: " + name);
        expect (progression.scaleIndex >= 0 && progression.scaleIndex < (int) getScales().size(), "scale: " + name);

        const auto tokens = getChordTokens (progression);
        expect (! tokens.empty() && (int) tokens.size() <= Progression::maxSlots, "length: " + name);
        for (const auto& token : tokens)
            expect (parseChordToken (token, chord), "token '" + token + "' in " + name);

        const auto tags = getTags (progression);
        expect (! tags.empty(), "has tags: " + name);
        for (const auto& tag : tags)
        {
            expect (knownTags.count (tag) == 1, "known tag '" + tag + "' in " + name);
            ++tagCounts[tag];
        }
    }

    for (const auto& tag : knownTags)
        expect (tagCounts[tag] >= 2, "tag has at least two progressions: " + tag);

    expect (getLibrary().size() >= 100, "library has 100+ progressions: " + std::to_string (getLibrary().size()));

    // Building in a key.
    auto find = [] (const std::string& name) -> const LibraryProgression*
    {
        for (const auto& progression : getLibrary())
            if (name == progression.name)
                return &progression;
        return nullptr;
    };

    if (const auto* anthem = find ("Four chord anthem"))
    {
        const auto slots = buildLibraryProgression (*anthem, 0, 60);
        expect (names (slots) == "C G Am F", "anthem in C: " + names (slots));
        expect (slots[1].notes == std::vector<int> { 67, 71, 74 }, "G above the tonic");
        expect (names (buildLibraryProgression (*anthem, 7, 67)) == "G D Em C", "anthem in G");
    }

    if (const auto* epic = find ("Minor pop loop"))
        expect (names (buildLibraryProgression (*epic, 9, 57)) == "Am F C G", "minor loop in A");

    if (const auto* hero = find ("Hero's cadence"))
    {
        const auto slots = buildLibraryProgression (*hero, 0, 60);
        expect (names (slots) == "C Ab Bb C", "hero's cadence: " + names (slots));
        expect (slots[1].lengthBeats == 2.0 && slots[3].lengthBeats == 8.0, "lengths");
    }

    if (const auto* mixo = find ("Mixolydian rock"))
        expect (names (buildLibraryProgression (*mixo, 2, 62)) == "D C G D", "Mixolydian rock in D");

    if (failures == 0)
        std::cout << "All library tests passed (" << getLibrary().size() << " progressions)\n";
    return failures == 0 ? 0 : 1;
}
