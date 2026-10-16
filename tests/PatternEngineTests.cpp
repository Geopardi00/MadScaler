#include "PatternEngine.h"

#include <iostream>
#include <map>
#include <string>
#include <vector>

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

struct Timed
{
    long long time;
    bool on;
    int note;
};

// Runs the engine in 512-sample blocks at 48 kHz and 120 bpm (24000 samples per
// beat, so a 1/16 step is 6000 samples). The chord starts at startTime and is
// cleared at stopTime.
static std::vector<Timed> run (PatternSettings settings, std::vector<int> chord, long long startTime,
                               long long stopTime, long long totalSamples, bool hostPlaying = false,
                               double startPpq = 0.0)
{
    PatternEngine engine;
    engine.reset();
    engine.setSettings (settings);

    std::vector<Timed> result;
    std::vector<PatternEvent> events;
    events.reserve (256);

    const int blockSize = 512;
    for (long long blockStart = 0; blockStart < totalSamples; blockStart += blockSize)
    {
        const auto ppq = startPpq + (double) blockStart / 24000.0;
        engine.beginBlock (48000.0, 120.0, hostPlaying, ppq);
        events.clear();

        if (startTime >= blockStart && startTime < blockStart + blockSize)
            engine.setChord (chord.data(), (int) chord.size(), (int) (startTime - blockStart), events);
        if (stopTime >= blockStart && stopTime < blockStart + blockSize)
            engine.setChord (nullptr, 0, (int) (stopTime - blockStart), events);

        engine.endBlock (blockSize, events);

        for (const auto& e : events)
            result.push_back ({ blockStart + e.sampleOffset, e.noteOn, e.note });
    }
    return result;
}

static std::vector<std::pair<long long, int>> noteOns (const std::vector<Timed>& events)
{
    std::vector<std::pair<long long, int>> ons;
    for (const auto& e : events)
        if (e.on)
            ons.push_back ({ e.time, e.note });
    return ons;
}

static bool balanced (const std::vector<Timed>& events)
{
    std::map<int, int> open;
    for (const auto& e : events)
        open[e.note] += e.on ? 1 : -1;
    for (const auto& [note, count] : open)
        if (count != 0)
            return false;
    return true;
}

static PatternSettings make (PatternMode mode, double stepBeats = 0.25, float gate = 0.5f, int octaves = 1)
{
    PatternSettings s;
    s.mode = mode;
    s.stepBeats = stepBeats;
    s.gate = gate;
    s.octaves = octaves;
    s.strumMs = 30.0;
    s.velocity = 100;
    return s;
}

int main()
{
    const std::vector<int> c { 60, 64, 67 };
    using Ons = std::vector<std::pair<long long, int>>;

    {
        const auto events = run (make (PatternMode::arpUp), c, 0, 23000, 30000);
        expect (noteOns (events) == Ons { { 0, 60 }, { 6000, 64 }, { 12000, 67 }, { 18000, 60 } }, "arp up steps on 1/16");
        expect (events.size() > 1 && ! events[1].on && events[1].time == 3000, "gate 50% ends notes half way");
        expect (balanced (events), "arp up note-ons and note-offs match");
    }

    expect (noteOns (run (make (PatternMode::arpDown), c, 0, 17000, 20000)) == Ons { { 0, 67 }, { 6000, 64 }, { 12000, 60 } },
            "arp down");

    {
        Ons expected;
        const int sequence[] = { 60, 64, 67, 64, 60, 64 };
        for (int i = 0; i < 6; ++i)
            expected.push_back ({ i * 6000, sequence[i] });
        expect (noteOns (run (make (PatternMode::arpUpDown), c, 0, 35000, 40000)) == expected, "arp up/down");
    }

    {
        Ons expected;
        const int sequence[] = { 60, 64, 67, 72, 76, 79, 60 };
        for (int i = 0; i < 7; ++i)
            expected.push_back ({ i * 6000, sequence[i] });
        expect (noteOns (run (make (PatternMode::arpUp, 0.25, 0.5f, 2), c, 0, 41000, 45000)) == expected, "arp over two octaves");
    }

    {
        const auto events = run (make (PatternMode::pulse, 0.5), c, 0, 23000, 30000);
        expect (noteOns (events) == Ons { { 0, 60 }, { 0, 64 }, { 0, 67 }, { 12000, 60 }, { 12000, 64 }, { 12000, 67 } },
                "pulse repeats the whole chord on 1/8");
        expect (balanced (events), "pulse balanced");
    }

    {
        const auto events = run (make (PatternMode::strumUp), c, 1000, 10000, 12000);
        expect (noteOns (events) == Ons { { 1000, 60 }, { 2440, 64 }, { 3880, 67 } }, "strum up, 30 ms apart");
        int offsAtStop = 0;
        for (const auto& e : events)
            if (! e.on)
            {
                expect (e.time == 10000, "strummed notes hold until the chord is released");
                ++offsAtStop;
            }
        expect (offsAtStop == 3, "all strummed notes released");
    }

    expect (noteOns (run (make (PatternMode::strumDown), c, 0, 10000, 12000)) == Ons { { 0, 67 }, { 1440, 64 }, { 2880, 60 } },
            "strum down");

    {
        // Releasing mid-step stops the sounding note at once and nothing more plays.
        const auto events = run (make (PatternMode::arpUp, 0.25, 1.0f), c, 0, 7000, 20000);
        expect (noteOns (events) == Ons { { 0, 60 }, { 6000, 64 } }, "arp stops on release");
        expect (! events.back().on && events.back().time == 7000 && events.back().note == 64, "released note ends at release time");
        expect (balanced (events), "release leaves nothing hanging");
    }

    {
        // Host playing from beat 0.1: first step at once, then on the 1/16 grid (beat 0.25 = sample 3600).
        const auto events = run (make (PatternMode::arpUp), c, 0, 14000, 16000, true, 0.1);
        expect (noteOns (events) == Ons { { 0, 60 }, { 3600, 64 }, { 9600, 67 } }, "arp locks to the host grid");
    }

    {
        // Host playing from beat 0.2: the grid step at 0.25 is too close, so it is skipped.
        const auto events = run (make (PatternMode::arpUp), c, 0, 14000, 16000, true, 0.2);
        expect (noteOns (events) == Ons { { 0, 60 }, { 7200, 64 }, { 13200, 67 } }, "grid step right after the first is skipped");
    }

    {
        const auto events = run (make (PatternMode::arpRandom), c, 0, 59000, 60000);
        bool allInChord = ! events.empty();
        for (const auto& [time, note] : noteOns (events))
            allInChord = allInChord && (note == 60 || note == 64 || note == 67);
        expect (allInChord && noteOns (events).size() == 10, "random arp plays chord notes on every step");
    }

    if (failures == 0)
        std::cout << "All pattern engine tests passed\n";

    return failures == 0 ? 0 : 1;
}
