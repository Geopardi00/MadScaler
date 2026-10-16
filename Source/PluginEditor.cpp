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

    void ChordPad::setSuggestion (int rank)
    {
        if (suggestion != rank)
        {
            suggestion = rank;
            repaint();
        }
    }

    void ChordPad::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat().reduced (4.0f);

        g.setColour (lit ? Colours::padLit : Colours::pad);
        g.fillRoundedRectangle (area, 10.0f);

        if (suggestion >= 0 && ! lit)
        {
            // Likely next chord: a tag in the corner, brightest for the most likely.
            const auto alpha = suggestion == 0 ? 0.95f : suggestion == 1 ? 0.7f : 0.5f;
            g.setColour (Colours::accent.withAlpha (alpha * 0.5f));
            g.drawRoundedRectangle (area, 10.0f, 1.5f);
            auto tag = area.reduced (8.0f).removeFromBottom (16.0f).removeFromRight (34.0f);
            tag.translate (0.0f, -18.0f);
            g.setColour (Colours::accent.withAlpha (alpha));
            g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
            g.drawText ("next", tag, juce::Justification::centredRight);
        }

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
            noteNames.add (juce::String (madscaler::spellNote (note, processor.getKey(), processor.getScaleIndex())));

        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (12.0f));
        g.drawFittedText (noteNames.joinIntoString (" "), content.toNearestInt(), juce::Justification::centred, 1);
    }

    void ChordPad::mouseDown (const juce::MouseEvent&)
    {
        held = true;
        processor.setPadPressed (degree, true);
        if (onPressed)
            onPressed (degree);
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
        if (onPressed)
            onPressed (chord);
    }

    void DetectedChordChip::mouseUp (const juce::MouseEvent&)
    {
        if (held)
        {
            held = false;
            processor.setDetectedChordPressed (slot, {}, false);
        }
    }

    ColourChip::ColourChip (MadScalerProcessor& p, int i) : processor (p), id (i) {}

    void ColourChip::setChord (const madscaler::ColourChord* newChord)
    {
        hasChord = newChord != nullptr;
        if (hasChord)
            chord = *newChord;
        setVisible (hasChord);
        repaint();
    }

    void ColourChip::setLit (bool shouldBeLit)
    {
        if (lit != shouldBeLit)
        {
            lit = shouldBeLit;
            repaint();
        }
    }

    void ColourChip::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat().reduced (2.0f);

        g.setColour (lit ? Colours::padLit : Colours::pad);
        g.fillRoundedRectangle (area, 6.0f);

        if (isMouseOver())
        {
            g.setColour (Colours::accent.withAlpha (0.6f));
            g.drawRoundedRectangle (area, 6.0f, 1.5f);
        }

        auto text = area.reduced (3.0f, 2.0f);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawFittedText (juce::String::fromUTF8 (chord.name.c_str()),
                          text.removeFromTop (text.getHeight() * 0.58f).toNearestInt(), juce::Justification::centredBottom, 1);
        g.setColour (lit ? Colours::text : Colours::textDim);
        g.setFont (juce::FontOptions (10.5f));
        g.drawFittedText (juce::String::fromUTF8 (chord.label.c_str()), text.toNearestInt(), juce::Justification::centredTop, 1);
    }

    void ColourChip::mouseDown (const juce::MouseEvent&)
    {
        if (! hasChord)
            return;

        held = true;
        processor.setColourChordPressed (id, chord.notes, true);
        if (onPressed)
            onPressed (id, chord);
    }

    void ColourChip::mouseUp (const juce::MouseEvent&)
    {
        if (held)
        {
            held = false;
            processor.setColourChordPressed (id, {}, false);
        }
    }

    CircleOfFifths::CircleOfFifths (MadScalerProcessor& p) : processor (p) {}

    void CircleOfFifths::refresh()
    {
        const auto key = processor.getKey();
        const auto scale = processor.getScaleIndex();

        for (int segment = 0; segment < 24; ++segment)
        {
            const auto chord = madscaler::buildCircleChord (segment % 12, segment >= 12, 60);
            inScale[(size_t) segment] = madscaler::isChordInScale (chord.notes, key, scale);
        }

        // The key's tonic chord: outer ring for major-type scales, inner for minor-type.
        const bool minorKey = madscaler::getScales()[(size_t) scale].intervals[2] == 3;
        tonicSegment = -1;
        for (int position = 0; position < 12; ++position)
            if (madscaler::circlePitchClass (position, minorKey) == key)
                tonicSegment = position + (minorKey ? 12 : 0);

        repaint();
    }

    void CircleOfFifths::setLitId (int id)
    {
        if (litId != id)
        {
            litId = id;
            repaint();
        }
    }

    void CircleOfFifths::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto centre = bounds.getCentre();
        const auto outer = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
        const auto middle = outer * 0.68f;
        const auto inner = outer * 0.38f;
        const auto step = juce::MathConstants<float>::twoPi / 12.0f;

        for (int segment = 0; segment < 24; ++segment)
        {
            const bool minor = segment >= 12;
            const auto position = segment % 12;
            const auto from = minor ? inner : middle;
            const auto to = minor ? middle : outer;
            const auto start = ((float) position - 0.5f) * step;

            juce::Path path;
            path.addPieSegment (centre.x - to, centre.y - to, to * 2.0f, to * 2.0f, start, start + step, from / to);

            const bool lit = litId == firstId + segment;
            const bool diatonic = inScale[(size_t) segment];
            g.setColour (lit ? Colours::padLit : diatonic ? Colours::pad.brighter (0.45f) : Colours::pad);
            g.fillPath (path);
            g.setColour (Colours::panel);
            g.strokePath (path, juce::PathStrokeType (1.5f));

            const auto angle = (float) position * step;
            const auto radius = (from + to) * 0.5f;
            const juce::Point<float> labelCentre { centre.x + radius * std::sin (angle), centre.y - radius * std::cos (angle) };
            const auto name = madscaler::getNoteName (madscaler::circlePitchClass (position, minor), position >= 7) + (minor ? "m" : "");

            g.setColour (diatonic || lit ? Colours::text : Colours::textDim);
            g.setFont (juce::FontOptions (minor ? 10.0f : 11.5f, diatonic ? juce::Font::bold : juce::Font::plain));
            g.drawText (name, juce::Rectangle<float> (30.0f, 14.0f).withCentre (labelCentre), juce::Justification::centred);
        }

        if (tonicSegment >= 0)
        {
            const bool minor = tonicSegment >= 12;
            const auto from = minor ? inner : middle;
            const auto to = minor ? middle : outer;
            const auto start = ((float) (tonicSegment % 12) - 0.5f) * step;
            juce::Path path;
            path.addPieSegment (centre.x - to, centre.y - to, to * 2.0f, to * 2.0f, start, start + step, from / to);
            g.setColour (Colours::accent);
            g.strokePath (path, juce::PathStrokeType (2.0f));
        }

        g.setColour (Colours::textDim);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("5ths", juce::Rectangle<float> (inner * 2.0f, 14.0f).withCentre (centre), juce::Justification::centred);
    }

    bool CircleOfFifths::hitTestSegment (juce::Point<float> position, int& segment, bool& minor) const
    {
        const auto bounds = getLocalBounds().toFloat();
        const auto centre = bounds.getCentre();
        const auto outer = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
        const auto middle = outer * 0.68f;
        const auto inner = outer * 0.38f;

        const auto offset = position - centre;
        const auto distance = offset.getDistanceFromOrigin();
        if (distance < inner || distance > outer)
            return false;

        auto angle = std::atan2 (offset.x, -offset.y); // 0 at the top, clockwise
        if (angle < 0.0f)
            angle += juce::MathConstants<float>::twoPi;

        segment = juce::roundToInt (angle / (juce::MathConstants<float>::twoPi / 12.0f)) % 12;
        minor = distance < middle;
        return true;
    }

    void CircleOfFifths::setKey (int pitchClass, bool minor)
    {
        auto setParam = [this] (const char* id, int value)
        {
            if (auto* param = processor.parameters.getParameter (id))
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (param->convertTo0to1 ((float) value));
                param->endChangeGesture();
            }
        };

        setParam ("key", pitchClass);
        setParam ("scale", minor ? 1 : 0); // Natural Minor or Major
    }

    void CircleOfFifths::mouseDown (const juce::MouseEvent& e)
    {
        int position = 0;
        bool minor = false;
        if (! hitTestSegment (e.position, position, minor))
            return;

        if (e.mods.isPopupMenu())
        {
            const auto pitchClass = madscaler::circlePitchClass (position, minor);
            const auto name = juce::String (madscaler::getNoteName (pitchClass, position >= 7)) + (minor ? " minor" : " major");
            juce::PopupMenu menu;
            menu.addItem ("Set key to " + name, [this, pitchClass, minor] { setKey (pitchClass, minor); });
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
            return;
        }

        const auto chord = processor.getCircleChord (position, minor);
        heldId = firstId + position + (minor ? 12 : 0);
        processor.setColourChordPressed (heldId, chord.notes, true);
        if (onPressed)
            onPressed (chord);
    }

    void CircleOfFifths::mouseUp (const juce::MouseEvent&)
    {
        if (heldId >= 0)
        {
            processor.setColourChordPressed (heldId, {}, false);
            heldId = -1;
        }
    }

    ColoursPanel::ColoursPanel (MadScalerProcessor& p) : processor (p), circle (p)
    {
        titleLabel.setColour (juce::Label::textColourId, Colours::accent);
        titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (titleLabel);

        hintLabel.setText ("Press a pad to see its colours. Right-click the circle to change key.", juce::dontSendNotification);
        hintLabel.setColour (juce::Label::textColourId, Colours::textDim);
        hintLabel.setFont (juce::FontOptions (11.5f));
        hintLabel.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (hintLabel);

        for (int g = 0; g < numGroups; ++g)
        {
            auto& label = groupLabels[(size_t) g];
            label.setColour (juce::Label::textColourId, Colours::textDim);
            label.setFont (juce::FontOptions (12.0f));
            addAndMakeVisible (label);

            for (int i = 0; i < maxChipsPerGroup; ++i)
            {
                auto& chip = chips[(size_t) g][(size_t) i];
                chip = std::make_unique<ColourChip> (processor, g * maxChipsPerGroup + i);
                chip->onPressed = [this] (int, const madscaler::ColourChord& chord)
                {
                    if (onChordPressed)
                        onChordPressed (chord.name, chord.notes);
                };
                addChildComponent (*chip);
            }
        }

        circle.onPressed = [this] (const madscaler::ColourChord& chord)
        {
            if (onChordPressed)
                onChordPressed (chord.name, chord.notes);
        };
        addAndMakeVisible (circle);
    }

    void ColoursPanel::setDegree (int newDegree)
    {
        if (degree != newDegree)
        {
            degree = newDegree;
            refresh();
        }
    }

    void ColoursPanel::refresh()
    {
        groups = processor.getColoursForDegree (degree);
        const auto base = processor.getChordForDegree (degree);
        titleLabel.setText ("Colours for " + juce::String::fromUTF8 (base.name.c_str()) + "  ("
                                + juce::String::fromUTF8 (base.roman.c_str()) + ")",
                            juce::dontSendNotification);

        for (size_t g = 0; g < (size_t) numGroups; ++g)
        {
            const auto* group = g < groups.size() ? &groups[g] : nullptr;
            groupLabels[g].setText (group != nullptr ? juce::String (group->title) : juce::String(), juce::dontSendNotification);

            for (size_t i = 0; i < (size_t) maxChipsPerGroup; ++i)
                chips[g][i]->setChord (group != nullptr && i < group->chords.size() ? &group->chords[i] : nullptr);
        }

        circle.refresh();
    }

    void ColoursPanel::update()
    {
        const auto active = processor.getActiveColourChord();
        for (auto& row : chips)
            for (auto& chip : row)
                chip->setLit (chip->getId() == active);
        circle.setLitId (active);
    }

    void ColoursPanel::paint (juce::Graphics& g)
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    }

    void ColoursPanel::resized()
    {
        auto area = getLocalBounds().reduced (10, 6);
        circle.setBounds (area.removeFromLeft (area.getHeight()));
        area.removeFromLeft (12);

        auto header = area.removeFromTop (20);
        titleLabel.setBounds (header.removeFromLeft (240));
        hintLabel.setBounds (header);

        const auto rowHeight = area.getHeight() / numGroups;
        for (int g = 0; g < numGroups; ++g)
        {
            auto row = area.removeFromTop (rowHeight);
            groupLabels[(size_t) g].setBounds (row.removeFromLeft (76));
            const auto chipWidth = juce::jmin (96, row.getWidth() / maxChipsPerGroup);
            for (auto& chip : chips[(size_t) g])
                chip->setBounds (row.removeFromLeft (chipWidth).reduced (1, 1));
        }
    }

    ProgressionSlotView::ProgressionSlotView (MadScalerProcessor& p, int i) : processor (p), index (i) {}

    void ProgressionSlotView::setSlot (const madscaler::ProgressionSlot* newSlot, bool isNextEmpty)
    {
        hasSlot = newSlot != nullptr;
        if (hasSlot)
            slot = *newSlot;
        nextEmpty = isNextEmpty;
        repaint();
    }

    void ProgressionSlotView::setCurrent (bool isCurrent)
    {
        if (current != isCurrent)
        {
            current = isCurrent;
            repaint();
        }
    }

    void ProgressionSlotView::setSelected (bool isSelected)
    {
        if (selected != isSelected)
        {
            selected = isSelected;
            repaint();
        }
    }

    void ProgressionSlotView::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat().reduced (2.0f);

        if (! hasSlot)
        {
            g.setColour (Colours::pad.withAlpha (nextEmpty ? 0.9f : 0.35f));
            g.drawRoundedRectangle (area, 6.0f, 1.0f);
            if (nextEmpty)
            {
                g.setColour (Colours::textDim.withAlpha (0.6f));
                g.setFont (juce::FontOptions (18.0f));
                g.drawText ("+", area, juce::Justification::centred);
            }
            return;
        }

        g.setColour (current ? Colours::padLit : Colours::pad);
        g.fillRoundedRectangle (area, 6.0f);

        if (selected)
        {
            g.setColour (Colours::accent);
            g.drawRoundedRectangle (area.reduced (1.0f), 6.0f, 2.0f);
        }

        auto text = area.reduced (4.0f, 2.0f);
        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawFittedText (juce::String::fromUTF8 (slot.name.c_str()), text.removeFromTop (text.getHeight() * 0.6f).toNearestInt(),
                          juce::Justification::centred, 1);
        g.setColour (current ? Colours::text : Colours::textDim);
        g.setFont (juce::FontOptions (11.0f));
        const auto beats = juce::String (slot.lengthBeats, std::abs (slot.lengthBeats - std::round (slot.lengthBeats)) < 1.0e-6 ? 0 : 1);
        g.drawFittedText (beats + (std::abs (slot.lengthBeats - 1.0) < 1.0e-6 ? " beat" : " beats"), text.toNearestInt(), juce::Justification::centred, 1);
    }

    void ProgressionSlotView::mouseDown (const juce::MouseEvent& e)
    {
        if (! hasSlot)
            return;

        if (e.mods.isPopupMenu())
        {
            showMenu();
            return;
        }

        // Left click: hear the chord and select the slot for replacing.
        auditioning = true;
        processor.setColourChordPressed (auditionId + index, slot.notes, true);
        if (onClicked)
            onClicked (index);
    }

    void ProgressionSlotView::mouseUp (const juce::MouseEvent&)
    {
        if (auditioning)
        {
            auditioning = false;
            processor.setColourChordPressed (auditionId + index, {}, false);
        }
    }

    void ProgressionSlotView::showMenu()
    {
        juce::PopupMenu lengthMenu;
        for (double beats : { 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 16.0 })
            lengthMenu.addItem (juce::String (beats, 0) + (std::abs (beats - 1.0) < 1.0e-6 ? " beat" : " beats"), true, std::abs (slot.lengthBeats - beats) < 1.0e-6,
                                [this, beats]
                                {
                                    auto progression = processor.getProgression();
                                    if (index < (int) progression.slots.size())
                                        progression.slots[(size_t) index].lengthBeats = beats;
                                    processor.setProgression (progression);
                                });

        const auto count = (int) processor.getProgression().slots.size();

        // The scale's chords, voice-led from the chord before this one.
        juce::PopupMenu replaceMenu;
        const auto& slots = processor.getProgression().slots;
        const auto previous = index > 0 && index - 1 < (int) slots.size() ? slots[(size_t) index - 1].notes : std::vector<int>();
        for (int degree = 0; degree < MadScalerProcessor::numDegrees; ++degree)
        {
            const auto chord = processor.getChordForDegree (degree, previous);
            replaceMenu.addItem (juce::String::fromUTF8 (chord.roman.c_str()) + "   " + juce::String::fromUTF8 (chord.name.c_str()),
                                 [this, chord] { if (onReplace) onReplace (index, chord); });
        }

        juce::PopupMenu menu;
        menu.addSectionHeader (juce::String::fromUTF8 (slot.name.c_str()));
        menu.addSubMenu ("Replace with", replaceMenu);
        menu.addItem ("Pick a new chord...", [this] { if (onClicked) onClicked (index); });
        menu.addSubMenu ("Length", lengthMenu);
        menu.addItem ("Move left", index > 0, false, [this]
        {
            auto progression = processor.getProgression();
            std::swap (progression.slots[(size_t) index], progression.slots[(size_t) index - 1]);
            processor.setProgression (progression);
        });
        menu.addItem ("Move right", index < count - 1, false, [this]
        {
            auto progression = processor.getProgression();
            std::swap (progression.slots[(size_t) index], progression.slots[(size_t) index + 1]);
            processor.setProgression (progression);
        });
        menu.addItem ("Duplicate", count < madscaler::Progression::maxSlots, false, [this]
        {
            auto progression = processor.getProgression();
            progression.slots.insert (progression.slots.begin() + index + 1, progression.slots[(size_t) index]);
            processor.setProgression (progression);
        });
        menu.addSeparator();
        menu.addItem ("Delete", [this]
        {
            auto progression = processor.getProgression();
            progression.slots.erase (progression.slots.begin() + index);
            processor.setProgression (progression);
        });

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
    }

    ProgressionStrip::DragHandle::DragHandle (ProgressionStrip& o) : owner (o)
    {
        setText ("Drag MIDI", juce::dontSendNotification);
        setJustificationType (juce::Justification::centred);
        setColour (juce::Label::textColourId, Colours::text);
        setColour (juce::Label::backgroundColourId, Colours::pad);
        setColour (juce::Label::outlineColourId, Colours::accent.withAlpha (0.5f));
        setFont (juce::FontOptions (13.0f));
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag the progression into your DAW as a MIDI clip");
    }

    void ProgressionStrip::DragHandle::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragging || e.getDistanceFromDragStart() < 6 || owner.processor.getProgression().slots.empty())
            return;

        dragging = true;
        const auto file = owner.writeMidiFile (juce::File::getSpecialLocation (juce::File::tempDirectory)
                                                   .getChildFile ("MadScaler Progression.mid"));
        if (file.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                        [this] { dragging = false; });
        else
            dragging = false;
    }

    void ProgressionStrip::DragHandle::mouseUp (const juce::MouseEvent&)
    {
        dragging = false;
    }

    ProgressionStrip::ProgressionStrip (MadScalerProcessor& p) : processor (p)
    {
        titleLabel.setText ("Progression", juce::dontSendNotification);
        titleLabel.setColour (juce::Label::textColourId, Colours::accent);
        titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        addAndMakeVisible (titleLabel);

        hintLabel.setColour (juce::Label::textColourId, Colours::textDim);
        hintLabel.setFont (juce::FontOptions (11.5f));
        addAndMakeVisible (hintLabel);

        for (auto* button : { &addButton, &playButton, &clearButton, &exportButton })
        {
            button->setColour (juce::TextButton::buttonColourId, Colours::pad);
            button->setColour (juce::TextButton::buttonOnColourId, Colours::padLit);
            button->setColour (juce::TextButton::textColourOffId, Colours::text);
            addAndMakeVisible (*button);
        }

        addButton.setClickingTogglesState (true);
        addButton.setTooltip ("While on, every chord you press is added to the end of the progression");
        playButton.setClickingTogglesState (true);
        playButton.setTooltip ("Loop the progression, following the DAW's position while it plays");
        playButton.onClick = [this] { processor.setProgressionPlaying (playButton.getToggleState()); };
        clearButton.onClick = [this]
        {
            select (-1);
            processor.setProgression ({});
        };
        exportButton.onClick = [this] { exportMidi(); };
        addAndMakeVisible (dragHandle);

        for (int i = 0; i < madscaler::Progression::maxSlots; ++i)
        {
            slots[(size_t) i] = std::make_unique<ProgressionSlotView> (processor, i);
            slots[(size_t) i]->onClicked = [this] (int index) { select (selected == index ? -1 : index); };
            slots[(size_t) i]->onReplace = [this] (int index, const madscaler::Chord& chord)
            {
                auto progression = processor.getProgression();
                if (index < (int) progression.slots.size())
                {
                    progression.slots[(size_t) index].name = chord.name;
                    progression.slots[(size_t) index].notes = chord.notes;
                    processor.setProgression (progression);
                }
            };
            addAndMakeVisible (*slots[(size_t) i]);
        }

        refresh();
    }

    void ProgressionStrip::chordPressed (const std::string& name, const std::vector<int>& notes)
    {
        auto progression = processor.getProgression();
        if (notes.empty())
            return;

        if (selected >= 0 && selected < (int) progression.slots.size())
        {
            // Replace the selected chord and keep its length. The slot stays
            // selected so other chords can be tried in its place.
            progression.slots[(size_t) selected].name = name;
            progression.slots[(size_t) selected].notes = notes;
        }
        else if (isAddMode() && (int) progression.slots.size() < madscaler::Progression::maxSlots)
        {
            progression.slots.push_back ({ name, notes, 4.0 });
        }
        else
        {
            return;
        }

        processor.setProgression (progression);
    }

    void ProgressionStrip::loadProgression (const madscaler::Progression& loaded, bool append)
    {
        auto progression = append ? processor.getProgression() : madscaler::Progression();
        for (const auto& slot : loaded.slots)
            if ((int) progression.slots.size() < madscaler::Progression::maxSlots)
                progression.slots.push_back (slot);

        select (-1);
        processor.setProgression (progression);
    }

    std::vector<int> ProgressionStrip::contextNotes() const
    {
        const auto& progression = processor.getProgression();
        if (selected > 0 && selected <= (int) progression.slots.size())
            return progression.slots[(size_t) selected - 1].notes;
        if (selected == 0)
            return {};
        return progression.slots.empty() ? std::vector<int>() : progression.slots.back().notes;
    }

    void ProgressionStrip::select (int index)
    {
        selected = index;
        for (int i = 0; i < (int) slots.size(); ++i)
            slots[(size_t) i]->setSelected (i == selected);

        hintLabel.setColour (juce::Label::textColourId, selected >= 0 ? Colours::accent : Colours::textDim);
        hintLabel.setText (selected >= 0 ? "Now press any chord to replace it"
                                         : "Click a chord to replace it, right-click for more",
                           juce::dontSendNotification);
    }

    void ProgressionStrip::refresh()
    {
        const auto& progression = processor.getProgression();
        for (size_t i = 0; i < slots.size(); ++i)
            slots[i]->setSlot (i < progression.slots.size() ? &progression.slots[i] : nullptr,
                               i == progression.slots.size());
        lastSize = progression.slots.size();
        if (selected >= (int) lastSize)
            select (-1);
        else if (hintLabel.getText().isEmpty())
            select (selected);
        resized();
    }

    void ProgressionStrip::update()
    {
        // The progression can change from slot menus or a host state load.
        static_assert (madscaler::Progression::maxSlots > 0);
        refresh();

        const auto current = processor.getCurrentProgressionSlot();
        for (int i = 0; i < (int) slots.size(); ++i)
            slots[(size_t) i]->setCurrent (i == current);

        if (playButton.getToggleState() != processor.isProgressionPlaying())
            playButton.setToggleState (processor.isProgressionPlaying(), juce::dontSendNotification);
    }

    void ProgressionStrip::paint (juce::Graphics& g)
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    }

    void ProgressionStrip::resized()
    {
        auto area = getLocalBounds().reduced (10, 6);

        auto top = area.removeFromTop (24);
        titleLabel.setBounds (top.removeFromLeft (90));
        dragHandle.setBounds (top.removeFromRight (90));
        top.removeFromRight (6);
        exportButton.setBounds (top.removeFromRight (100));
        top.removeFromRight (6);
        clearButton.setBounds (top.removeFromRight (60));
        top.removeFromRight (6);
        playButton.setBounds (top.removeFromRight (60));
        top.removeFromRight (6);
        addButton.setBounds (top.removeFromRight (100));
        top.removeFromRight (6);
        hintLabel.setBounds (top);

        area.removeFromTop (6);

        // Show at least 8 slots, and one empty slot after the last chord.
        const auto visible = juce::jlimit (8, madscaler::Progression::maxSlots, (int) lastSize + 1);
        const auto slotWidth = area.getWidth() / visible;
        for (int i = 0; i < (int) slots.size(); ++i)
        {
            slots[(size_t) i]->setVisible (i < visible);
            if (i < visible)
                slots[(size_t) i]->setBounds (area.getX() + i * slotWidth, area.getY(), slotWidth, area.getHeight());
        }
    }

    juce::File ProgressionStrip::writeMidiFile (const juce::File& file) const
    {
        const auto pattern = processor.getPatternSettings();
        const auto velocity = (int) processor.parameters.getRawParameterValue ("velocity")->load();
        const auto channel = (int) processor.parameters.getRawParameterValue ("channel")->load();
        const auto bpm = processor.getLastBpm();

        const auto notes = madscaler::renderProgression (processor.getProgression(), velocity,
                                                         pattern ? &*pattern : nullptr, bpm);

        constexpr int ticksPerBeat = 960;
        juce::MidiMessageSequence sequence;
        sequence.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / bpm)), 0.0);
        for (const auto& note : notes)
        {
            const auto start = std::round (note.startBeat * ticksPerBeat);
            const auto end = std::round ((note.startBeat + note.lengthBeats) * ticksPerBeat);
            sequence.addEvent (juce::MidiMessage::noteOn (channel, note.note, (juce::uint8) note.velocity), start);
            sequence.addEvent (juce::MidiMessage::noteOff (channel, note.note), juce::jmax (start + 1.0, end));
        }
        sequence.updateMatchedPairs();

        juce::MidiFile midiFile;
        midiFile.setTicksPerQuarterNote (ticksPerBeat);
        midiFile.addTrack (sequence);

        file.deleteFile();
        juce::FileOutputStream stream (file);
        if (! stream.openedOk() || ! midiFile.writeTo (stream))
            return {};
        stream.flush();
        return file;
    }

    void ProgressionStrip::exportMidi()
    {
        if (processor.getProgression().slots.empty())
            return;

        chooser = std::make_unique<juce::FileChooser> ("Export progression as MIDI",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                           .getChildFile ("MadScaler Progression.mid"),
                                                       "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (file != juce::File())
                                      writeMidiFile (file.withFileExtension ("mid"));
                              });
    }

    LibraryPanel::LibraryPanel (MadScalerProcessor& p, ProgressionStrip& s) : processor (p), strip (s)
    {
        tagRows.push_back ({ "All", false });
        for (const auto& group : madscaler::getLibraryTagGroups())
        {
            tagRows.push_back ({ group.title, true });
            for (const auto* tag : group.tags)
                tagRows.push_back ({ tag, false });
        }

        for (auto* list : { &tagList, &progressionList })
        {
            list->setColour (juce::ListBox::backgroundColourId, Colours::background);
            list->setColour (juce::ListBox::outlineColourId, Colours::pad);
            list->setOutlineThickness (1);
            addAndMakeVisible (*list);
        }

        tagList.setModel (&tagModel);
        tagList.setRowHeight (20);
        progressionList.setModel (&progressionModel);
        progressionList.setRowHeight (38);

        detailLabel.setColour (juce::Label::textColourId, Colours::textDim);
        detailLabel.setFont (juce::FontOptions (11.5f));
        detailLabel.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (detailLabel);

        for (auto* button : { &loadButton, &appendButton })
        {
            button->setColour (juce::TextButton::buttonColourId, Colours::pad);
            button->setColour (juce::TextButton::textColourOffId, Colours::text);
            addAndMakeVisible (*button);
        }
        loadButton.setTooltip ("Replace the progression with this one, in the current key");
        appendButton.setTooltip ("Add this progression after the current one");
        loadButton.onClick = [this] { load (false); };
        appendButton.onClick = [this] { load (true); };

        tagList.selectRow (0);
    }

    void LibraryPanel::TagModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool isSelected)
    {
        if (row < 0 || row >= (int) owner.tagRows.size())
            return;

        const auto& tagRow = owner.tagRows[(size_t) row];
        if (tagRow.header)
        {
            g.setColour (Colours::accent);
            g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
            g.drawText (tagRow.text.toUpperCase(), 6, 0, width - 8, height, juce::Justification::centredLeft);
            return;
        }

        if (isSelected)
        {
            g.setColour (Colours::padLit);
            g.fillRect (0, 0, width, height);
        }

        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (tagRow.text, 14, 0, width - 16, height, juce::Justification::centredLeft);
    }

    void LibraryPanel::TagModel::selectedRowsChanged (int row)
    {
        if (row < 0 || row >= (int) owner.tagRows.size())
            return;

        if (owner.tagRows[(size_t) row].header)
        {
            // Headers are not selectable: move to the first tag under it.
            owner.tagList.selectRow (row + 1);
            return;
        }

        owner.showTag (owner.tagRows[(size_t) row].text);
    }

    void LibraryPanel::ProgressionModel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool isSelected)
    {
        if (row < 0 || row >= (int) owner.matches.size())
            return;

        const auto& progression = *owner.matches[(size_t) row];
        g.setColour (isSelected ? Colours::padLit : (row % 2 == 0 ? Colours::panel : Colours::background));
        g.fillRect (0, 0, width, height);

        g.setColour (Colours::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 (progression.name), 8, 3, width - 16, height / 2 - 2, juce::Justification::centredLeft);

        g.setColour (isSelected ? Colours::text : Colours::textDim);
        g.setFont (juce::FontOptions (11.5f));
        g.drawText (owner.chordNamesInKey (progression), 8, height / 2 + 1, width - 16, height / 2 - 4,
                    juce::Justification::centredLeft);
    }

    void LibraryPanel::ProgressionModel::listBoxItemDoubleClicked (int, const juce::MouseEvent&)
    {
        owner.load (false);
    }

    void LibraryPanel::ProgressionModel::selectedRowsChanged (int)
    {
        const auto* progression = owner.selectedProgression();
        owner.loadButton.setEnabled (progression != nullptr);
        owner.appendButton.setEnabled (progression != nullptr);

        if (progression == nullptr)
        {
            owner.detailLabel.setText ("Pick a progression. Double-click loads it.", juce::dontSendNotification);
            return;
        }

        juce::StringArray tags;
        for (const auto& tag : madscaler::getTags (*progression))
            tags.add (tag);

        const auto scaleName = juce::String (madscaler::getScales()[(size_t) progression->scaleIndex].name);
        owner.detailLabel.setText (juce::String (madscaler::getChordTokens (*progression).size()) + " chords, "
                                       + scaleName + "\n" + tags.joinIntoString (", "),
                                   juce::dontSendNotification);
    }

    void LibraryPanel::showTag (const juce::String& tag)
    {
        matches.clear();
        for (const auto& progression : madscaler::getLibrary())
            if (tag == "All" || madscaler::hasTag (progression, tag.toStdString()))
                matches.push_back (&progression);

        progressionList.updateContent();
        progressionList.deselectAllRows();
        progressionList.scrollToEnsureRowIsOnscreen (0);
        progressionList.repaint();
        progressionModel.selectedRowsChanged (-1);
    }

    const madscaler::LibraryProgression* LibraryPanel::selectedProgression() const
    {
        const auto row = progressionList.getSelectedRow();
        return row >= 0 && row < (int) matches.size() ? matches[(size_t) row] : nullptr;
    }

    juce::String LibraryPanel::chordNamesInKey (const madscaler::LibraryProgression& progression) const
    {
        juce::StringArray names;
        for (const auto& slot : madscaler::buildLibraryProgression (progression, processor.getKey(), 60))
            names.add (juce::String::fromUTF8 (slot.name.c_str()));
        return names.joinIntoString ("  ");
    }

    void LibraryPanel::load (bool append)
    {
        const auto* progression = selectedProgression();
        if (progression == nullptr)
            return;

        // Switch the pads to the progression's scale, so they offer fitting
        // chords for editing it.
        if (auto* param = processor.parameters.getParameter ("scale"))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) progression->scaleIndex));
            param->endChangeGesture();
        }

        strip.loadProgression (processor.buildFromLibrary (*progression), append);
    }

    void LibraryPanel::refresh()
    {
        progressionList.repaint();
    }

    void LibraryPanel::paint (juce::Graphics& g)
    {
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    }

    void LibraryPanel::resized()
    {
        auto area = getLocalBounds().reduced (10, 8);
        tagList.setBounds (area.removeFromLeft (170));
        area.removeFromLeft (8);

        auto right = area.removeFromRight (190);
        area.removeFromRight (8);
        progressionList.setBounds (area);

        auto buttons = right.removeFromBottom (28);
        loadButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2, 0));
        appendButton.setBounds (buttons.reduced (2, 0));
        right.removeFromBottom (4);
        detailLabel.setBounds (right);
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
            const auto detected = madscaler::detectChord (held, processor.getKey(), processor.getScaleIndex());

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
            addToHistory (madscaler::detectChord (held, processor.getKey(), processor.getScaleIndex()));

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
    : AudioProcessorEditor (&p), processor (p), detectionPanel (p), coloursPanel (p), progressionStrip (p), libraryPanel (p, progressionStrip), keyboard (p)
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
    setUpLabel (patternLabel, "Pattern");
    setUpLabel (rateLabel, "Rate");
    setUpLabel (gateLabel, "Gate %");
    setUpLabel (arpOctavesLabel, "Arp Octaves");
    setUpLabel (strumLabel, "Strum ms");

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
    setUpCombo (patternBox, "pattern", patternAttachment);
    setUpCombo (rateBox, "rate", rateAttachment);

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
    setUpSlider (arpOctavesSlider, "arpOctaves", arpOctavesAttachment);
    setUpSlider (strumSlider, "strum", strumAttachment);

    gateSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    gateSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 40, 24);
    gateSlider.setColour (juce::Slider::thumbColourId, Colours::accent);
    gateSlider.setColour (juce::Slider::trackColourId, Colours::padLit.withAlpha (0.6f));
    addAndMakeVisible (gateSlider);
    gateAttachment = std::make_unique<SliderAttachment> (processor.parameters, "gate", gateSlider);

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

    addChildComponent (detectionPanel);
    addAndMakeVisible (coloursPanel);
    addChildComponent (libraryPanel);
    addAndMakeVisible (progressionStrip);

    for (auto* tab : { &coloursTab, &libraryTab, &detectTab })
    {
        tab->setClickingTogglesState (true);
        tab->setRadioGroupId (1);
        tab->setColour (juce::TextButton::buttonColourId, Colours::background);
        tab->setColour (juce::TextButton::buttonOnColourId, Colours::panel);
        tab->setColour (juce::TextButton::textColourOffId, Colours::textDim);
        tab->setColour (juce::TextButton::textColourOnId, Colours::accent);
        tab->setConnectedEdges (juce::Button::ConnectedOnBottom);
        addAndMakeVisible (*tab);
    }
    coloursTab.onClick = [this] { showTab (0); };
    libraryTab.onClick = [this] { showTab (1); };
    detectTab.onClick = [this] { showTab (2); };
    coloursTab.setToggleState (true, juce::dontSendNotification);

    coloursPanel.onChordPressed = [this] (const std::string& name, const std::vector<int>& notes)
    {
        progressionStrip.chordPressed (name, notes);
    };

    // With "Add chords" on, pressed chords also go into the progression.
    for (auto& pad : pads)
        pad->onPressed = [this] (int degree)
        {
            coloursPanel.setDegree (degree);
            if (progressionStrip.wantsChords())
            {
                const auto chord = processor.getChordForDegree (degree, progressionStrip.contextNotes());
                progressionStrip.chordPressed (chord.name, chord.notes);
            }
        };

    for (auto& chip : detectionPanel.chips)
        chip->onPressed = [this] (const madscaler::DetectedChord& chord)
        {
            progressionStrip.chordPressed (chord.name, chord.notes);
        };

    keysLockButton.setColour (juce::ToggleButton::textColourId, Colours::textDim);
    keysLockButton.setTooltip ("Move every note you play to the nearest note of the scale");
    addAndMakeVisible (keysLockButton);
    keysLockAttachment = std::make_unique<ButtonAttachment> (processor.parameters, "keysLock", keysLockButton);
    addAndMakeVisible (keyboard);

    setResizable (true, true);
    setResizeLimits (800, 800, 1600, 1360);
    setSize (880, 860);
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

    controls = area.removeFromTop (52);
    place (patternLabel, patternBox, 130);
    place (rateLabel, rateBox, 80);
    place (gateLabel, gateSlider, 160);
    place (arpOctavesLabel, arpOctavesSlider, 100);
    place (strumLabel, strumSlider, 100);

    auto secondRow = area.removeFromTop (30);
    scaleNotesLabel.setBounds (secondRow.removeFromLeft (300));
    keysLockButton.setBounds (secondRow.removeFromLeft (120));
    keyTriggerButton.setBounds (secondRow);

    area.removeFromTop (4);
    auto tabs = area.removeFromTop (24);
    coloursTab.setBounds (tabs.removeFromLeft (90));
    libraryTab.setBounds (tabs.removeFromLeft (90));
    detectTab.setBounds (tabs.removeFromLeft (90));
    const auto panelArea = area.removeFromTop (150);
    coloursPanel.setBounds (panelArea);
    libraryPanel.setBounds (panelArea);
    detectionPanel.setBounds (panelArea);
    area.removeFromTop (8);
    progressionStrip.setBounds (area.removeFromTop (84));
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
        names.add (madscaler::spellNote (pc, processor.getKey(), processor.getScaleIndex()));

    scaleNotesLabel.setText ("Scale notes: " + names.joinIntoString ("  "), juce::dontSendNotification);
    keyboard.repaint();
    coloursPanel.refresh();
    libraryPanel.refresh();
    lastSuggestedDegree = -2; // the scale may have changed between major and minor
}

void MadScalerEditor::showTab (int tab)
{
    coloursPanel.setVisible (tab == 0);
    libraryPanel.setVisible (tab == 1);
    detectionPanel.setVisible (tab == 2);
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
    coloursPanel.update();
    progressionStrip.update();

    // Mark the pads that usually follow the last chord played.
    const auto lastDegree = processor.getLastPlayedDegree();
    if (lastDegree != lastSuggestedDegree)
    {
        lastSuggestedDegree = lastDegree;
        std::array<int, MadScalerProcessor::numDegrees> ranks;
        ranks.fill (-1);
        if (lastDegree >= 0)
        {
            const auto next = madscaler::suggestNextDegrees (processor.getScaleIndex(), lastDegree);
            for (size_t rank = 0; rank < next.size(); ++rank)
                ranks[(size_t) next[rank]] = (int) rank;
        }
        for (size_t i = 0; i < ranks.size(); ++i)
            pads[i]->setSuggestion (ranks[i]);
    }

    const auto mask = processor.getActiveDegreeMask();
    for (int i = 0; i < MadScalerProcessor::numDegrees; ++i)
        pads[(size_t) i]->setLit ((mask >> i) & 1u);
}
