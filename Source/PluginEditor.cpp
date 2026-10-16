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
        setWantsKeyboardFocus (false);
        setKeyPressBaseOctave (10);
        setAvailableRange (24, 107);
        setOctaveForMiddleC (4); // MIDI note 60 is labelled C4, matching the Octave control
        setColour (keyDownOverlayColourId, Colours::padLit);
    }

    ScaleKeyboard::~ScaleKeyboard()
    {
        releaseNote();
    }

    int ScaleKeyboard::noteAt (const juce::MouseEvent& e)
    {
        return getNoteAndVelocityAtPosition (e.position).note;
    }

    void ScaleKeyboard::pressNote (int note)
    {
        if (note == heldNote)
            return;

        releaseNote();
        if (note >= 0)
        {
            heldNote = note;
            processor.setKeyboardNotePressed (note, true);
        }
    }

    void ScaleKeyboard::releaseNote()
    {
        if (heldNote >= 0)
        {
            processor.setKeyboardNotePressed (heldNote, false);
            heldNote = -1;
        }
    }

    // The base class would play notes straight into the keyboard state, which here
    // only mirrors MadScaler's output. Clicks go to the processor as MIDI input instead.
    void ScaleKeyboard::mouseDown (const juce::MouseEvent& e)  { pressNote (noteAt (e)); }
    void ScaleKeyboard::mouseDrag (const juce::MouseEvent& e)  { pressNote (noteAt (e)); }
    void ScaleKeyboard::mouseUp (const juce::MouseEvent&)      { releaseNote(); }

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

    DetectedChordChip::DetectedChordChip (MadScalerProcessor& p, int s) : processor (p), slot (s) {}

    void DetectedChordChip::setChord (const madscaler::DetectedChord* newChord)
    {
        hasChord = newChord != nullptr;
        if (hasChord)
            chord = *newChord;
        setMouseCursor (hasChord ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }

    void DetectedChordChip::setLit (bool shouldBeLit)
    {
        if (lit != shouldBeLit)
        {
            lit = shouldBeLit;
            repaint();
        }
    }

    void DetectedChordChip::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat().reduced (3.0f);

        if (! hasChord)
        {
            g.setColour (Colours::pad.withAlpha (0.4f));
            g.drawRoundedRectangle (area, 6.0f, 1.0f);
            return;
        }

        g.setColour (lit ? Colours::padLit : Colours::pad);
        g.fillRoundedRectangle (area, 6.0f);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawFittedText (chord.name, area.toNearestInt(), juce::Justification::centred, 1);
    }

    void DetectedChordChip::mouseDown (const juce::MouseEvent&)
    {
        if (! hasChord)
            return;

        held = true;
        processor.setDetectedChordPressed (slot, chord.notes, true);
    }

    void DetectedChordChip::mouseUp (const juce::MouseEvent&)
    {
        if (held)
        {
            held = false;
            processor.setDetectedChordPressed (slot, {}, false);
        }
    }

    DetectionPanel::DetectionPanel (MadScalerProcessor& p) : processor (p)
    {
        titleLabel.setText ("Detect", juce::dontSendNotification);
        titleLabel.setColour (juce::Label::textColourId, Colours::accent);
        titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (titleLabel);

        currentLabel.setColour (juce::Label::textColourId, Colours::text);
        currentLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
        addAndMakeVisible (currentLabel);

        suggestionsLabel.setText ("Fits:", juce::dontSendNotification);
        suggestionsLabel.setColour (juce::Label::textColourId, Colours::textDim);
        suggestionsLabel.setFont (juce::FontOptions (13.0f));
        addAndMakeVisible (suggestionsLabel);

        clearButton.onClick = [this]
        {
            processor.detectedChords.clear();
            refreshHistory();
        };
        addAndMakeVisible (clearButton);

        for (int i = 0; i < MadScalerProcessor::maxDetectedChords; ++i)
        {
            chips[(size_t) i] = std::make_unique<DetectedChordChip> (processor, i);
            addAndMakeVisible (*chips[(size_t) i]);
        }

        for (size_t i = 0; i < suggestionButtons.size(); ++i)
        {
            auto& button = suggestionButtons[i];
            button.setTooltip ("Set the key and scale to this");
            button.setColour (juce::TextButton::buttonColourId, Colours::pad);
            button.setColour (juce::TextButton::textColourOffId, Colours::text);
            button.onClick = [this, i] { applySuggestion (i); };
            addChildComponent (button);
        }

        refreshHistory();
    }

    void DetectionPanel::paint (juce::Graphics& g)
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    }

    void DetectionPanel::resized()
    {
        auto area = getLocalBounds().reduced (10, 6);

        auto left = area.removeFromLeft (150);
        titleLabel.setBounds (left.removeFromTop (18));
        currentLabel.setBounds (left);

        auto bottom = area.removeFromBottom (28);
        clearButton.setBounds (bottom.removeFromRight (60));
        suggestionsLabel.setBounds (bottom.removeFromLeft (40));
        for (auto& button : suggestionButtons)
        {
            button.setBounds (bottom.removeFromLeft (170).reduced (2, 0));
        }

        area.removeFromBottom (4);
        const auto chipWidth = area.getWidth() / MadScalerProcessor::maxDetectedChords;
        for (auto& chip : chips)
            chip->setBounds (area.removeFromLeft (chipWidth));
    }

    void DetectionPanel::update()
    {
        const auto held = processor.getHeldInputNotes();

        if (held != lastHeldNotes)
        {
            lastHeldNotes = held;
            const auto detected = madscaler::detectChord (held);

            if (held.empty())
            {
                currentLabel.setColour (juce::Label::textColourId, Colours::textDim);
                currentLabel.setFont (juce::FontOptions (13.0f));
                currentLabel.setText ("Play a chord into MadScaler", juce::dontSendNotification);
            }
            else
            {
                currentLabel.setColour (juce::Label::textColourId, Colours::text);
                currentLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
                currentLabel.setText (detected.valid ? juce::String (detected.name) : juce::String ("?"),
                                      juce::dontSendNotification);
            }

            pendingName = detected.valid ? detected.name : std::string();
            stableTicks = 0;
        }

        // A chord goes into the history once it has been held unchanged for a
        // moment, so notes of a chord that are not struck together are not logged.
        if (! pendingName.empty() && ++stableTicks == 6)
            addToHistory (madscaler::detectChord (held));

        const auto mask = processor.getActiveDetectedMask();
        for (int i = 0; i < MadScalerProcessor::maxDetectedChords; ++i)
            chips[(size_t) i]->setLit ((mask >> i) & 1u);
    }

    void DetectionPanel::addToHistory (const madscaler::DetectedChord& chord)
    {
        auto& history = processor.detectedChords;

        if (! history.empty() && history.back().name == chord.name)
            return;

        history.push_back (chord);
        if ((int) history.size() > MadScalerProcessor::maxDetectedChords)
            history.erase (history.begin());

        refreshHistory();
    }

    void DetectionPanel::refreshHistory()
    {
        const auto& history = processor.detectedChords;

        for (size_t i = 0; i < chips.size(); ++i)
            chips[i]->setChord (i < history.size() ? &history[i] : nullptr);

        std::vector<int> weights (12, 0);
        std::vector<int> roots;
        for (const auto& chord : history)
        {
            std::array<bool, 12> seen {};
            for (auto note : chord.notes)
                seen[(size_t) (note % 12)] = true;
            for (size_t pc = 0; pc < 12; ++pc)
                weights[pc] += seen[pc] ? 1 : 0;
            roots.push_back (chord.rootPitchClass);
        }

        suggestions = madscaler::suggestKeys (weights, roots, suggestionButtons.size());

        for (size_t i = 0; i < suggestionButtons.size(); ++i)
        {
            const auto visible = i < suggestions.size();
            suggestionButtons[i].setVisible (visible);
            if (visible)
                suggestionButtons[i].setButtonText (suggestions[i].name);
        }

        suggestionsLabel.setVisible (! suggestions.empty());
    }

    void DetectionPanel::applySuggestion (size_t index)
    {
        if (index >= suggestions.size())
            return;

        auto setParam = [this] (const char* id, int value)
        {
            if (auto* param = processor.parameters.getParameter (id))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->convertTo0to1 ((float) value));
                param->endChangeGesture();
            }
        };

        setParam ("key", suggestions[index].keyPitchClass);
        setParam ("scale", suggestions[index].scaleIndex);
    }
} // namespace madscaler::ui

MadScalerEditor::MadScalerEditor (MadScalerProcessor& p)
    : AudioProcessorEditor (&p), processor (p), detectionPanel (p), keyboard (p)
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
    setUpLabel (inversionLabel, "Inversion");
    setUpLabel (voicingLabel, "Voicing");

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
    setUpCombo (inversionBox, "inversion", inversionAttachment);
    setUpCombo (voicingBox, "voicing", voicingAttachment);

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

    for (auto* button : { &bassButton, &voiceLeadingButton })
    {
        button->setColour (juce::ToggleButton::textColourId, Colours::textDim);
        addAndMakeVisible (*button);
    }
    bassButton.setTooltip ("Add the chord root an octave below");
    voiceLeadingButton.setTooltip ("Pick the inversion that moves least from the previous chord");
    bassAttachment = std::make_unique<ButtonAttachment> (processor.parameters, "bass", bassButton);
    voiceLeadingAttachment = std::make_unique<ButtonAttachment> (processor.parameters, "voiceLeading", voiceLeadingButton);

    scaleNotesLabel.setColour (juce::Label::textColourId, Colours::text);
    scaleNotesLabel.setFont (juce::FontOptions (14.0f));
    addAndMakeVisible (scaleNotesLabel);

    for (int i = 0; i < MadScalerProcessor::numDegrees; ++i)
    {
        pads[(size_t) i] = std::make_unique<madscaler::ui::ChordPad> (processor, i);
        addAndMakeVisible (*pads[(size_t) i]);
    }

    addAndMakeVisible (detectionPanel);
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (760, 580, 1600, 1100);
    setSize (860, 640);
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

    controls = area.removeFromTop (52);
    place (inversionLabel, inversionBox, 90);
    place (voicingLabel, voicingBox, 110);
    auto toggles = controls.removeFromBottom (28);
    bassButton.setBounds (toggles.removeFromLeft (110));
    voiceLeadingButton.setBounds (toggles.removeFromLeft (140));

    auto secondRow = area.removeFromTop (30);
    scaleNotesLabel.setBounds (secondRow.removeFromLeft (secondRow.getWidth() / 2));
    keyTriggerButton.setBounds (secondRow);

    area.removeFromTop (4);
    detectionPanel.setBounds (area.removeFromTop (96));
    area.removeFromTop (8);
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
    std::array<float, 7> settings {};
    size_t index = 0;
    for (auto* id : { "key", "scale", "chordSize", "octave", "inversion", "voicing", "bass" })
        settings[index++] = processor.parameters.getRawParameterValue (id)->load();

    if (settings != lastChordSettings)
    {
        lastChordSettings = settings;
        refreshChords();
    }

    detectionPanel.update();

    const auto mask = processor.getActiveDegreeMask();
    for (int i = 0; i < MadScalerProcessor::numDegrees; ++i)
        pads[(size_t) i]->setLit ((mask >> i) & 1u);
}
