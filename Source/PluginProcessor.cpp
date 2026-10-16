#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamIDs
{
    static const juce::String key { "key" };
    static const juce::String scale { "scale" };
    static const juce::String chordSize { "chordSize" };
    static const juce::String octave { "octave" };
    static const juce::String velocity { "velocity" };
    static const juce::String channel { "channel" };
    static const juce::String keyTrigger { "keyTrigger" };
    static const juce::String inversion { "inversion" };
    static const juce::String voicing { "voicing" };
    static const juce::String bass { "bass" };
    static const juce::String voiceLeading { "voiceLeading" };
    static const juce::String pattern { "pattern" };
    static const juce::String rate { "rate" };
    static const juce::String gate { "gate" };
    static const juce::String arpOctaves { "arpOctaves" };
    static const juce::String strum { "strum" };
}

MadScalerProcessor::MadScalerProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "MadScaler", createParameterLayout())
{
    keyParam = parameters.getRawParameterValue (ParamIDs::key);
    scaleParam = parameters.getRawParameterValue (ParamIDs::scale);
    chordSizeParam = parameters.getRawParameterValue (ParamIDs::chordSize);
    octaveParam = parameters.getRawParameterValue (ParamIDs::octave);
    velocityParam = parameters.getRawParameterValue (ParamIDs::velocity);
    channelParam = parameters.getRawParameterValue (ParamIDs::channel);
    keyTriggerParam = parameters.getRawParameterValue (ParamIDs::keyTrigger);
    inversionParam = parameters.getRawParameterValue (ParamIDs::inversion);
    voicingParam = parameters.getRawParameterValue (ParamIDs::voicing);
    bassParam = parameters.getRawParameterValue (ParamIDs::bass);
    voiceLeadingParam = parameters.getRawParameterValue (ParamIDs::voiceLeading);
    patternParam = parameters.getRawParameterValue (ParamIDs::pattern);
    rateParam = parameters.getRawParameterValue (ParamIDs::rate);
    gateParam = parameters.getRawParameterValue (ParamIDs::gate);
    arpOctavesParam = parameters.getRawParameterValue (ParamIDs::arpOctaves);
    strumParam = parameters.getRawParameterValue (ParamIDs::strum);
}

juce::AudioProcessorValueTreeState::ParameterLayout MadScalerProcessor::createParameterLayout()
{
    juce::StringArray keyNames;
    for (auto* name : madscaler::getNoteNames())
        keyNames.add (name);

    juce::StringArray scaleNames;
    for (const auto& scale : madscaler::getScales())
        scaleNames.add (scale.name);

    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::key, 1 }, "Key", keyNames, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::scale, 1 }, "Scale", scaleNames, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::chordSize, 1 }, "Chord Size",
                                                        StringArray { "Triads", "7th Chords" }, 0));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::octave, 1 }, "Octave", 1, 6, 4));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::velocity, 1 }, "Velocity", 1, 127, 100));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::channel, 1 }, "MIDI Channel", 1, 16, 1));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::keyTrigger, 1 }, "Chords From MIDI Keys", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::inversion, 1 }, "Inversion",
                                                        StringArray { "Root", "1st", "2nd", "3rd" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::voicing, 1 }, "Voicing",
                                                        StringArray { "Close", "Open", "Drop 2", "Drop 3" }, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::bass, 1 }, "Bass Note", false));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::voiceLeading, 1 }, "Voice Leading", false));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::pattern, 1 }, "Pattern",
                                                        StringArray { "Off", "Strum Up", "Strum Down", "Arp Up", "Arp Down",
                                                                      "Arp Up/Down", "Arp Random", "Pulse" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::rate, 1 }, "Rate",
                                                        StringArray { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 3));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::gate, 1 }, "Gate",
                                                       NormalisableRange<float> (10.0f, 100.0f, 1.0f), 80.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::arpOctaves, 1 }, "Arp Octaves", 1, 3, 1));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::strum, 1 }, "Strum Time", 5, 150, 30,
                                                     AudioParameterIntAttributes().withLabel ("ms")));
    return layout;
}

void MadScalerProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    patternEngine.reset();
    patternEvents.reserve (1024);
    lastChordNotes.reserve (maxChordNotes);
    outputMidi.ensureSize (2048);
    outputKeyboardState.reset();
}

void MadScalerProcessor::releaseResources()
{
    // Notes still held will be stopped at the start of the next block.
    needsAllNotesOff = true;
}

bool MadScalerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

int MadScalerProcessor::getKey() const
{
    return juce::jlimit (0, 11, (int) keyParam->load());
}

int MadScalerProcessor::getScaleIndex() const
{
    return juce::jlimit (0, (int) madscaler::getScales().size() - 1, (int) scaleParam->load());
}

madscaler::VoicingSettings MadScalerProcessor::getVoicingSettings() const
{
    madscaler::VoicingSettings settings;
    settings.inversion = juce::jlimit (0, 3, (int) inversionParam->load());
    settings.style = (madscaler::VoicingStyle) juce::jlimit (0, 3, (int) voicingParam->load());
    settings.addBass = bassParam->load() > 0.5f;
    settings.voiceLeading = voiceLeadingParam->load() > 0.5f;
    return settings;
}

madscaler::Chord MadScalerProcessor::getChordForDegree (int degree, const std::vector<int>& previousNotes) const
{
    const auto key = getKey();
    const auto octave = (int) octaveParam->load();
    const auto size = chordSizeParam->load() > 0.5f ? madscaler::ChordSize::seventh
                                                    : madscaler::ChordSize::triad;
    // MIDI note 60 is C4, so the tonic of octave n is 12 * (n + 1) + key.
    auto chord = madscaler::buildDiatonicChord (key, getScaleIndex(), degree, size, 12 * (octave + 1) + key);
    chord.notes = madscaler::voiceChord (chord.notes, getVoicingSettings(), previousNotes);
    return chord;
}

void MadScalerProcessor::pushUiEvent (const UiEvent& event)
{
    const auto scope = uiFifo.write (1);
    if (scope.blockSize1 > 0)
        uiEvents[(size_t) scope.startIndex1] = event;
    else if (scope.blockSize2 > 0)
        uiEvents[(size_t) scope.startIndex2] = event;
}

void MadScalerProcessor::setPadPressed (int degree, bool pressed)
{
    if (! juce::isPositiveAndBelow (degree, numDegrees))
        return;

    UiEvent event;
    event.kind = UiEvent::Kind::pad;
    event.index = degree;
    event.pressed = pressed;
    pushUiEvent (event);
}

void MadScalerProcessor::setKeyboardNotePressed (int midiNote, bool pressed)
{
    if (! juce::isPositiveAndBelow (midiNote, 128))
        return;

    UiEvent event;
    event.kind = UiEvent::Kind::keyboardNote;
    event.index = midiNote;
    event.pressed = pressed;
    pushUiEvent (event);
}

void MadScalerProcessor::setDetectedChordPressed (int slot, const std::vector<int>& notes, bool pressed)
{
    if (! juce::isPositiveAndBelow (slot, maxDetectedChords))
        return;

    UiEvent event;
    event.kind = UiEvent::Kind::detectedChord;
    event.index = slot;
    event.pressed = pressed;
    event.numNotes = juce::jmin ((int) notes.size(), maxChordNotes);
    for (int i = 0; i < event.numNotes; ++i)
        event.notes[(size_t) i] = juce::jlimit (0, 127, notes[(size_t) i]);
    pushUiEvent (event);
}

void MadScalerProcessor::setHeldInputNote (int midiNote, bool held)
{
    if (! juce::isPositiveAndBelow (midiNote, 128))
        return;

    auto& word = heldInputNotes[(size_t) midiNote / 32];
    const auto bit = 1u << (midiNote % 32);
    if (held)
        word.fetch_or (bit);
    else
        word.fetch_and (~bit);
}

std::vector<int> MadScalerProcessor::getHeldInputNotes() const
{
    std::vector<int> notes;
    for (int note = 0; note < 128; ++note)
        if ((heldInputNotes[(size_t) note / 32].load() >> (note % 32)) & 1u)
            notes.push_back (note);
    return notes;
}

void MadScalerProcessor::startChord (HeldChord& held, int degree, juce::MidiBuffer& out, int samplePos)
{
    // Building a chord allocates a little; chords are tiny and this only runs on
    // pad presses and key presses, not every block.
    const auto chord = getChordForDegree (degree, lastChordNotes);
    startNotes (held, chord.notes.data(), (int) chord.notes.size(), out, samplePos);
    held.degree = degree;
    lastChordNotes = chord.notes;
}

void MadScalerProcessor::startNotes (HeldChord& held, const int* notes, int numNotes, juce::MidiBuffer& out, int samplePos)
{
    if (held.active)
        stopChord (held, out, samplePos);

    const auto channel = juce::jlimit (1, 16, (int) channelParam->load());
    const auto velocity = (juce::uint8) juce::jlimit (1, 127, (int) velocityParam->load());

    held.active = true;
    held.channel = channel;
    held.degree = 0;
    held.serial = ++chordSerialCounter;
    held.numNotes = juce::jmin (numNotes, (int) held.notes.size());

    for (int i = 0; i < held.numNotes; ++i)
    {
        const auto note = notes[i];
        held.notes[(size_t) i] = note;

        if (patternActive)
            continue; // the pattern engine plays it

        auto& count = noteRefCount[(size_t) channel - 1][(size_t) note];
        if (count++ == 0)
            out.addEvent (juce::MidiMessage::noteOn (channel, note, velocity), samplePos);
    }
}

void MadScalerProcessor::stopChord (HeldChord& held, juce::MidiBuffer& out, int samplePos)
{
    if (! held.active)
        return;

    held.active = false;

    if (patternActive)
        return; // nothing was sent directly; the pattern engine follows on syncPattern

    for (int i = 0; i < held.numNotes; ++i)
    {
        const auto note = held.notes[(size_t) i];
        auto& count = noteRefCount[(size_t) held.channel - 1][(size_t) note];
        if (count > 0 && --count == 0)
            out.addEvent (juce::MidiMessage::noteOff (held.channel, note), samplePos);
    }
}

void MadScalerProcessor::stopAllChords (juce::MidiBuffer& out, int samplePos)
{
    for (auto& held : padChords)
        stopChord (held, out, samplePos);
    for (auto& held : keyChords)
        stopChord (held, out, samplePos);
    for (auto& held : detectedChordsHeld)
        stopChord (held, out, samplePos);
}

void MadScalerProcessor::updateActiveDegreeMask()
{
    uint32_t mask = 0;
    for (const auto& held : padChords)
        if (held.active)
            mask |= 1u << held.degree;
    for (const auto& held : keyChords)
        if (held.active)
            mask |= 1u << held.degree;
    activeDegreeMask.store (mask);

    uint32_t detectedMask = 0;
    for (size_t i = 0; i < detectedChordsHeld.size(); ++i)
        if (detectedChordsHeld[i].active)
            detectedMask |= 1u << i;
    activeDetectedMask.store (detectedMask);
}

void MadScalerProcessor::beginPatternBlock (int numSamples, juce::MidiBuffer& out)
{
    const auto choice = juce::jlimit (0, 7, (int) patternParam->load());

    if (choice != lastPatternChoice)
    {
        // Switching between direct chords and patterns (or between patterns):
        // release everything so no note is left hanging, then start clean.
        stopAllChords (out, 0);
        stopPattern (0);
        flushPatternEvents (out);
        patternActive = choice != 0;
        lastPatternChoice = choice;
    }

    if (! patternActive)
        return;

    static constexpr double rateBeats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

    madscaler::PatternSettings settings;
    settings.mode = (madscaler::PatternMode) (choice - 1);
    settings.stepBeats = rateBeats[juce::jlimit (0, 5, (int) rateParam->load())];
    settings.gate = gateParam->load() / 100.0f;
    settings.octaves = juce::jlimit (1, 3, (int) arpOctavesParam->load());
    settings.strumMs = strumParam->load();
    settings.velocity = juce::jlimit (1, 127, (int) velocityParam->load());
    patternEngine.setSettings (settings);

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;
            if (const auto hostPpq = position->getPpqPosition())
            {
                ppq = *hostPpq;
                playing = position->getIsPlaying();
            }
        }
    }

    juce::ignoreUnused (numSamples);
    patternEngine.beginBlock (currentSampleRate, bpm, playing, ppq);
}

void MadScalerProcessor::syncPattern (int samplePos)
{
    if (! patternActive)
        return;

    // The newest held chord drives the pattern.
    const HeldChord* newest = nullptr;
    auto consider = [&newest] (const HeldChord& held)
    {
        if (held.active && (newest == nullptr || held.serial > newest->serial))
            newest = &held;
    };

    for (const auto& held : padChords)          consider (held);
    for (const auto& held : keyChords)          consider (held);
    for (const auto& held : detectedChordsHeld) consider (held);

    const auto serial = newest != nullptr ? newest->serial : 0u;
    if (serial == patternSerial)
        return;

    patternSerial = serial;

    if (newest != nullptr)
    {
        if (! patternEngine.isSounding())
            patternChannel = newest->channel;
        patternEngine.setChord (newest->notes.data(), newest->numNotes, samplePos, patternEvents);
    }
    else
    {
        patternEngine.setChord (nullptr, 0, samplePos, patternEvents);
    }
}

void MadScalerProcessor::stopPattern (int samplePos)
{
    patternEngine.allNotesOff (samplePos, patternEvents);
    patternSerial = 0;
}

void MadScalerProcessor::flushPatternEvents (juce::MidiBuffer& out)
{
    for (const auto& event : patternEvents)
        out.addEvent (event.noteOn ? juce::MidiMessage::noteOn (patternChannel, event.note, (juce::uint8) event.velocity)
                                   : juce::MidiMessage::noteOff (patternChannel, event.note),
                      event.sampleOffset);
    patternEvents.clear();
}

// Maps a white key to a scale degree: C plays degree 1, D degree 2 ... B degree 7.
static int degreeForWhiteKey (int midiNote)
{
    switch (midiNote % 12)
    {
        case 0:  return 0;
        case 2:  return 1;
        case 4:  return 2;
        case 5:  return 3;
        case 7:  return 4;
        case 9:  return 5;
        case 11: return 6;
        default: return -1;
    }
}

void MadScalerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // MadScaler is silent: the sound comes from whichever synth receives its MIDI.
    buffer.clear();

    auto& out = outputMidi;
    out.clear();

    patternEvents.clear();
    beginPatternBlock (buffer.getNumSamples(), out);

    if (needsAllNotesOff)
    {
        stopAllChords (out, 0);
        stopPattern (0);
        for (auto& word : heldInputNotes)
            word.store (0);
        needsAllNotesOff = false;
    }

    // Clicks from the editor. On-screen keyboard notes join the incoming MIDI.
    {
        const auto channel = juce::jlimit (1, 16, (int) channelParam->load());
        const auto velocity = (juce::uint8) juce::jlimit (1, 127, (int) velocityParam->load());
        const auto scope = uiFifo.read (uiFifo.getNumReady());

        auto handle = [&] (int start, int size)
        {
            for (int i = start; i < start + size; ++i)
            {
                const auto& event = uiEvents[(size_t) i];

                switch (event.kind)
                {
                    case UiEvent::Kind::pad:
                    {
                        auto& held = padChords[(size_t) event.index];
                        if (event.pressed)
                            startChord (held, event.index, out, 0);
                        else
                            stopChord (held, out, 0);
                        syncPattern (0);
                        break;
                    }

                    case UiEvent::Kind::detectedChord:
                    {
                        auto& held = detectedChordsHeld[(size_t) event.index];
                        if (event.pressed)
                            startNotes (held, event.notes.data(), event.numNotes, out, 0);
                        else
                            stopChord (held, out, 0);
                        syncPattern (0);
                        break;
                    }

                    case UiEvent::Kind::keyboardNote:
                        midi.addEvent (event.pressed ? juce::MidiMessage::noteOn (channel, event.index, velocity)
                                                     : juce::MidiMessage::noteOff (channel, event.index),
                                       0);
                        break;
                }
            }
        };

        handle (scope.startIndex1, scope.blockSize1);
        handle (scope.startIndex2, scope.blockSize2);
    }

    // Incoming MIDI: either passed straight through, or white keys trigger chords.
    const auto keyTrigger = keyTriggerParam->load() > 0.5f;

    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        const auto pos = metadata.samplePosition;

        if (keyTrigger && (message.isNoteOn() || message.isNoteOff()))
        {
            const auto degree = degreeForWhiteKey (message.getNoteNumber());
            if (degree >= 0)
            {
                auto& held = keyChords[(size_t) message.getNoteNumber()];
                if (message.isNoteOn())
                    startChord (held, degree, out, pos);
                else
                    stopChord (held, out, pos);
                syncPattern (pos);
                continue;
            }
        }

        // Notes that pass through are what chord detection listens to.
        if (message.isNoteOn())
            setHeldInputNote (message.getNoteNumber(), true);
        else if (message.isNoteOff())
            setHeldInputNote (message.getNoteNumber(), false);

        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            stopAllChords (out, pos);
            stopPattern (pos);
            for (auto& word : heldInputNotes)
                word.store (0);
        }

        out.addEvent (message, pos);
    }

    if (patternActive)
        patternEngine.endBlock (buffer.getNumSamples(), patternEvents);
    flushPatternEvents (out);

    midi.swapWith (out);
    outputKeyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), false);
    updateActiveDegreeMask();
}

void MadScalerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MadScalerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* MadScalerProcessor::createEditor()
{
    return new MadScalerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MadScalerProcessor();
}
