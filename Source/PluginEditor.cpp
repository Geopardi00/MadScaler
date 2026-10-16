#include "PluginEditor.h"

namespace
{
    namespace Colours
    {
        const juce::Colour background { 0xff15171c };
        const juce::Colour panel { 0xff20232b };
        const juce::Colour pad { 0xff2b2f3a };
        const juce::Colour padLit { 0xffe0613a };
        const juce::Colour accent { 0xfff0a04b };
        const juce::Colour text { 0xffeceef2 };
        const juce::Colour textDim { 0xff9097a6 };
        const juce::Colour scaleKey { 0xfff6e3cf };
    }
}

namespace madscaler::ui
{
    ChordPad::ChordPad (MadScalerProcessor& p, int d) : processor (p), degree (d) {}

    void ChordPad::setChord (const madscaler::Chord& newChord)
    {
        chord = newChord;
        repaint();
    }

    void ChordPad::setLit (bool shouldBeLit)
    {
        if (lit != shouldBeLit)
        {
            lit = shouldBeLit;
            repaint();
        }
    }

    void ChordPad::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat().reduced (4.0f);

        g.setColour (lit ? Colours::padLit : Colours::pad);
        g.fillRoundedRectangle (area, 10.0f);

        if (isMouseOver())
        {
            g.setColour (Colours::accent.withAlpha (0.6f));
            g.drawRoundedRectangle (area, 10.0f, 2.0f);
        }

        auto content = area.reduced (8.0f);

        g.setColour (lit ? Colours::text : Colours::textDim);
        g.setFont (juce::FontOptions (15.0f));
        g.drawFittedText (juce::String::fromUTF8 (chord.roman.c_str()),
                          content.removeFromTop (22.0f).toNearestInt(), juce::Justification::centred, 1);

        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
        g.drawFittedText (juce::String::fromUTF8 (chord.name.c_str()),
                          content.removeFromTop (content.getHeight() - 22.0f).toNearestInt(),
                          juce::Justification::centred, 1);

        juce::StringArray noteNames;
        for (auto note : chord.notes)
            noteNames.add (juce::String (madscaler::getNoteName (note)));

        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (noteNames.joinIntoString (" "), content.toNearestInt(), juce::Justification::centred, 1);
    }

    void ChordPad::mouseDown (const juce::MouseEvent&)
    {
        held = true;
        processor.setPadPressed (degree, true);
    }

    void ChordPad::mouseUp (const juce::MouseEvent&)
    {
        if (held)
        {
            held = false;
            processor.setPadPressed (degree, false);
        }
    }

    ScaleKeyboard::ScaleKeyboard (MadScalerProcessor& p)
        : juce::MidiKeyboardComponent (p.outputKeyboardState, juce::KeyboardComponentBase::horizontalKeyboard),
          processor (p)
    {
        // Display only: the pads are how chords are played.
        setInterceptsMouseClicks (false, false);
        setWantsKeyboardFocus (false);
        setKeyPressBaseOctave (10);
        setAvailableRange (24, 107);
        setOctaveForMiddleC (4); // MIDI note 60 is labelled C4, matching the Octave control
        setColour (keyDownOverlayColourId, Colours::padLit);
    }

    bool ScaleKeyboard::inScale (int midiNote) const
    {
        return madscaler::isInScale (midiNote, processor.getKey(), processor.getScaleIndex());
    }

    void ScaleKeyboard::drawWhiteNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                       bool isDown, bool isOver, juce::Colour lineColour, juce::Colour textColour)
    {
        g.setColour (inScale (midiNoteNumber) ? Colours::scaleKey : juce::Colours::white.darker (0.35f));
        g.fillRect (area);
        juce::MidiKeyboardComponent::drawWhiteNote (midiNoteNumber, g, area, isDown, isOver,
                                                    lineColour, textColour);
    }

    void ScaleKeyboard::drawBlackNote (int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                       bool isDown, bool isOver, juce::Colour)
    {
        const auto fill = isDown ? Colours::padLit
                                 : (inScale (midiNoteNumber) ? juce::Colour (0xff6b4a2e) : juce::Colour (0xff111111));
        juce::MidiKeyboardComponent::drawBlackNote (midiNoteNumber, g, area, false, isOver, fill);
    }
} // namespace madscaler::ui

MadScalerEditor::MadScalerEditor (MadScalerProcessor& p)
    : AudioProcessorEditor (&p), processor (p), keyboard (p)
{
    auto& lf = getLookAndFeel();
    lf.setColour (juce::ComboBox::backgroundColourId, Colours::panel);
    lf.setColour (juce::ComboBox::outlineColourId, Colours::pad);
    lf.setColour (juce::ComboBox::arrowColourId, Colours::accent);
    lf.setColour (juce::PopupMenu::backgroundColourId, Colours::panel);
    lf.setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::padLit);
    lf.setColour (juce::Slider::textBoxOutlineColourId, Colours::pad);
    lf.setColour (juce::Slider::textBoxBackgroundColourId, Colours::panel);
    lf.setColour (juce::ToggleButton::tickColourId, Colours::accent);

    auto setUpLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, Colours::textDim);
        label.setFont (juce::FontOptions (13.0f));
        addAndMakeVisible (label);
    };

    setUpLabel (keyLabel, "Key");
    setUpLabel (scaleLabel, "Scale");
    setUpLabel (sizeLabel, "Chords");
    setUpLabel (octaveLabel, "Octave");
    setUpLabel (velocityLabel, "Velocity");
    setUpLabel (channelLabel, "MIDI Ch");

    auto setUpCombo = [this] (juce::ComboBox& box, const juce::String& paramID,
                              std::unique_ptr<ComboAttachment>& attachment)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (processor.parameters.getParameter (paramID)))
            box.addItemList (choice->choices, 1);
        addAndMakeVisible (box);
        attachment = std::make_unique<ComboAttachment> (processor.parameters, paramID, box);
    };

    setUpCombo (keyBox, "key", keyAttachment);
    setUpCombo (scaleBox, "scale", scaleAttachment);
    setUpCombo (sizeBox, "chordSize", sizeAttachment);

    auto setUpSlider = [this] (juce::Slider& slider, const juce::String& paramID,
                               std::unique_ptr<SliderAttachment>& attachment)
    {
        slider.setSliderStyle (juce::Slider::IncDecButtons);
        slider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 24);
        addAndMakeVisible (slider);
        attachment = std::make_unique<SliderAttachment> (processor.parameters, paramID, slider);
    };

    setUpSlider (octaveSlider, "octave", octaveAttachment);
    setUpSlider (velocitySlider, "velocity", velocityAttachment);
    setUpSlider (channelSlider, "channel", channelAttachment);

    keyTriggerButton.setColour (juce::ToggleButton::textColourId, Colours::textDim);
    addAndMakeVisible (keyTriggerButton);
    keyTriggerAttachment = std::make_unique<ButtonAttachment> (processor.parameters, "keyTrigger", keyTriggerButton);

    scaleNotesLabel.setColour (juce::Label::textColourId, Colours::text);
    scaleNotesLabel.setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (scaleNotesLabel);

    for (int i = 0; i < MadScalerProcessor::numDegrees; ++i)
    {
        pads[(size_t) i] = std::make_unique<madscaler::ui::ChordPad> (processor, i);
        addAndMakeVisible (*pads[(size_t) i]);
    }

    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (720, 400, 1600, 900);
    setSize (860, 460);
    startTimerHz (30);
}

MadScalerEditor::~MadScalerEditor()
{
    // Release any pad that is still held when the window closes.
    for (int i = 0; i < MadScalerProcessor::numDegrees; ++i)
        processor.setPadPressed (i, false);
}

void MadScalerEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::background);

    auto header = getLocalBounds().removeFromTop (44);
    g.setColour (Colours::panel);
    g.fillRect (header);

    g.setColour (Colours::accent);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("MadScaler", header.reduced (16, 0), juce::Justification::centredLeft);

    g.setColour (Colours::textDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("MIDI chords out, route to any synth", header.reduced (16, 0), juce::Justification::centredRight);
}

void MadScalerEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (44);
    area.reduce (12, 8);

    // Controls row: label above each control.
    auto controls = area.removeFromTop (52);
    auto place = [&controls] (juce::Label& label, juce::Component& control, int width)
    {
        auto column = controls.removeFromLeft (width);
        controls.removeFromLeft (10);
        label.setBounds (column.removeFromTop (20));
        control.setBounds (column.removeFromTop (28));
    };

    place (keyLabel, keyBox, 70);
    place (scaleLabel, scaleBox, 170);
    place (sizeLabel, sizeBox, 110);
    place (octaveLabel, octaveSlider, 100);
    place (velocityLabel, velocitySlider, 100);
    place (channelLabel, channelSlider, 100);

    auto secondRow = area.removeFromTop (30);
    scaleNotesLabel.setBounds (secondRow.removeFromLeft (secondRow.getWidth() / 2));
    keyTriggerButton.setBounds (secondRow);

    area.removeFromTop (6);
    keyboard.setBounds (area.removeFromBottom (80));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 49.0f); // 49 white keys from C1 to B7
    area.removeFromBottom (8);

    const auto padWidth = area.getWidth() / MadScalerProcessor::numDegrees;
    for (auto& pad : pads)
        pad->setBounds (area.removeFromLeft (padWidth));
}

void MadScalerEditor::refreshChords()
{
    for (int i = 0; i < MadScalerProcessor::numDegrees; ++i)
        pads[(size_t) i]->setChord (processor.getChordForDegree (i));

    juce::StringArray names;
    for (auto pc : madscaler::getScalePitchClasses (processor.getKey(), processor.getScaleIndex()))
        names.add (madscaler::getNoteName (pc));

    scaleNotesLabel.setText ("Scale notes: " + names.joinIntoString ("  "), juce::dontSendNotification);
    keyboard.repaint();
}

void MadScalerEditor::timerCallback()
{
    // Rebuild the pads only when a setting that changes the chords moves.
    std::array<float, 4> settings {};
    size_t index = 0;
    for (auto* id : { "key", "scale", "chordSize", "octave" })
        settings[index++] = processor.parameters.getRawParameterValue (id)->load();

    if (settings != lastChordSettings)
    {
        lastChordSettings = settings;
        refreshChords();
    }

    const auto mask = processor.getActiveDegreeMask();
    for (int i = 0; i < MadScalerProcessor::numDegrees; ++i)
        pads[(size_t) i]->setLit ((mask >> i) & 1u);
}
