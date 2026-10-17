#pragma once

#include "ChordColours.h"
#include "ChordDetector.h"
#include "ChordLibrary.h"
#include "MelodyMaker.h"
#include "ChordEngine.h"
#include "PatternEngine.h"
#include "Progression.h"
#include "Voicing.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <optional>

// MadScaler makes no sound. It turns chord pad clicks (and, optionally, incoming
// MIDI keys) into MIDI chords on its MIDI output, which the DAW routes to any synth.
// It also listens to the MIDI it receives so the editor can name the chords played.
class MadScalerProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numDegrees = 7;
    static constexpr int maxDetectedChords = 8;
    static constexpr int maxChordNotes = 8;

    MadScalerProcessor();
    ~MadScalerProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Called from the editor (message thread) when a chord pad is pressed or released.
    void setPadPressed (int degree, bool pressed);

    // Called from the editor when a key of the on-screen keyboard is pressed or
    // released. The note is treated exactly like incoming MIDI.
    void setKeyboardNotePressed (int midiNote, bool pressed);

    // Called from the editor when a detected chord is pressed or released.
    void setDetectedChordPressed (int slot, const std::vector<int>& notes, bool pressed);

    // Called from the editor when a colour chord (or Circle of Fifths chord) is
    // pressed or released. id is any number the editor uses to light the chip.
    void setColourChordPressed (int id, const std::vector<int>& notes, bool pressed);

    // id of the colour chord sounding now, or -1.
    int getActiveColourChord() const { return activeColourChord.load(); }

    // Scale degree of the last chord played from a pad or MIDI key, or -1.
    int getLastPlayedDegree() const { return lastPlayedDegree.load(); }

    // Colours for a degree's chord, with the current key, scale, size, octave and
    // voicing applied. previousNotes is used for voice leading.
    std::vector<madscaler::ColourGroup> getColoursForDegree (int degree, const std::vector<int>& previousNotes = {}) const;

    // A Circle of Fifths triad in the current octave, voiced like the pads.
    madscaler::ColourChord getCircleChord (int position, bool minor, const std::vector<int>& previousNotes = {}) const;

    // A library progression in the current key and octave, voiced with the
    // current voicing and always voice-led so the chords flow smoothly.
    madscaler::Progression buildFromLibrary (const madscaler::LibraryProgression& entry) const;

    // MelodyMaker: the chords sent to it, its settings and the melody it made.
    // Message thread only; change the melody through setMelodyState so the
    // audio thread gets a copy.
    struct MelodyState
    {
        std::vector<madscaler::ProgressionSlot> chords;
        madscaler::MelodySettings settings;
        std::vector<madscaler::TimedNote> notes;
        double totalBeats = 0.0;
    };

    const MelodyState& getMelodyState() const { return melodyState; }
    void setMelodyState (const MelodyState& newState);
    // Goes up on every setMelodyState, so the editor can notice a state load.
    int getMelodyStateVersion() const { return melodyStateVersion; }

    // While on, the melody plays along with the progression on the melody channel.
    void setMelodyPlaying (bool shouldPlay) { melodyPlaying.store (shouldPlay); }
    bool isMelodyPlaying() const { return melodyPlaying.load(); }

    // Plays one note on the melody channel, to hear a note while editing it.
    void setMelodyNotePressed (int midiNote, bool pressed);

    // MIDI notes currently held on MadScaler's input (keyboard, DAW track or
    // on-screen keys), excluding keys used to trigger chords. Safe to call from any thread.
    std::vector<int> getHeldInputNotes() const;

    // Chord detection history, newest last. Message thread only.
    std::vector<madscaler::DetectedChord> detectedChords;

    // The progression being built. Read it on the message thread; change it
    // only through setProgression so the audio thread gets a copy.
    const madscaler::Progression& getProgression() const { return progression; }
    void setProgression (const madscaler::Progression& newProgression);

    // Plays the progression in a loop: locked to the DAW's position while it
    // plays, otherwise from the start at the DAW's tempo.
    void setProgressionPlaying (bool shouldPlay) { progressionPlaying.store (shouldPlay); }
    bool isProgressionPlaying() const { return progressionPlaying.load(); }

    // Index of the progression chord sounding now, or -1.
    int getCurrentProgressionSlot() const { return currentProgressionSlot.load(); }

    // The pattern currently selected, or nullopt when Pattern is Off.
    std::optional<madscaler::PatternSettings> getPatternSettings() const;
    double getLastBpm() const { return lastBpm.load(); }

    // Chord for a scale degree with the current key, scale, size, octave and
    // voicing. With voice leading on, previousNotes is the chord it moves from.
    madscaler::Chord getChordForDegree (int degree, const std::vector<int>& previousNotes = {}) const;

    madscaler::VoicingSettings getVoicingSettings() const;

    int getKey() const;
    int getScaleIndex() const;

    // Bit n is set while the chord on degree n is sounding (from a pad or a MIDI key).
    uint32_t getActiveDegreeMask() const { return activeDegreeMask.load(); }

    // Bit n is set while detected chord slot n is sounding.
    uint32_t getActiveDetectedMask() const { return activeDetectedMask.load(); }

    juce::AudioProcessorValueTreeState parameters;

    // Mirrors the notes MadScaler is sending out, for the on-screen keyboard.
    juce::MidiKeyboardState outputKeyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct HeldChord
    {
        bool active = false;
        int channel = 1;
        int degree = 0;
        int numNotes = 0;
        std::array<int, maxChordNotes> notes {};
        uint32_t serial = 0; // order of pressing, so the newest chord drives the pattern
    };

    void startChord (HeldChord& held, int degree, juce::MidiBuffer& out, int samplePos);
    void startNotes (HeldChord& held, const int* notes, int numNotes, juce::MidiBuffer& out, int samplePos);
    void stopChord (HeldChord& held, juce::MidiBuffer& out, int samplePos);
    void stopAllChords (juce::MidiBuffer& out, int samplePos);
    void updateActiveDegreeMask();
    void beginPatternBlock (int numSamples, juce::MidiBuffer& out);
    void syncPattern (int samplePos);
    void stopPattern (int samplePos);
    void flushPatternEvents (juce::MidiBuffer& out);
    void readPlayHead();
    void playProgression (int numSamples, juce::MidiBuffer& out);
    void playMelody (double startBeat, double endBeat, double beatsPerSample, int numSamples, juce::MidiBuffer& out);
    void stopMelodyNotes (juce::MidiBuffer& out, int samplePos);
    int applyKeysLock (const juce::MidiMessage& message, juce::MidiMessage& result);

    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* chordSizeParam = nullptr;
    std::atomic<float>* octaveParam = nullptr;
    std::atomic<float>* velocityParam = nullptr;
    std::atomic<float>* channelParam = nullptr;
    std::atomic<float>* keyTriggerParam = nullptr;
    std::atomic<float>* inversionParam = nullptr;
    std::atomic<float>* voicingParam = nullptr;
    std::atomic<float>* bassParam = nullptr;
    std::atomic<float>* voiceLeadingParam = nullptr;
    std::atomic<float>* patternParam = nullptr;
    std::atomic<float>* rateParam = nullptr;
    std::atomic<float>* gateParam = nullptr;
    std::atomic<float>* arpOctavesParam = nullptr;
    std::atomic<float>* strumParam = nullptr;
    std::atomic<float>* keysLockParam = nullptr;
    std::atomic<float>* melodyChannelParam = nullptr;

    // Clicks in the editor travel from the message thread to the audio thread
    // through a lock-free single-producer single-consumer queue.
    struct UiEvent
    {
        enum class Kind { pad, keyboardNote, detectedChord, colourChord, melodyNote };

        Kind kind = Kind::pad;
        int index = 0; // degree, MIDI note, detected chord slot or colour chord id
        bool pressed = false;
        int numNotes = 0;
        std::array<int, maxChordNotes> notes {};
    };

    void pushUiEvent (const UiEvent& event);
    void setHeldInputNote (int midiNote, bool held);

    juce::AbstractFifo uiFifo { 256 };
    std::array<UiEvent, 256> uiEvents {};

    std::array<HeldChord, numDegrees> padChords {};
    std::array<HeldChord, maxDetectedChords> detectedChordsHeld {};
    std::array<HeldChord, 128> keyChords {}; // indexed by the incoming MIDI note
    std::array<std::array<int, 128>, 16> noteRefCount {};
    std::atomic<uint32_t> activeDegreeMask { 0 };
    std::atomic<uint32_t> activeDetectedMask { 0 };
    HeldChord colourHeld;
    int colourHeldId = -1;
    std::atomic<int> activeColourChord { -1 };
    std::atomic<int> lastPlayedDegree { -1 };
    std::array<std::atomic<uint32_t>, 4> heldInputNotes {}; // 128 bits, one per MIDI note
    juce::MidiBuffer outputMidi;
    std::vector<int> lastChordNotes; // audio thread only, for voice leading
    bool needsAllNotesOff = false;

    // Patterns (audio thread only). With a pattern selected, held chords do not
    // sound directly: the newest one is handed to the pattern engine instead.
    madscaler::PatternEngine patternEngine;
    std::vector<madscaler::PatternEvent> patternEvents;
    bool patternActive = false;
    int lastPatternChoice = -1;
    int patternChannel = 1;
    uint32_t chordSerialCounter = 0;
    uint32_t patternSerial = 0;
    double currentSampleRate = 44100.0;

    // Host transport, read once per block.
    double hostBpm = 120.0;
    double hostPpq = 0.0;
    bool hostPlaying = false;
    std::atomic<double> lastBpm { 120.0 };

    // Progression: edited on the message thread, copied for the audio thread
    // into fixed-size storage under a spin lock (the audio thread only try-locks).
    struct ProgressionSnapshot
    {
        struct Slot
        {
            int numNotes = 0;
            std::array<int, maxChordNotes> notes {};
            double lengthBeats = 4.0;
        };

        int numSlots = 0;
        std::array<Slot, madscaler::Progression::maxSlots> slots {};
        double totalBeats = 0.0;

        int slotAt (double beat) const;
    };

    madscaler::Progression progression;
    juce::SpinLock progressionLock;
    ProgressionSnapshot pendingSnapshot;
    std::atomic<bool> snapshotChanged { false };
    ProgressionSnapshot audioSnapshot;
    std::atomic<bool> progressionPlaying { false };
    std::atomic<int> currentProgressionSlot { -1 };
    bool wasProgressionPlaying = false;
    double freeProgressionBeats = 0.0;
    int playingSlot = -1;
    std::array<HeldChord, 2> progressionHeld {};
    int progressionHolder = 0;

    // Melody playback, swapped in like the progression snapshot.
    struct MelodySnapshot
    {
        static constexpr int maxNotes = 512;

        struct Note
        {
            double start = 0.0, length = 0.0;
            int note = 60, velocity = 100;
        };

        int numNotes = 0;
        std::array<Note, maxNotes> notes {};
        double totalBeats = 0.0;
    };

    struct ActiveMelodyNote
    {
        int note = 0, channel = 1;
        double endBeat = 0.0;
    };

    MelodyState melodyState;
    int melodyStateVersion = 0;
    MelodySnapshot pendingMelody, audioMelody;
    std::atomic<bool> melodyChanged { false };
    std::atomic<bool> melodyPlaying { true };
    std::array<ActiveMelodyNote, 32> activeMelodyNotes {};
    int numActiveMelodyNotes = 0;
    double lastMelodyEndBeat = -1.0;

    // Keys Lock: the scale note each held input note was moved to, per channel.
    std::array<std::array<int8_t, 128>, 16> keysLockMap {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MadScalerProcessor)
};
