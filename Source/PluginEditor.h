#pragma once

#include "PluginProcessor.h"

#include <array>
#include <functional>
#include <memory>
#include <set>

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
        void setSelected (bool isSelected);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        // Called on a left click, to select the slot for replacing.
        std::function<void (int index)> onClicked;
        // Called from the menu's "Replace with" items.
        std::function<void (int index, const madscaler::Chord&)> onReplace;

    private:
        void showMenu();

        static constexpr int auditionId = 300; // colour chord ids used to preview slots

        MadScalerProcessor& processor;
        const int index;
        bool hasSlot = false, nextEmpty = false, current = false, selected = false, auditioning = false;
        madscaler::ProgressionSlot slot;
    };

    // A label you drag into the DAW to drop a MIDI clip. writeFile writes the
    // clip to the file it is given and returns it, or an empty File on failure.
    class MidiDragHandle : public juce::Label
    {
    public:
        MidiDragHandle (const juce::String& text, const juce::String& fileName, const juce::String& tooltip);

        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;

        std::function<juce::File (const juce::File&)> writeFile;

    private:
        juce::String fileName;
        bool dragging = false;
    };

    // Shows the progression, plays it in a loop and exports it as MIDI.
    class ProgressionStrip : public juce::Component
    {
    public:
        explicit ProgressionStrip (MadScalerProcessor& processor);

        bool isAddMode() const { return addButton.getToggleState(); }

        // A chord was pressed somewhere (pad, colour, circle, detected chip):
        // it replaces the selected slot, or with "Add chords" on goes at the end.
        void chordPressed (const std::string& name, const std::vector<int>& notes);
        bool wantsChords() const { return isAddMode() || selected >= 0; }

        void loadProgression (const madscaler::Progression& progression, bool append);

        // The chord a new or replaced chord follows, for voice leading.
        std::vector<int> contextNotes() const;

        // Called regularly from the editor's timer.
        void update();
        void refresh();

        void paint (juce::Graphics&) override;
        void resized() override;

        // Called by the "To Melody" button.
        std::function<void()> onSendToMelody;

    private:
        juce::File writeMidiFile (const juce::File& file) const;
        void exportMidi();
        void select (int index);

        MadScalerProcessor& processor;
        int selected = -1;
        juce::Label titleLabel, hintLabel;
        juce::TextButton addButton { "Add chords" }, playButton { "Play" }, clearButton { "Clear" },
                         exportButton { "Export MIDI" }, melodyButton { "To Melody" };
        MidiDragHandle dragHandle { "Drag MIDI", "MadScaler Progression.mid", "Drag the progression into your DAW as a MIDI clip" };
        std::array<std::unique_ptr<ProgressionSlotView>, madscaler::Progression::maxSlots> slots;
        std::unique_ptr<juce::FileChooser> chooser;
        size_t lastSize = 0;
        int lastCurrent = -2;
    };

    // Browses MadScaler's progression library by style, mood and use, and loads
    // a progression into the strip in the current key.
    class LibraryPanel : public juce::Component
    {
    public:
        LibraryPanel (MadScalerProcessor& processor, ProgressionStrip& strip);

        void refresh(); // key or voicing changed

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        struct TagRow
        {
            juce::String text;
            bool header = false;
        };

        class TagModel : public juce::ListBoxModel
        {
        public:
            explicit TagModel (LibraryPanel& o) : owner (o) {}
            int getNumRows() override { return (int) owner.tagRows.size(); }
            void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
            void selectedRowsChanged (int lastRowSelected) override;

        private:
            LibraryPanel& owner;
        };

        class ProgressionModel : public juce::ListBoxModel
        {
        public:
            explicit ProgressionModel (LibraryPanel& o) : owner (o) {}
            int getNumRows() override { return (int) owner.matches.size(); }
            void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
            void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
            void selectedRowsChanged (int lastRowSelected) override;

        private:
            LibraryPanel& owner;
        };

        void showTag (const juce::String& tag);
        void load (bool append);
        const madscaler::LibraryProgression* selectedProgression() const;
        juce::String chordNamesInKey (const madscaler::LibraryProgression& progression) const;

        MadScalerProcessor& processor;
        ProgressionStrip& strip;
        std::vector<TagRow> tagRows;
        std::vector<const madscaler::LibraryProgression*> matches;
        TagModel tagModel { *this };
        ProgressionModel progressionModel { *this };
        juce::ListBox tagList, progressionList;
        juce::Label detailLabel;
        juce::TextButton loadButton { "Load" }, appendButton { "Append" };
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

    // Draws the melody as a small piano roll over the chords it was made for.
    class MelodyRoll : public juce::Component
    {
    public:
        void setMelody (const MadScalerProcessor::MelodyState& state, bool bass = false);
        void paint (juce::Graphics&) override;

    private:
        std::vector<madscaler::ProgressionSlot> chords;
        std::vector<madscaler::TimedNote> notes;
        double totalBeats = 0.0;
        int lowNote = 60, highNote = 79;
    };

    // MelodyMaker: takes the progression's chords, suggests scales that fit
    // them and invents melodies or arpeggios in rhythmic patterns over them.
    class MelodyPanel : public juce::Component
    {
    public:
        explicit MelodyPanel (MadScalerProcessor& processor);

        // Takes the chords of the current progression.
        void takeChords();

        // Called by the "Edit notes" button.
        std::function<void()> onEditNotes;

        // Called regularly from the editor's timer.
        void update();

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class ScaleModel : public juce::ListBoxModel
        {
        public:
            explicit ScaleModel (MelodyPanel& o) : owner (o) {}
            int getNumRows() override { return (int) owner.fits.size(); }
            void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
            void selectedRowsChanged (int lastRowSelected) override;

        private:
            MelodyPanel& owner;
        };

        void loadFromProcessor();
        void readControls (madscaler::MelodySettings& settings) const;
        void regenerate (bool newSeed);
        void updateEnablement();
        juce::File writeMidiFile (const juce::File& file) const;
        void exportMidi();

        MadScalerProcessor& processor;
        std::vector<madscaler::ScaleFit> fits;
        ScaleModel scaleModel { *this };
        juce::ListBox scaleList;
        juce::Label titleLabel, channelLabel;
        juce::ComboBox modeBox, rhythmBox, arpBox, densityBox, rangeBox;
        juce::ComboBox octavesBox, notesBox, variationBox, gateBox, accentBox, swingBox, humaniseBox;
        juce::ToggleButton bassButton { "Bass" }, flowButton { "Keep flowing" };
        juce::Slider channelSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> channelAttachment;
        juce::TextButton getChordsButton { "Get chords" }, newButton { "New melody" }, playButton { "Play along" },
                         exportButton { "Export" }, editButton { "Edit notes" };
        MidiDragHandle dragHandle { "Drag melody", "MadScaler Melody.mid", "Drag the melody into your DAW as a MIDI clip" };
        MelodyRoll roll;
        std::unique_ptr<juce::FileChooser> chooser;
        int lastVersion = -1;
        bool loading = false;
    };

    // Sends the progression's chords to the MelodyMaker and the bass, picking
    // the scale that fits best and making a melody and a bassline.
    void sendChordsToMelody (MadScalerProcessor& processor);

    // A bassline over the chords sent to the MelodyMaker, on its own channel.
    class BassPanel : public juce::Component
    {
    public:
        explicit BassPanel (MadScalerProcessor& processor);

        // Called regularly from the editor's timer.
        void update();

        // Called by the "Edit notes" button.
        std::function<void()> onEditNotes;

        void paint (juce::Graphics&) override;
        void resized() override;

    private:
        class StyleModel : public juce::ListBoxModel
        {
        public:
            explicit StyleModel (BassPanel& o) : owner (o) {}
            int getNumRows() override { return (int) madscaler::getBassStyleNames().size(); }
            void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
            void selectedRowsChanged (int lastRowSelected) override;

        private:
            BassPanel& owner;
        };

        void loadFromProcessor();
        void regenerate (bool newSeed);
        juce::File writeMidiFile (const juce::File& file) const;
        void exportMidi();

        MadScalerProcessor& processor;
        StyleModel styleModel { *this };
        juce::ListBox styleList;
        juce::Label titleLabel, channelLabel, hintLabel;
        juce::ComboBox rangeBox, gateBox, swingBox, humaniseBox;
        juce::ToggleButton leadInButton { "Lead-in notes" };
        juce::Slider channelSlider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> channelAttachment;
        juce::TextButton getChordsButton { "Get chords" }, newButton { "New bass" }, playButton { "Play along" },
                         exportButton { "Export" }, editButton { "Edit notes" };
        MidiDragHandle dragHandle { "Drag bass", "MadScaler Bass.mid", "Drag the bassline into your DAW as a MIDI clip" };
        MelodyRoll roll;
        std::unique_ptr<juce::FileChooser> chooser;
        int lastVersion = -1;
        bool loading = false;
    };

    // A piano roll for changing the melody's notes one by one: drag to move,
    // drag the right edge to stretch, double-click to add or delete, and set
    // each note's velocity in the lane underneath.
    class NoteEditor : public juce::Component
    {
    public:
        explicit NoteEditor (MadScalerProcessor& processor);
        ~NoteEditor() override;

        // Edits the bassline instead of the melody.
        void setPart (bool bass);

        // Called regularly from the editor's timer.
        void update();

        std::function<void()> onDone;

        void paint (juce::Graphics&) override;
        void resized() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        bool keyPressed (const juce::KeyPress&) override;
        void visibilityChanged() override;

    private:
        enum class DragMode { none, move, resize, velocity };

        juce::Rectangle<int> gridArea() const;
        juce::Rectangle<int> velocityArea() const;
        double gridStep() const;
        double snapBeat (double beat) const;
        double xToBeat (float x) const;
        float beatToX (double beat) const;
        int yToNote (float y) const;
        float noteToY (int note) const;
        juce::Rectangle<float> noteBounds (const madscaler::TimedNote& note) const;
        int noteAt (juce::Point<float> position) const;
        static bool isOnEdge (juce::Rectangle<float> noteBounds, float x);
        int snapPitch (int note, int direction) const;
        bool inScale (int note) const;
        void updateRange();
        const std::vector<madscaler::TimedNote>& partNotes (const MadScalerProcessor::MelodyState& state) const;

        void pushUndo();
        void commit();            // sends the edited notes to the processor
        void undo();
        void redo();
        void deleteSelected();
        void transposeSelected (int semitones);
        void showNoteMenu (int index);
        void audition (int note);
        void stopAudition();

        MadScalerProcessor& processor;
        juce::Label titleLabel, hintLabel;
        juce::ComboBox snapBox, partBox;
        juce::ToggleButton scaleButton { "Scale notes only" };
        bool bassPart = false;
        juce::TextButton undoButton { "Undo" }, redoButton { "Redo" }, doneButton { "Done" };

        std::vector<madscaler::TimedNote> notes;
        std::vector<std::vector<madscaler::TimedNote>> undoStack, redoStack;
        std::set<int> selected;
        int lowNote = 48, highNote = 84;
        int lastVersion = -1;

        DragMode dragMode = DragMode::none;
        std::vector<madscaler::TimedNote> dragStartNotes;
        int dragNote = -1;
        bool dragChanged = false;
        int auditionNote = -1;
        static constexpr int auditionId = 400;
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
    void showTab (int tab);

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
    juce::TextButton coloursTab { "Colours" }, libraryTab { "Library" }, detectTab { "Detect" }, melodyTab { "Melody" },
                     bassTab { "Bass" };
    int lastSuggestedDegree = -2;
    madscaler::ui::ProgressionStrip progressionStrip;
    madscaler::ui::LibraryPanel libraryPanel;
    madscaler::ui::MelodyPanel melodyPanel;
    madscaler::ui::BassPanel bassPanel;
    madscaler::ui::NoteEditor noteEditor;
    madscaler::ui::ScaleKeyboard keyboard;

    std::array<float, 7> lastChordSettings { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MadScalerEditor)
};
