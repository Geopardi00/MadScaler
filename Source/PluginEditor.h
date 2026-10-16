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
    class ScaleKeyboard : public juce::MidiKeyboardComponent
    {
    public:
        ScaleKeyboard (MadScalerProcessor& processor);

    private:
        void drawWhiteNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                            bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour) override;
        void drawBlackNote (int midiNoteNumber, juce::Graphics&, juce::Rectangle<float> area,
                            bool isDown, bool isOver, juce::Colour noteFillColour) override;

        bool inScale (int midiNote) const;

        MadScalerProcessor& processor;
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
    juce::ComboBox keyBox, scaleBox, sizeBox;
    juce::Slider octaveSlider, velocitySlider, channelSlider;
    juce::ToggleButton keyTriggerButton { "Play chords from MIDI keys (C-B = I-VII)" };
    juce::Label scaleNotesLabel;

    std::unique_ptr<ComboAttachment> keyAttachment, scaleAttachment, sizeAttachment;
    std::unique_ptr<SliderAttachment> octaveAttachment, velocityAttachment, channelAttachment;
    std::unique_ptr<ButtonAttachment> keyTriggerAttachment;

    std::array<std::unique_ptr<madscaler::ui::ChordPad>, MadScalerProcessor::numDegrees> pads;
    madscaler::ui::ScaleKeyboard keyboard;

    std::array<float, 4> lastChordSettings { -1.0f, -1.0f, -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MadScalerEditor)
};
