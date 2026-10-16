#pragma once

#include "PluginProcessor.h"

#include <array>
#include <functional>
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

        // Marks the pad as a likely next chord: 0 = most likely, -1 = not suggested.
        void setSuggestion (int rank);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        // Called when the pad is pressed, with its scale degree.
        std::function<void (int)> onPressed;

    private:
        MadScalerProcessor& processor;
        const int degree;
        madscaler::Chord chord;
        bool lit = false;
        bool held = false;
        int suggestion = -1;
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

        // Called when a chip holding a chord is pressed.
        std::function<void (const madscaler::DetectedChord&)> onPressed;

    private:
        MadScalerProcessor& processor;
        const int slot;
        bool hasChord = false;
        madscaler::DetectedChord chord;
        bool lit = false;
        bool held = false;
    };

    // A colour chord (extension, borrowed or secondary chord). Holding the mouse
    // down plays it.
    class ColourChip : public juce::Component
    {
    public:
        ColourChip (MadScalerProcessor& processor, int id);

        void setChord (const madscaler::ColourChord* chord);
        void setLit (bool shouldBeLit);
        int getId() const { return id; }

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void (int id, const madscaler::ColourChord&)> onPressed;

    private:
        MadScalerProcessor& processor;
        const int id;
        bool hasChord = false;
        madscaler::ColourChord chord;
        bool lit = false;
        bool held = false;
    };

    // Circle of Fifths: major chords on the outer ring, relative minors inside.
    // Chords of the current scale are highlighted. Click to play, right-click to
    // change key.
    class CircleOfFifths : public juce::Component
    {
    public:
        static constexpr int firstId = 100; // colour chord ids used by the circle

        explicit CircleOfFifths (MadScalerProcessor& processor);

        void refresh();
        void setLitId (int id);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<void (const madscaler::ColourChord&)> onPressed;

    private:
        bool hitTestSegment (juce::Point<float> position, int& segment, bool& minor) const;
        void setKey (int pitchClass, bool minor);

        MadScalerProcessor& processor;
        std::array<bool, 24> inScale {};
        int tonicSegment = -1; // 0-11 major ring, 12-23 minor ring
        int litId = -1;
        int heldId = -1;
    };

    // Colours for the last pad pressed, next to the Circle of Fifths.
    class ColoursPanel : public juce::Component
    {
    public:
        static constexpr int maxChipsPerGroup = 8;
        static constexpr int numGroups = 3;

        explicit ColoursPanel (MadScalerProcessor& processor);

        void setDegree (int degree);
        void refresh();
        void update(); // called from the editor's timer

        void paint (juce::Graphics&) override;
        void resized() override;

        // Called when a chord is pressed, with its name and notes.
        std::function<void (const std::string&, const std::vector<int>&)> onChordPressed;

    private:
        MadScalerProcessor& processor;
        int degree = 0;
        std::vector<madscaler::ColourGroup> groups;
        juce::Label titleLabel, hintLabel;
        std::array<juce::Label, numGroups> groupLabels;
        std::array<std::array<std::unique_ptr<ColourChip>, maxChipsPerGroup>, numGroups> chips;
    public:
        CircleOfFifths circle;
    };

    // One chord of the progression. Right-click for length, move and delete.
    class ProgressionSlotView : public juce::Component
    {
    public:
        ProgressionSlotView (MadScalerProcessor& processor, int index);

        void setSlot (const madscaler::ProgressionSlot* slot, bool isNextEmpty);
        void setCurrent (bool isCurrent);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        void showMenu();

        MadScalerProcessor& processor;
        const int index;
        bool hasSlot = false, nextEmpty = false, current = false;
        madscaler::ProgressionSlot slot;
    };

    // Shows the progression, plays it in a loop and exports it as MIDI.
    class ProgressionStrip : public juce::Component
    {
    public:
        explicit ProgressionStrip (MadScalerProcessor& processor);

        bool isAddMode() const { return addButton.getToggleState(); }
        void addChord (const std::string& name, const std::vector<int>& notes);

        // Notes of the last chord in the progression, for voice leading new ones.
        std::vector<int> lastChordNotes() const;

        // Called regularly from the editor's timer.
        void update();
        void refresh();

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class DragHandle : public juce::Label
        {
        public:
            explicit DragHandle (ProgressionStrip& owner);
            void mouseDrag (const juce::MouseEvent&) override;
            void mouseUp (const juce::MouseEvent&) override;

        private:
            ProgressionStrip& owner;
            bool dragging = false;
        };

        juce::File writeMidiFile (const juce::File& file) const;
        void exportMidi();

        MadScalerProcessor& processor;
        juce::Label titleLabel;
        juce::TextButton addButton { "Add chords" }, playButton { "Play" }, clearButton { "Clear" },
                         exportButton { "Export MIDI" };
        DragHandle dragHandle { *this };
        std::array<std::unique_ptr<ProgressionSlotView>, madscaler::Progression::maxSlots> slots;
        std::unique_ptr<juce::FileChooser> chooser;
        size_t lastSize = 0;
        int lastCurrent = -2;
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
    public:
        std::array<std::unique_ptr<DetectedChordChip>, MadScalerProcessor::maxDetectedChords> chips;

    private:
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
    void showTab (bool colours);

    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    MadScalerProcessor& processor;

    juce::Label keyLabel, scaleLabel, sizeLabel, octaveLabel, velocityLabel, channelLabel;
    juce::Label inversionLabel, voicingLabel;
    juce::Label patternLabel, rateLabel, gateLabel, arpOctavesLabel, strumLabel;
    juce::ComboBox keyBox, scaleBox, sizeBox, inversionBox, voicingBox, patternBox, rateBox;
    juce::Slider gateSlider, arpOctavesSlider, strumSlider;
    juce::ToggleButton bassButton { "Bass note" };
    juce::ToggleButton voiceLeadingButton { "Voice leading" };
    juce::Slider octaveSlider, velocitySlider, channelSlider;
    juce::ToggleButton keyTriggerButton { "Chords from MIDI keys (C-B = I-VII)" };
    juce::ToggleButton keysLockButton { "Keys Lock" };
    juce::Label scaleNotesLabel;

    std::unique_ptr<ComboAttachment> keyAttachment, scaleAttachment, sizeAttachment, inversionAttachment, voicingAttachment,
                                     patternAttachment, rateAttachment;
    std::unique_ptr<SliderAttachment> octaveAttachment, velocityAttachment, channelAttachment,
                                      gateAttachment, arpOctavesAttachment, strumAttachment;
    std::unique_ptr<ButtonAttachment> keyTriggerAttachment, bassAttachment, voiceLeadingAttachment, keysLockAttachment;

    std::array<std::unique_ptr<madscaler::ui::ChordPad>, MadScalerProcessor::numDegrees> pads;
    madscaler::ui::DetectionPanel detectionPanel;
    madscaler::ui::ColoursPanel coloursPanel;
    juce::TextButton coloursTab { "Colours" }, detectTab { "Detect" };
    int lastSuggestedDegree = -2;
    madscaler::ui::ProgressionStrip progressionStrip;
    madscaler::ui::ScaleKeyboard keyboard;

    std::array<float, 7> lastChordSettings { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MadScalerEditor)
};
