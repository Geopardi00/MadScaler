#pragma once

#include <array>
#include <cstdint>
#include <vector>

// Turns a held chord into a performance: a strum, an arpeggio or rhythmic
// pulses, locked to the host tempo. Plain C++ with no JUCE dependency, and no
// allocation while rendering once the output vector has capacity.
namespace madscaler
{
    enum class PatternMode
    {
        strumUp,
        strumDown,
        arpUp,
        arpDown,
        arpUpDown,
        arpRandom,
        pulse
    };

    struct PatternSettings
    {
        PatternMode mode = PatternMode::arpUp;
        double stepBeats = 0.25; // step length in quarter notes, e.g. 0.25 = 1/16
        float gate = 0.8f;       // note length as a fraction of a step
        int octaves = 1;         // arpeggio range
        double strumMs = 30.0;   // delay between strummed notes
        int velocity = 100;
    };

    struct PatternEvent
    {
        int sampleOffset = 0; // within the current block
        bool noteOn = false;
        int note = 0;
        int velocity = 0;
    };

    class PatternEngine
    {
    public:
        static constexpr int maxChordNotes = 8;

        void reset();
        void setSettings (const PatternSettings& newSettings) { settings = newSettings; }

        // Call once at the start of every block. When the host is playing, steps
        // lock to its beat position (ppq); otherwise they run from an internal clock.
        void beginBlock (double sampleRate, double bpm, bool hostPlaying, double ppqAtBlockStart);

        // Changes the chord at a sample offset within the block. The new chord
        // replaces the old one; an empty chord stops the pattern.
        void setChord (const int* notes, int numNotes, int sampleOffset, std::vector<PatternEvent>& out);

        // Renders the rest of the block and advances the clocks.
        void endBlock (int numSamples, std::vector<PatternEvent>& out);

        // Stops every sounding note at a sample offset.
        void allNotesOff (int sampleOffset, std::vector<PatternEvent>& out);

        bool isSounding() const { return numSounding > 0 || numPending > 0; }

    private:
        struct Sounding
        {
            int note = 0;
            int64_t offTime = -1; // -1: until the chord changes
        };

        struct Pending
        {
            int note = 0;
            int64_t onTime = 0;
        };

        void renderTo (int64_t time, std::vector<PatternEvent>& out);
        void emit (int64_t time, bool noteOn, int note, std::vector<PatternEvent>& out);
        void releaseAll (int64_t time, std::vector<PatternEvent>& out);
        void playStep (int64_t time, std::vector<PatternEvent>& out);
        void startStrum (int64_t time);
        bool isStrum() const;
        double beatsAt (int64_t time) const;
        int64_t timeOfBeat (double beat) const;
        int64_t nextStepTime() const;
        int64_t stepIndexAt (int64_t time) const;
        int sequenceLength() const;
        int sequenceNote (int position);

        PatternSettings settings;

        std::array<int, maxChordNotes> chord {};
        int numChordNotes = 0;

        std::array<Sounding, 64> sounding {};
        int numSounding = 0;
        std::array<Pending, maxChordNotes> pending {};
        int numPending = 0;

        int64_t blockStart = 0; // absolute sample time of the current block
        int64_t renderedTo = 0; // absolute time rendered so far
        double sampleRate = 44100.0;
        double samplesPerBeat = 22050.0;
        double beatsAtBlockStart = 0.0;
        double freeBeats = 0.0; // internal beat clock when the host is stopped
        bool hostWasPlaying = false;
        double lastHostBeats = 0.0;

        int64_t lastStepIndex = -1;
        int arpPosition = 0;
        uint32_t randomState = 12345;
    };
} // namespace madscaler
