#pragma once

#include "ChordEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

// MadScaler makes no sound. It turns chord pad clicks (and, optionally, incoming
// MIDI keys) into MIDI chords on its MIDI output, which the DAW routes to any synth.
class MadScalerProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numDegrees = 7;

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

    // Chord for a scale degree with the current key, scale, size and octave.
    madscaler::Chord getChordForDegree (int degree) const;

    int getKey() const;
    int getScaleIndex() const;

    // Bit n is set while the chord on degree n is sounding (from a pad or a MIDI key).
    uint32_t getActiveDegreeMask() const { return activeDegreeMask.load(); }

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
        std::array<int, 4> notes {};
    };

    void startChord (HeldChord& held, int degree, juce::MidiBuffer& out, int samplePos);
    void stopChord (HeldChord& held, juce::MidiBuffer& out, int samplePos);
    void stopAllChords (juce::MidiBuffer& out, int samplePos);
    void updateActiveDegreeMask();

    std::atomic<float>* keyParam = nullptr;
    std::atomic<float>* scaleParam = nullptr;
    std::atomic<float>* chordSizeParam = nullptr;
    std::atomic<float>* octaveParam = nullptr;
    std::atomic<float>* velocityParam = nullptr;
    std::atomic<float>* channelParam = nullptr;
    std::atomic<float>* keyTriggerParam = nullptr;

    // Pad presses travel from the message thread to the audio thread through a
    // lock-free single-producer single-consumer queue.
    struct PadEvent
    {
        int degree = 0;
        bool pressed = false;
    };

    juce::AbstractFifo padFifo { 256 };
    std::array<PadEvent, 256> padEvents {};

    std::array<HeldChord, numDegrees> padChords {};
    std::array<HeldChord, 128> keyChords {}; // indexed by the incoming MIDI note
    std::array<std::array<int, 128>, 16> noteRefCount {};
    std::atomic<uint32_t> activeDegreeMask { 0 };
    juce::MidiBuffer outputMidi;
    bool needsAllNotesOff = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MadScalerProcessor)
};
