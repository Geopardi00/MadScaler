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
    static const juce::String keysLock { "keysLock" };
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
    keysLockParam = parameters.getRawParameterValue (ParamIDs::keysLock);

    for (auto& channelMap : keysLockMap)
        channelMap.fill (-1);
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
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::keysLock, 1 }, "Keys Lock", false));
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

std::vector<madscaler::ColourGroup> MadScalerProcessor::getColoursForDegree (int degree, const std::vector<int>& previousNotes) const
{
    const auto key = getKey();
    const auto octave = (int) octaveParam->load();
    const auto sevenths = chordSizeParam->load() > 0.5f;
    auto groups = madscaler::getChordColours (key, getScaleIndex(), degree, sevenths, 12 * (octave + 1) + key);

    const auto voicing = getVoicingSettings();
    for (auto& group : groups)
        for (auto& chord : group.chords)
            chord.notes = madscaler::voiceChord (chord.notes, voicing, previousNotes);
    return groups;
}

madscaler::ColourChord MadScalerProcessor::getCircleChord (int position, bool minor, const std::vector<int>& previousNotes) const
{
    // Roots sit in the octave above the key's tonic, like the pads.
    const auto key = getKey();
    const auto octave = (int) octaveParam->load();
    auto chord = madscaler::buildCircleChord (position, minor, 12 * (octave + 1));
    if (chord.rootPitchClass < key)
        for (auto& note : chord.notes)
            note = juce::jmin (127, note + 12);
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

void MadScalerProcessor::setColourChordPressed (int id, const std::vector<int>& notes, bool pressed)
{
    UiEvent event;
    event.kind = UiEvent::Kind::colourChord;
    event.index = id;
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
    lastPlayedDegree.store (degree);
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
    for (auto& held : progressionHeld)
        stopChord (held, out, samplePos);
    stopChord (colourHeld, out, samplePos);
    colourHeldId = -1;
    playingSlot = -1;
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
    activeColourChord.store (colourHeld.active ? colourHeldId : -1);
}

void MadScalerProcessor::beginPatternBlock (int, juce::MidiBuffer& out)
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

    patternEngine.setSettings (*getPatternSettings());
    patternEngine.beginBlock (currentSampleRate, hostBpm, hostPlaying, hostPpq);
}

std::optional<madscaler::PatternSettings> MadScalerProcessor::getPatternSettings() const
{
    const auto choice = juce::jlimit (0, 7, (int) patternParam->load());
    if (choice == 0)
        return std::nullopt;

    static constexpr double rateBeats[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

    madscaler::PatternSettings settings;
    settings.mode = (madscaler::PatternMode) (choice - 1);
    settings.stepBeats = rateBeats[juce::jlimit (0, 5, (int) rateParam->load())];
    settings.gate = gateParam->load() / 100.0f;
    settings.octaves = juce::jlimit (1, 3, (int) arpOctavesParam->load());
    settings.strumMs = strumParam->load();
    settings.velocity = juce::jlimit (1, 127, (int) velocityParam->load());
    return settings;
}

void MadScalerProcessor::readPlayHead()
{
    hostBpm = 120.0;
    hostPpq = 0.0;
    hostPlaying = false;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                hostBpm = *bpm > 1.0 ? *bpm : 120.0;
            if (const auto ppq = position->getPpqPosition())
            {
                hostPpq = *ppq;
                hostPlaying = position->getIsPlaying();
            }
        }
    }

    lastBpm.store (hostBpm);
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
    for (const auto& held : progressionHeld)  consider (held);
    consider (colourHeld);

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

int MadScalerProcessor::ProgressionSnapshot::slotAt (double beat) const
{
    if (numSlots == 0 || totalBeats <= 0.0)
        return -1;

    auto position = std::fmod (beat, totalBeats);
    if (position < 0.0)
        position += totalBeats;

    double end = 0.0;
    for (int i = 0; i < numSlots; ++i)
    {
        end += slots[(size_t) i].lengthBeats;
        if (position < end - 1.0e-9)
            return i;
    }
    return numSlots - 1;
}

void MadScalerProcessor::setProgression (const madscaler::Progression& newProgression)
{
    progression = newProgression;
    if ((int) progression.slots.size() > madscaler::Progression::maxSlots)
        progression.slots.resize ((size_t) madscaler::Progression::maxSlots);

    ProgressionSnapshot snapshot;
    for (const auto& slot : progression.slots)
    {
        auto& target = snapshot.slots[(size_t) snapshot.numSlots++];
        target.numNotes = juce::jmin ((int) slot.notes.size(), maxChordNotes);
        for (int i = 0; i < target.numNotes; ++i)
            target.notes[(size_t) i] = juce::jlimit (0, 127, slot.notes[(size_t) i]);
        target.lengthBeats = juce::jmax (0.25, slot.lengthBeats);
        snapshot.totalBeats += target.lengthBeats;
    }

    const juce::SpinLock::ScopedLockType lock (progressionLock);
    pendingSnapshot = snapshot;
    snapshotChanged.store (true);
}

void MadScalerProcessor::playProgression (int numSamples, juce::MidiBuffer& out)
{
    if (snapshotChanged.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (progressionLock);
        if (lock.isLocked())
        {
            audioSnapshot = pendingSnapshot;
            snapshotChanged.store (false);
            playingSlot = -1; // pick up edits to the sounding chord too
        }
    }

    const auto playing = progressionPlaying.load() && audioSnapshot.numSlots > 0;

    auto setSlot = [&] (int slot, int pos)
    {
        if (slot == playingSlot)
            return;

        playingSlot = slot;

        // Two holders take turns: the new chord starts before the old one stops,
        // so notes the chords share keep ringing instead of being re-struck.
        auto& previous = progressionHeld[(size_t) progressionHolder];
        progressionHolder ^= 1;
        auto& next = progressionHeld[(size_t) progressionHolder];

        if (slot >= 0)
        {
            const auto& data = audioSnapshot.slots[(size_t) slot];
            startNotes (next, data.notes.data(), data.numNotes, out, pos);
        }
        stopChord (previous, out, pos);
        syncPattern (pos);
    };

    if (! playing)
    {
        if (wasProgressionPlaying)
            setSlot (-1, 0);
        wasProgressionPlaying = false;
        currentProgressionSlot.store (-1);
        return;
    }

    if (! wasProgressionPlaying)
        freeProgressionBeats = 0.0; // start from the top when not following the DAW
    wasProgressionPlaying = true;

    const auto beatsPerSample = hostBpm / (60.0 * currentSampleRate);
    const auto startBeat = hostPlaying ? hostPpq : freeProgressionBeats;

    setSlot (audioSnapshot.slotAt (startBeat), 0);

    // Chord changes inside this block, at the exact sample. Slot boundaries are
    // walked in order from the start of the loop containing startBeat.
    const auto endBeat = startBeat + numSamples * beatsPerSample;
    auto boundary = std::floor (startBeat / audioSnapshot.totalBeats) * audioSnapshot.totalBeats;
    bool done = false;

    for (int loop = 0; loop < 64 && ! done; ++loop)
    {
        for (int i = 0; i < audioSnapshot.numSlots; ++i)
        {
            boundary += audioSnapshot.slots[(size_t) i].lengthBeats;
            if (boundary >= endBeat - 1.0e-9)
            {
                done = true;
                break;
            }
            if (boundary > startBeat + 1.0e-9)
            {
                const auto pos = juce::jlimit (0, numSamples - 1, (int) std::ceil ((boundary - startBeat) / beatsPerSample));
                setSlot (audioSnapshot.slotAt (boundary + 1.0e-9), pos);
            }
        }
    }

    if (! hostPlaying)
        freeProgressionBeats = endBeat;

    currentProgressionSlot.store (playingSlot);
}

int MadScalerProcessor::applyKeysLock (const juce::MidiMessage& message, juce::MidiMessage& result)
{
    result = message;
    if (! message.isNoteOnOrOff())
        return -1;

    const auto channel = juce::jlimit (1, 16, message.getChannel());
    const auto note = message.getNoteNumber();
    auto& mapped = keysLockMap[(size_t) channel - 1][(size_t) note];
    auto target = note;

    if (message.isNoteOn())
    {
        if (keysLockParam->load() > 0.5f)
        {
            // Move to the nearest scale note; ties go down.
            for (int distance = 0; distance < 12; ++distance)
            {
                if (note - distance >= 0 && madscaler::isInScale (note - distance, getKey(), getScaleIndex()))
                {
                    target = note - distance;
                    break;
                }
                if (note + distance <= 127 && madscaler::isInScale (note + distance, getKey(), getScaleIndex()))
                {
                    target = note + distance;
                    break;
                }
            }
        }
        mapped = (int8_t) target;
    }
    else
    {
        // Release whatever note this key turned into when it was pressed.
        if (mapped >= 0)
            target = mapped;
        mapped = -1;
    }

    result.setNoteNumber (target);
    return target;
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
    readPlayHead();
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

                    case UiEvent::Kind::colourChord:
                    {
                        if (event.pressed)
                        {
                            startNotes (colourHeld, event.notes.data(), event.numNotes, out, 0);
                            colourHeldId = event.index;
                            lastChordNotes.assign (event.notes.begin(), event.notes.begin() + event.numNotes);
                        }
                        else if (event.index == colourHeldId)
                        {
                            stopChord (colourHeld, out, 0);
                            colourHeldId = -1;
                        }
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

        // Keys Lock moves notes into the scale. Notes that pass through are
        // what chord detection listens to.
        juce::MidiMessage passed;
        const auto passedNote = applyKeysLock (message, passed);
        if (message.isNoteOn())
            setHeldInputNote (passedNote, true);
        else if (message.isNoteOff())
            setHeldInputNote (passedNote, false);

        if (message.isAllNotesOff() || message.isAllSoundOff())
        {
            stopAllChords (out, pos);
            stopPattern (pos);
            for (auto& word : heldInputNotes)
                word.store (0);
        }

        out.addEvent (passed, pos);
    }

    playProgression (buffer.getNumSamples(), out);

    if (patternActive)
        patternEngine.endBlock (buffer.getNumSamples(), patternEvents);
    flushPatternEvents (out);

    midi.swapWith (out);
    outputKeyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), false);
    updateActiveDegreeMask();
}

void MadScalerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    juce::ValueTree progressionTree ("Progression");
    for (const auto& slot : progression.slots)
    {
        juce::StringArray notes;
        for (auto note : slot.notes)
            notes.add (juce::String (note));

        juce::ValueTree slotTree ("Slot");
        slotTree.setProperty ("name", juce::String::fromUTF8 (slot.name.c_str()), nullptr);
        slotTree.setProperty ("notes", notes.joinIntoString (" "), nullptr);
        slotTree.setProperty ("beats", slot.lengthBeats, nullptr);
        progressionTree.appendChild (slotTree, nullptr);
    }
    state.removeChild (state.getChildWithName ("Progression"), nullptr);
    state.appendChild (progressionTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MadScalerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);

    madscaler::Progression restored;
    const auto progressionTree = state.getChildWithName ("Progression");
    for (const auto& slotTree : progressionTree)
    {
        madscaler::ProgressionSlot slot;
        slot.name = slotTree.getProperty ("name").toString().toStdString();
        for (const auto& token : juce::StringArray::fromTokens (slotTree.getProperty ("notes").toString(), " ", ""))
            if (token.isNotEmpty())
                slot.notes.push_back (token.getIntValue());
        slot.lengthBeats = (double) slotTree.getProperty ("beats", 4.0);
        restored.slots.push_back (slot);
    }

    state.removeChild (progressionTree, nullptr);
    parameters.replaceState (state);
    setProgression (restored);
}

juce::AudioProcessorEditor* MadScalerProcessor::createEditor()
{
    return new MadScalerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MadScalerProcessor();
}
