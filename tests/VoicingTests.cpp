#include "Voicing.h"

#include <iostream>
#include <string>
#include <vector>

using namespace madscaler;

static int failures = 0;

static void expectNotes (const std::vector<int>& actual, const std::vector<int>& expected, const std::string& what)
{
    if (actual != expected)
    {
        ++failures;
        std::cerr << "FAIL: " << what << " got";
        for (auto note : actual)
            std::cerr << ' ' << note;
        std::cerr << '\n';
    }
}

static VoicingSettings settings (int inversion, VoicingStyle style, bool bass = false, bool leading = false)
{
    VoicingSettings s;
    s.inversion = inversion;
    s.style = style;
    s.addBass = bass;
    s.voiceLeading = leading;
    return s;
}

int main()
{
    const std::vector<int> cMajor { 60, 64, 67 };
    const std::vector<int> cMaj7 { 60, 64, 67, 71 };

    expectNotes (voiceChord (cMajor, settings (0, VoicingStyle::close), {}), { 60, 64, 67 }, "root position");
    expectNotes (voiceChord (cMajor, settings (1, VoicingStyle::close), {}), { 64, 67, 72 }, "first inversion");
    expectNotes (voiceChord (cMajor, settings (2, VoicingStyle::close), {}), { 67, 72, 76 }, "second inversion");
    expectNotes (voiceChord (cMajor, settings (5, VoicingStyle::close), {}), { 67, 72, 76 }, "inversion is clamped");
    expectNotes (voiceChord (cMaj7, settings (3, VoicingStyle::close), {}), { 71, 72, 76, 79 }, "third inversion");

    expectNotes (voiceChord (cMajor, settings (0, VoicingStyle::open), {}), { 60, 67, 76 }, "open triad");
    expectNotes (voiceChord (cMaj7, settings (0, VoicingStyle::drop2), {}), { 55, 60, 64, 71 }, "drop 2");
    expectNotes (voiceChord (cMaj7, settings (0, VoicingStyle::drop3), {}), { 52, 60, 67, 71 }, "drop 3");

    expectNotes (voiceChord (cMajor, settings (0, VoicingStyle::close, true), {}), { 48, 60, 64, 67 }, "bass in root position");
    expectNotes (voiceChord (cMajor, settings (1, VoicingStyle::close, true), {}), { 48, 64, 67, 72 }, "bass under first inversion");

    // Voice leading: C (60 64 67) to F should pick C F A (60 65 69), the
    // second inversion, rather than jumping to F A C (65 69 72).
    const std::vector<int> fMajor { 65, 69, 72 };
    expectNotes (voiceChord (fMajor, settings (0, VoicingStyle::close, false, true), cMajor),
                 { 60, 65, 69 }, "voice leading C to F");

    // C to G: B D G (59 62 67) moves least.
    const std::vector<int> gMajor { 67, 71, 74 };
    expectNotes (voiceChord (gMajor, settings (0, VoicingStyle::close, false, true), cMajor),
                 { 59, 62, 67 }, "voice leading C to G");

    // Without a previous chord, voice leading uses the chosen inversion.
    expectNotes (voiceChord (fMajor, settings (1, VoicingStyle::close, false, true), {}),
                 { 69, 72, 77 }, "voice leading falls back to the inversion");

    // Bass stays on the root when voice leading.
    const auto led = voiceChord (fMajor, settings (0, VoicingStyle::close, true, true), { 48, 60, 64, 67 });
    if (led.empty() || led.front() % 12 != 5 || led.front() > led[1] - 12)
    {
        ++failures;
        std::cerr << "FAIL: bass under voice-led chord\n";
    }

    // Notes stay in MIDI range.
    for (auto note : voiceChord ({ 120, 124, 127 }, settings (2, VoicingStyle::open), {}))
        if (note < 0 || note > 127)
        {
            ++failures;
            std::cerr << "FAIL: note out of range\n";
        }

    if (failures == 0)
        std::cout << "All voicing tests passed\n";

    return failures == 0 ? 0 : 1;
}
