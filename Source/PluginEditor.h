#pragma once

#include "PluginProcessor.h"

#include <array>
#include <memory>

namespace madscaler::ui
{
    // One clickable chord in the grid. Holding the mouse down holds the chord.
    class ChordPad : public juce::Component
    {
    public:
        explicit ChordPad (MadScalerProcessor& processor, int degree);

        void setChord (const madscaler::Chord& chord);
        void setLit (bool shouldBeLit);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        MadScalerProcessor& processor;
        const int degree;
        madscaler::Chord chord;
        bool lit = false;
        bool held = false;
    };

    // Shows the notes MadScaler is sending out, with the current scale shaded.
    // Clicking a key plays that note as if it came in over MIDI.
    class ScaleKeyboard : public juce::MidiKeyboardComponent
    {
    public:
        ScaleKeyboard (MadScalerProcessor& processor);
        ~ScaleKeyboard() override;

        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        int noteAt (const juce::MouseEvent&);
        void pressNote (int note);
        void releaseNote();

        int heldNote = -1;

        void drawWhiteNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                            bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
        void drawBlackNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                            bool isDown, bool isOver, juce::Colour noteFillColour) override;

        bool inScale (int midiNote) const;

        MadScalerProcessor& processor;
    };

    // A detected chord in the history row. Holding the mouse down replays it.
    class DetectedChordChip : public juce::Component
    {
    public:
        DetectedChordChip (MadScalerProcessor& processor, int slot);

        void setChord (const madscaler::DetectedChord* chord);
        void setLit (bool shouldBeLit);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

    private:
        MadScalerProcessor& processor;
        const int slot;
        bool hasChord = false;
        madscaler::DetectedChord chord;
        bool lit = false;
        bool held = false;
    };

    // Names the chord coming in on MIDI, keeps a history of the chords played and
    // suggests keys and scales that fit them.
    class DetectionPanel : public juce::Component
    {
    public:
        explicit DetectionPanel (MadScalerProcessor& processor);

        // Called regularly from the editor's timer.
        void update();

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        void addToHistory (const madscaler::DetectedChord& chord);
        void refreshHistory();
        void applySuggestion (size_t index);

        MadScalerProcessor& processor;

        juce::Label titleLabel, currentLabel, suggestionsLabel;
        juce::TextButton clearButton { "Clear" };
        std::array<std::unique_ptr<DetectedChordChip>, MadScalerProcessor::maxDetectedChords> chips;
        std::array<juce::TextButton, 3> suggestionButtons;
        std::vector<madscaler::KeySuggestion> suggestions;

        std::vector<int> lastHeldNotes { -1 }; // forces the first update to set the label
        std::string pendingName;
        int stableTicks = 0;
    };
} // namespace madscaler::ui

class MadScalerEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit MadScalerEditor (MadScalerProcessor&);
    ~MadScalerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshChords();

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    MadScalerProcessor& processor;

    juce::Label keyLabel, scaleLabel, sizeLabel, octaveLabel, velocityLabel, channelLabel;
    juce::Label inversionLabel, voicingLabel;
    juce::ComboBox keyBox, scaleBox, sizeBox, inversionBox, voicingBox;
    juce::ToggleButton bassButton { "Bass note" };
    juce::ToggleButton voiceLeadingButton { "Voice leading" };
    juce::Slider octaveSlider, velocitySlider, channelSlider;
    juce::ToggleButton keyTriggerButton { "Play chords from MIDI keys (C-B = I-VII)" };
    juce::Label scaleNotesLabel;

    std::unique_ptr<ComboAttachment> keyAttachment, scaleAttachment, sizeAttachment, inversionAttachment, voicingAttachment;
    std::unique_ptr<SliderAttachment> octaveAttachment, velocityAttachment, channelAttachment;
    std::unique_ptr<ButtonAttachment> keyTriggerAttachment, bassAttachment, voiceLeadingAttachment;

    std::array<std::unique_ptr<madscaler::ui::ChordPad>, MadScalerProcessor::numDegrees> pads;
    madscaler::ui::DetectionPanel detectionPanel;
    madscaler::ui::ScaleKeyboard keyboard;

    std::array<float, 7> lastChordSettings { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MadScalerEditor)
};
