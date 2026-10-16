#include "ChordLibrary.h"
#include "ChordEngine.h"

#include <algorithm>
#include <array>
#include <sstream>

namespace madscaler
{
    const std::vector<LibraryProgression>& getLibrary()
    {
        // MadScaler's own collection. Scale indexes: 0 Major, 1 Natural Minor,
        // 2 Harmonic Minor, 4 Dorian, 5 Phrygian, 6 Lydian, 7 Mixolydian,
        // 10 Phrygian Dominant.
        static const std::vector<LibraryProgression> library {
            // Pop
            { "Four chord anthem",     0, "Pop|Anthem|Happy|Hopeful",                  "I V vi IV" },
            { "Emotional four",        0, "Pop|EDM|Ballad|Bittersweet",                "vi IV I V" },
            { "Fifties ballad",        0, "Pop|Nostalgic|Romantic|Ballad",             "I vi IV V" },
            { "Deceptive turn",        0, "Pop|Classical|Bittersweet",                 "I IV V vi" },
            { "Royal road",            0, "Pop|Hopeful|Romantic|Game music",           "IVmaj7 V7 iii7 vi" },
            { "Feel good",             0, "Pop|Happy|Folk & Country",                  "I iii IV V" },
            { "Sunrise",               0, "Pop|Hopeful|Intro & outro",                 "Isus2 IV vi V" },
            { "Tearjerker",            0, "Pop|Sad|Ballad|Film score",                 "IV I V vi" },
            { "Minor pop loop",        1, "Pop|EDM|Epic|Sad|Trailer",                  "i bVI bIII bVII" },
            { "Bittersweet minor iv",  0, "Pop|Rock|Bittersweet|Nostalgic",            "I III IV iv" },
            { "Chill sunset",          0, "Pop|Chill|Dreamy",                          "Imaj7 iii7 IVmaj7 iv6" },
            { "Stepping up",           0, "Pop|Happy|Game music",                      "I:2 ii:2 iii:2 IV:2 V:4 V:4" },
            { "Hopeful ballad",        0, "Pop|Ballad|Hopeful",                        "I V vi iii IV I ii V" },
            { "Heartbreak ballad",     1, "Pop|Ballad|Sad",                            "i iv bVII bIII bVI iv V V" },

            // Rock and metal
            { "Mixolydian rock",       7, "Rock|Anthem|Heroic",                        "I bVII IV I" },
            { "Pentatonic riff",       0, "Rock|Blues",                                "I bIII IV I" },
            { "Aeolian rock",          1, "Rock|Epic|Dark",                            "i bVII bVI bVII" },
            { "Adventure road",        0, "Rock|Heroic|Game music",                    "I V bVII IV" },
            { "Power ballad",          0, "Rock|Ballad|Romantic",                      "I iii vi IV" },
            { "Garage stomp",          0, "Rock|Happy",                                "I bVII IV IV" },
            { "Anthem chorus",         0, "Rock|Anthem|Triumphant",                    "IV V I I" },
            { "Gallop",                1, "Metal|Battle|Dark|Trailer",                 "i bVI bVII i" },
            { "Phrygian riff",         5, "Metal|Dark|Tense|Horror",                   "i bII i bVII" },
            { "Harmonic minor doom",   2, "Metal|Dark|Epic",                           "i iv bVI V" },
            { "Power chord descent",   1, "Metal|Rock|Tense",                          "i5 bVII5 bVI5 V5" },

            // Electronic and hip-hop
            { "Future bass",           0, "EDM|Dreamy|Chill",                          "IVmaj9 V iii7 vi9" },
            { "Deep house vamp",       1, "EDM|Chill|Study",                           "i7:8 iv7:8" },
            { "Rising house",          1, "EDM|Synthwave|Hopeful|Anthem",              "bVI bVII i i" },
            { "Techno pulse",          5, "EDM|Dark|Tense|Sci-fi",                     "i:8 bII:8" },
            { "Trance anthem",         1, "EDM|Epic|Triumphant",                       "bVI bVII bIII bVII" },
            { "Dark trap",             2, "Hip-hop|Dark|Tense",                        "i bVI V i" },
            { "Haunted trap",          2, "Hip-hop|Horror|Dark",                       "i ii\xC2\xB0 V i" },
            { "Boom bap soul",         4, "Hip-hop|R&B & Soul|Chill|Nostalgic",        "i9:8 IV9:8" },
            { "Cloud rap",             0, "Hip-hop|Dreamy|Chill",                      "Imaj7 iii7 vi7 IVmaj7" },
            { "Drill",                 1, "Hip-hop|Dark|Tense",                        "i bVI iv V" },
            { "Night drive",           1, "Synthwave|Nostalgic|Mysterious",            "i bIII bVII bVI" },
            { "Outrun",                1, "Synthwave|Heroic|Game music",               "i bVI bVII V" },

            // R&B, soul, gospel and funk
            { "Neo-soul two-five",     0, "R&B & Soul|Jazz|Romantic|Chill",            "ii9 V13 Imaj9 vi9" },
            { "Backdoor soul",         0, "R&B & Soul|Romantic",                       "IVmaj7 bVII7 Imaj7 Imaj7" },
            { "Soul turnaround",       0, "R&B & Soul|Gospel|Bittersweet",             "I I7 IV iv" },
            { "Slow jam",              0, "R&B & Soul|Romantic|Love scene",            "vi7 ii7 IVmaj7 V7sus4" },
            { "Gospel turnaround",     0, "Gospel|R&B & Soul|Happy",                   "I:2 I7:2 IV:2 iv:2 I:2 VI7:2 ii7:2 V7:2" },
            { "Hymn",                  0, "Gospel|Classical|Hopeful|Wedding",          "I:2 IV:2 I:2 V:2 I:2 IV:2 V:2 I:2" },
            { "Gospel climb",          0, "Gospel|Triumphant",                         "IV V iii vi ii V I I" },
            { "Dorian funk vamp",      4, "Funk|R&B & Soul|Chill",                     "i7:8 IV7:8" },
            { "Dominant funk",         7, "Funk|Happy",                                "I7:8 IV7:8" },
            { "Funk turnaround",       4, "Funk|Jazz",                                 "i9 i9 IV9 bVII9" },

            // Jazz, lo-fi and blues
            { "Two-five-one",          0, "Jazz|Chill",                                "ii7 V7 Imaj7 Imaj7" },
            { "Minor two-five-one",    1, "Jazz|Mysterious|Sad",                       "ii\xC3\xB8" "7 V7 i6 i6" },
            { "Rhythm turnaround",     0, "Jazz|Happy",                                "Imaj7:2 vi7:2 ii7:2 V7:2" },
            { "Tritone sub",           0, "Jazz|Chill|Romantic",                       "ii7 bII7 Imaj7 Imaj7" },
            { "Cycle of fifths",       0, "Jazz|Nostalgic|Romantic|Classical",         "ii7 V7 Imaj7 IVmaj7 vii\xC3\xB8" "7 III7 vi7 vi7" },
            { "Ragtime",               0, "Jazz|Happy|Nostalgic|Game music",           "I III7 VI7 II7:2 V7:2" },
            { "Line clich\xC3\xA9",    1, "Jazz|Mysterious|Film score|Tense",          "i imaj7 i7 i6" },
            { "Bossa",                 0, "Latin|Jazz|Romantic|Chill",                 "Imaj7 II7 ii7 bII7" },
            { "Lo-fi study",           0, "Lo-fi|Study|Chill",                         "ii7 V7 Imaj7 vi7" },
            { "Lo-fi descent",         0, "Lo-fi|Nostalgic|Study",                     "IVmaj7 iii7 ii7 Imaj7" },
            { "Lo-fi drift",           0, "Lo-fi|Dreamy|Chill|Ambient",                "Imaj7:8 IVmaj7:8" },
            { "Rainy window",          1, "Lo-fi|Sad|Study",                           "i9 bVImaj7 iv9 V7" },
            { "Late night",            4, "Lo-fi|Chill|Mysterious",                    "i7 IV7 bIIImaj7 ii7" },
            { "Study session",         0, "Lo-fi|Study|Chill",                         "Imaj9 vi9 ii9 V9" },
            { "Twelve-bar blues",      0, "Blues|Rock",                                "I7 I7 I7 I7 IV7 IV7 I7 I7 V7 IV7 I7 V7" },
            { "Quick-change blues",    0, "Blues|Jazz",                                "I7 IV7 I7 I7 IV7 IV7 I7 I7 V7 IV7 I7 V7" },
            { "Minor blues",           1, "Blues|Sad|Dark",                            "i7 i7 i7 i7 iv7 iv7 i7 i7 bVI7 V7 i7 V7" },
            { "Eight-bar blues",       0, "Blues|Folk & Country",                      "I7 V7 IV7 IV7 I7 V7 I7 V7" },

            // Folk, country, Latin and reggae
            { "Country three-chord",   0, "Folk & Country|Happy",                      "I I IV IV V V I I" },
            { "Folk song",             0, "Folk & Country|Pop|Nostalgic|Hopeful",      "I IV vi V" },
            { "Campfire",              0, "Folk & Country|Happy|Chill",                "I V IV I" },
            { "Celtic",                1, "Folk & Country|Fantasy|Epic",               "i bVII i bVII bIII bVII i i" },
            { "Dorian folk",           4, "Folk & Country|Fantasy|Mysterious",         "i IV i IV" },
            { "Andalusian cadence",    1, "Latin|Rock|Mysterious|Tense",               "i bVII bVI V" },
            { "Son montuno",           0, "Latin|Happy",                               "I IV V IV" },
            { "Flamenco",              10, "Latin|Tense|Mysterious",                   "I bII bIII bII" },
            { "Bolero",                1, "Latin|Romantic|Sad",                        "i iv V i" },
            { "Reggae skank",          0, "Reggae|Happy|Chill",                        "I IV I V" },
            { "Roots minor",           1, "Reggae|Chill",                              "i iv i v" },
            { "Dub lift",              0, "Reggae|Chill|Dreamy",                       "ii7:8 Imaj7:8" },

            // Classical
            { "Canon style",           0, "Classical|Wedding|Romantic",                "I V vi iii IV I IV V" },
            { "Baroque circle",        0, "Classical|Hopeful",                         "I IV vii\xC2\xB0 iii vi ii V I" },
            { "Neapolitan drama",      1, "Classical|Tense|Film score",                "i bII V i" },
            { "Picardy ending",        1, "Classical|Hopeful|Intro & outro",           "i iv V I" },
            { "Romantic swell",        0, "Classical|Romantic|Love scene|Film score",  "I III vi IV" },
            { "Sad waltz",             1, "Classical|Sad|Romantic",                    "i:3 iv:3 V:3 i:3" },
            { "Wedding march",         0, "Wedding|Happy|Classical",                   "I IV V I" },

            // Film, games and other uses
            { "Hero's cadence",        0, "Heroic|Triumphant|Game music|Film score",   "I:4 bVI:2 bVII:2 I:8" },
            { "Victory fanfare",       0, "Triumphant|Heroic|Game music",              "bVI:2 bVII:2 I:4" },
            { "Superhero",             0, "Heroic|Triumphant|Film score|Anthem",       "I bVII IV I bVI bVII I I" },
            { "Heroic Lydian",         6, "Heroic|Film score|Hopeful",                 "I II I V" },
            { "Tragic hero",           2, "Film score|Sad|Epic|Heroic",                "i bVI iv V i bVI bII V" },
            { "Chromatic wonder",      0, "Film score|Fantasy|Dreamy|Sci-fi",          "I bVI I bIII" },
            { "Lydian dream",          6, "Film score|Dreamy|Fantasy|Sci-fi",          "I II I II" },
            { "Villain",               1, "Film score|Dark|Horror|Mysterious",         "i bvi i bvi" },
            { "Melancholy",            0, "Film score|Sad|Bittersweet",                "vi iii IV I" },
            { "Hope after loss",       0, "Film score|Bittersweet|Hopeful",            "vi V IV I" },
            { "Magic plagal",          0, "Film score|Fantasy|Nostalgic|Dreamy",       "I IV iv I" },
            { "Tritone dread",         1, "Horror|Tense|Sci-fi",                       "i #iv i #iv" },
            { "Suspense build",        1, "Horror|Tense|Trailer",                      "i:2 bII:2 i:2 bII:2 i:2 bII:2 V:4" },
            { "Dark pulse",            2, "Horror|Dark|Hip-hop",                       "i:8 bVI:8 vii\xC2\xB0" "7:8 i:8" },
            { "Epic trailer",          1, "Trailer|Epic|Battle|Film score",            "i bVI bIII bVII i bVI bVII bVII" },
            { "Rising stakes",         1, "Trailer|Epic|Tense",                        "i iv bVI bVII" },
            { "Tense ostinato",        1, "Trailer|Tense|Film score",                  "i:2 i:2 bVI:2 bVI:2 i:2 i:2 bII:4" },
            { "Battle march",          2, "Battle|Epic|Dark|Game music",               "i:2 bVI:2 bVII:2 i:2 i:2 bVI:2 V:4" },
            { "Boss fight",            2, "Battle|Game music|Tense",                   "i:2 ii\xC2\xB0:2 V:2 i:2" },
            { "Village",               0, "Game music|Happy|Fantasy",                  "I:2 vi:2 IV:2 V:2 I:2 iii:2 IV:2 V:2" },
            { "Dungeon",               1, "Game music|Mysterious|Dark",                "i bII i vii\xC2\xB0" },
            { "Fantasy quest",         4, "Fantasy|Heroic|Game music",                 "i bIII bVII IV" },
            { "Mystic",                5, "Fantasy|Mysterious|Meditation",             "i:8 bII:8 bvii:8 i:8" },
            { "Space drift",           6, "Sci-fi|Ambient|Dreamy",                     "Imaj7:8 II:8" },
            { "Starfield",             0, "Sci-fi|Ambient|Hopeful",                    "Isus2:8 bVIsus2:8 IVsus2:8 Isus2:8" },
            { "Love theme",            0, "Love scene|Romantic|Film score",            "Imaj7 IVmaj7 iii7 vi7" },
            { "First kiss",            0, "Love scene|Romantic|Dreamy",                "I vi7 ii7 V7sus4" },
            { "Floating",              0, "Ambient|Meditation|Dreamy",                 "Isus2:8 IVsus2:8 vi7:8 Vsus4:8" },
            { "Drone",                 0, "Ambient|Meditation",                        "Isus2:16 IVadd9:16" },
            { "Ambient minor",         1, "Ambient|Sad|Film score",                    "i9:8 bVImaj7:8 bIIImaj7:8 bVII6:8" },
            { "Morning light",         6, "Ambient|Hopeful|Meditation",                "Imaj7:8 II:8 iii7:8 II:8" },
            { "Intro swell",           0, "Intro & outro|Hopeful",                     "IV:4 V:4 I:8" },
            { "Fade out",              0, "Intro & outro|Nostalgic|Bittersweet",       "IV:4 iv:4 I:8" },
        };
        return library;
    }

    const std::vector<TagGroup>& getLibraryTagGroups()
    {
        static const std::vector<TagGroup> groups {
            { "Style", { "Pop", "Rock", "Metal", "EDM", "Synthwave", "Hip-hop", "R&B & Soul", "Gospel", "Funk",
                         "Jazz", "Lo-fi", "Blues", "Folk & Country", "Latin", "Reggae", "Classical" } },
            { "Mood", { "Happy", "Hopeful", "Triumphant", "Heroic", "Epic", "Romantic", "Dreamy", "Chill",
                        "Nostalgic", "Bittersweet", "Sad", "Mysterious", "Tense", "Dark" } },
            { "Use", { "Film score", "Trailer", "Game music", "Battle", "Fantasy", "Sci-fi", "Horror",
                       "Love scene", "Ambient", "Meditation", "Study", "Ballad", "Anthem", "Wedding",
                       "Intro & outro" } },
        };
        return groups;
    }

    std::vector<std::string> getTags (const LibraryProgression& progression)
    {
        std::vector<std::string> tags;
        std::stringstream stream (progression.tags);
        std::string tag;
        while (std::getline (stream, tag, '|'))
            if (! tag.empty())
                tags.push_back (tag);
        return tags;
    }

    bool hasTag (const LibraryProgression& progression, const std::string& tag)
    {
        const auto tags = getTags (progression);
        return std::find (tags.begin(), tags.end(), tag) != tags.end();
    }

    std::vector<std::string> getChordTokens (const LibraryProgression& progression)
    {
        std::vector<std::string> tokens;
        std::stringstream stream (progression.chords);
        std::string token;
        while (stream >> token)
            tokens.push_back (token);
        return tokens;
    }

    bool parseChordToken (const std::string& token, ParsedChord& result)
    {
        auto text = token;
        result = {};

        if (const auto colon = text.find (':'); colon != std::string::npos)
        {
            try
            {
                result.beats = std::stod (text.substr (colon + 1));
            }
            catch (...)
            {
                return false;
            }

            if (result.beats <= 0.0 || result.beats > 64.0)
                return false;

            text = text.substr (0, colon);
        }

        size_t pos = 0;
        int accidental = 0;
        if (pos < text.size() && (text[pos] == 'b' || text[pos] == '#'))
            accidental = text[pos++] == 'b' ? -1 : 1;

        const auto numeralStart = pos;
        while (pos < text.size() && (text[pos] == 'I' || text[pos] == 'V' || text[pos] == 'i' || text[pos] == 'v'))
            ++pos;

        const auto numeral = text.substr (numeralStart, pos - numeralStart);
        if (numeral.empty())
            return false;

        const bool upper = std::all_of (numeral.begin(), numeral.end(), [] (char c) { return c == 'I' || c == 'V'; });
        const bool lower = std::all_of (numeral.begin(), numeral.end(), [] (char c) { return c == 'i' || c == 'v'; });
        if (! upper && ! lower)
            return false;

        std::string key;
        for (auto c : numeral)
            key += (char) (c == 'i' ? 'I' : c == 'v' ? 'V' : c);

        static const std::array<const char*, 7> numerals { "I", "II", "III", "IV", "V", "VI", "VII" };
        static const std::array<int, 7> majorScale { 0, 2, 4, 5, 7, 9, 11 };

        const auto found = std::find_if (numerals.begin(), numerals.end(), [&] (const char* n) { return key == n; });
        if (found == numerals.end())
            return false;

        const auto degree = (size_t) (found - numerals.begin());
        result.rootOffset = ((majorScale[degree] + accidental) % 12 + 12) % 12;

        struct Shape
        {
            const char* suffix;
            std::vector<int> majorIntervals;
            const char* majorName;
            std::vector<int> minorIntervals;
            const char* minorName;
        };

        static const std::vector<Shape> shapes {
            { "",                    { 0, 4, 7 },             "",      { 0, 3, 7 },             "m" },
            { "\xC2\xB0",            { 0, 3, 6 },             "dim",   { 0, 3, 6 },             "dim" },
            { "dim",                 { 0, 3, 6 },             "dim",   { 0, 3, 6 },             "dim" },
            { "+",                   { 0, 4, 8 },             "aug",   { 0, 4, 8 },             "aug" },
            { "aug",                 { 0, 4, 8 },             "aug",   { 0, 4, 8 },             "aug" },
            { "7",                   { 0, 4, 7, 10 },         "7",     { 0, 3, 7, 10 },         "m7" },
            { "maj7",                { 0, 4, 7, 11 },         "maj7",  { 0, 3, 7, 11 },         "m(maj7)" },
            { "6",                   { 0, 4, 7, 9 },          "6",     { 0, 3, 7, 9 },          "m6" },
            { "9",                   { 0, 4, 7, 10, 14 },     "9",     { 0, 3, 7, 10, 14 },     "m9" },
            { "maj9",                { 0, 4, 7, 11, 14 },     "maj9",  { 0, 3, 7, 11, 14 },     "m(maj9)" },
            { "add9",                { 0, 4, 7, 14 },         "add9",  { 0, 3, 7, 14 },         "madd9" },
            { "11",                  { 0, 7, 10, 14, 17 },    "11",    { 0, 3, 7, 10, 14, 17 }, "m11" },
            { "13",                  { 0, 4, 7, 10, 14, 21 }, "13",    { 0, 3, 7, 10, 14, 21 }, "m13" },
            { "sus2",                { 0, 2, 7 },             "sus2",  { 0, 2, 7 },             "sus2" },
            { "sus4",                { 0, 5, 7 },             "sus4",  { 0, 5, 7 },             "sus4" },
            { "7sus4",               { 0, 5, 7, 10 },         "7sus4", { 0, 5, 7, 10 },         "7sus4" },
            { "\xC3\xB8" "7",        { 0, 3, 6, 10 },         "m7b5",  { 0, 3, 6, 10 },         "m7b5" },
            { "m7b5",                { 0, 3, 6, 10 },         "m7b5",  { 0, 3, 6, 10 },         "m7b5" },
            { "\xC2\xB0" "7",        { 0, 3, 6, 9 },          "dim7",  { 0, 3, 6, 9 },          "dim7" },
            { "dim7",                { 0, 3, 6, 9 },          "dim7",  { 0, 3, 6, 9 },          "dim7" },
            { "5",                   { 0, 7 },                "5",     { 0, 7 },                "5" },
        };

        const auto suffix = text.substr (pos);
        for (const auto& shape : shapes)
        {
            if (suffix == shape.suffix)
            {
                result.intervals = upper ? shape.majorIntervals : shape.minorIntervals;
                result.suffix = upper ? shape.majorName : shape.minorName;
                return true;
            }
        }

        return false;
    }

    std::vector<ProgressionSlot> buildLibraryProgression (const LibraryProgression& progression,
                                                          int keyPitchClass, int baseNote)
    {
        std::vector<ProgressionSlot> slots;

        for (const auto& token : getChordTokens (progression))
        {
            ParsedChord chord;
            if (! parseChordToken (token, chord))
                continue;

            ProgressionSlot slot;
            slot.name = getNoteName (keyPitchClass + chord.rootOffset) + chord.suffix;
            slot.lengthBeats = chord.beats;
            for (auto interval : chord.intervals)
                slot.notes.push_back (std::clamp (baseNote + chord.rootOffset + interval, 0, 127));
            slots.push_back (std::move (slot));

            if ((int) slots.size() == Progression::maxSlots)
                break;
        }

        return slots;
    }
} // namespace madscaler
