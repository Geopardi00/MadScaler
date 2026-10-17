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
    static const juce::String melodyChannel { "melodyChannel" };
    static const juce::String bassChannel { "bassChannel" };
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
    melodyChannelParam = parameters.getRawParameterValue (ParamIDs::melodyChannel);
    bassChannelParam = parameters.getRawParameterValue (ParamIDs::bassChannel);

    for (auto& channelMap : keysLockMap)
        channelMap.fill (-1);
}

juce::AudioProcessorValueTreeState::ParameterLayout MadScalerProcessor::createParameterLayout()
{
    juce::StringArray keyNames;
    for (auto* name : { "C", "C#/Db", "D", "Eb", "E", "F", "F#/Gb", "G", "Ab", "A", "Bb", "B" })
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
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::melodyChannel, 1 }, "Melody MIDI Channel", 1, 16, 2));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::bassChannel, 1 }, "Bass MIDI Channel", 1, 16, 3));
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

    const auto colour = juce::isPositiveAndBelow (degree, numDegrees) ? padColours[(size_t) degree].load() : 0u;
    if ((colour & 0x80000000u) != 0)
    {
        const auto rootOffset = (int) ((colour >> 24) & 0x0fu);
        const auto root = 12 * (octave + 1) + key + rootOffset;
        chord.notes.clear();
        for (int interval = 0; interval < 24; ++interval)
            if ((colour >> interval) & 1u)
                chord.notes.push_back (juce::jlimit (0, 127, root + interval));
        chord.rootPitchClass = (key + rootOffset) % 12;

        const juce::SpinLock::ScopedTryLockType lock (padColourLock);
        if (lock.isLocked())
            chord.name = madscaler::spellNote (chord.rootPitchClass, key, getScaleIndex()) + padColourSuffixes[(size_t) degree];
    }

    chord.notes = madscaler::voiceChord (chord.notes, getVoicingSettings(), previousNotes);
    return chord;
}

void MadScalerProcessor::setPadColour (int degree, const madscaler::ColourChord& chord)
{
    if (! juce::isPositiveAndBelow (degree, numDegrees) || chord.notes.empty())
        return;

    const auto rootPc = ((chord.rootPitchClass % 12) + 12) % 12;
    const auto lowest = chord.notes.front();
    const auto rootNote = lowest - (((lowest - rootPc) % 12) + 12) % 12;
    uint32_t bits = 0x80000000u | ((uint32_t) (((rootPc - getKey()) % 12 + 12) % 12) << 24);
    for (auto note : chord.notes)
        if (juce::isPositiveAndBelow (note - rootNote, 24))
            bits |= 1u << (note - rootNote);

    // The name without its root, so the root can be spelled for any key.
    auto suffix = chord.name;
    if (! suffix.empty() && suffix[0] >= 'A' && suffix[0] <= 'G')
    {
        suffix.erase (0, 1);
        if (! suffix.empty() && (suffix[0] == 'b' || suffix[0] == '#'))
            suffix.erase (0, 1);
    }

    {
        const juce::SpinLock::ScopedLockType lock (padColourLock);
        padColourSuffixes[(size_t) degree] = suffix;
    }
    padColours[(size_t) degree].store (bits);
    ++padColourVersion;
}

void MadScalerProcessor::clearPadColour (int degree)
{
    if (! juce::isPositiveAndBelow (degree, numDegrees))
        return;
    padColours[(size_t) degree].store (0u);
    ++padColourVersion;
}

bool MadScalerProcessor::hasPadColour (int degree) const
{
    return juce::isPositiveAndBelow (degree, numDegrees) && (padColours[(size_t) degree].load() & 0x80000000u) != 0;
}

std::vector<madscaler::ColourGroup> MadScalerProcessor::getColoursForDegree (int degree, const std::vector<int>& previousNotes,
                                                                             bool voiced) const
{
    const auto key = getKey();
    const auto octave = (int) octaveParam->load();
    const auto sevenths = chordSizeParam->load() > 0.5f;
    auto groups = madscaler::getChordColours (key, getScaleIndex(), degree, sevenths, 12 * (octave + 1) + key);
    if (! voiced)
        return groups;

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

madscaler::Progression MadScalerProcessor::buildFromLibrary (const madscaler::LibraryProgression& entry) const
{
    const auto key = getKey();
    const auto octave = (int) octaveParam->load();

    auto voicing = getVoicingSettings();
    voicing.voiceLeading = true;

    madscaler::Progression result;
    std::vector<int> previous;
    for (auto slot : madscaler::buildLibraryProgression (entry, key, 12 * (octave + 1) + key))
    {
        slot.notes = madscaler::voiceChord (slot.notes, voicing, previous);
        previous = slot.notes;
        result.slots.push_back (std::move (slot));
    }
    return result;
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

void MadScalerProcessor::setMelodyNotePressed (int midiNote, bool pressed, bool bass)
{
    UiEvent event;
    event.kind = bass ? UiEvent::Kind::bassNote : UiEvent::Kind::melodyNote;
    event.index = juce::jlimit (0, 127, midiNote);
    event.pressed = pressed;
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

    sections[(size_t) currentSection] = progression;
    updateProgressionSnapshot();
}

void MadScalerProcessor::updateProgressionSnapshot()
{
    ProgressionSnapshot snapshot;
    for (int section = 0; section < numSections; ++section)
    {
        if (! chainSections && section != currentSection)
            continue;

        int index = 0;
        for (const auto& slot : sections[(size_t) section].slots)
        {
            auto& target = snapshot.slots[(size_t) snapshot.numSlots++];
            target.numNotes = juce::jmin ((int) slot.notes.size(), maxChordNotes);
            for (int i = 0; i < target.numNotes; ++i)
                target.notes[(size_t) i] = juce::jlimit (0, 127, slot.notes[(size_t) i]);
            target.lengthBeats = juce::jmax (0.25, slot.lengthBeats);
            target.section = section;
            target.index = index++;
            snapshot.totalBeats += target.lengthBeats;
        }
    }

    const juce::SpinLock::ScopedLockType lock (progressionLock);
    pendingSnapshot = snapshot;
    snapshotChanged.store (true);
}

int MadScalerProcessor::getCurrentProgressionSlot() const
{
    return playingSection.load() == currentSection ? currentProgressionSlot.load() : -1;
}

void MadScalerProcessor::selectSection (int section)
{
    section = juce::jlimit (0, numSections - 1, section);
    if (section == currentSection)
        return;

    currentSection = section;
    setProgression (sections[(size_t) section]);
}

const madscaler::Progression& MadScalerProcessor::getSection (int section) const
{
    return sections[(size_t) juce::jlimit (0, numSections - 1, section)];
}

void MadScalerProcessor::copySection (int from, int to)
{
    from = juce::jlimit (0, numSections - 1, from);
    to = juce::jlimit (0, numSections - 1, to);
    sections[(size_t) to] = sections[(size_t) from];
    if (to == currentSection)
        setProgression (sections[(size_t) to]);
    else
        updateProgressionSnapshot();
}

void MadScalerProcessor::clearSection (int section)
{
    section = juce::jlimit (0, numSections - 1, section);
    sections[(size_t) section] = {};
    if (section == currentSection)
        setProgression ({});
    else
        updateProgressionSnapshot();
}

void MadScalerProcessor::setChainSections (bool shouldChain)
{
    chainSections = shouldChain;
    updateProgressionSnapshot();
}

madscaler::Progression MadScalerProcessor::getPlaybackProgression() const
{
    if (! chainSections)
        return progression;

    madscaler::Progression chained;
    for (const auto& section : sections)
        chained.slots.insert (chained.slots.end(), section.slots.begin(), section.slots.end());
    return chained;
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
        stopMelodyNotes (out, 0);
        if (wasProgressionPlaying)
            setSlot (-1, 0);
        wasProgressionPlaying = false;
        currentProgressionSlot.store (-1);
        playingSection.store (-1);
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

    if (melodyPlaying.load() || bassPlaying.load())
        playMelody (startBeat, endBeat, beatsPerSample, numSamples, out);
    else
        stopMelodyNotes (out, 0);

    if (! hostPlaying)
        freeProgressionBeats = endBeat;

    if (playingSlot >= 0)
    {
        const auto& data = audioSnapshot.slots[(size_t) playingSlot];
        playingSection.store (data.section);
        currentProgressionSlot.store (data.index);
    }
    else
    {
        playingSection.store (-1);
        currentProgressionSlot.store (-1);
    }
}

void MadScalerProcessor::setMelodyState (const MelodyState& newState)
{
    melodyState = newState;
    ++melodyStateVersion;

    MelodySnapshot snapshot;
    snapshot.totalBeats = newState.totalBeats;
    auto addNotes = [&snapshot] (const std::vector<madscaler::TimedNote>& notes, bool bass)
    {
        for (const auto& note : notes)
        {
            if (snapshot.numNotes == MelodySnapshot::maxNotes)
                break;
            snapshot.notes[(size_t) snapshot.numNotes++] = { note.startBeat, note.lengthBeats, juce::jlimit (0, 127, note.note),
                                                             juce::jlimit (1, 127, note.velocity), bass };
        }
    };
    addNotes (newState.notes, false);
    addNotes (newState.bassNotes, true);

    const juce::SpinLock::ScopedLockType lock (progressionLock);
    pendingMelody = snapshot;
    melodyChanged.store (true);
}

void MadScalerProcessor::stopMelodyNotes (juce::MidiBuffer& out, int samplePos)
{
    for (int i = 0; i < numActiveMelodyNotes; ++i)
        out.addEvent (juce::MidiMessage::noteOff (activeMelodyNotes[(size_t) i].channel, activeMelodyNotes[(size_t) i].note), samplePos);
    numActiveMelodyNotes = 0;
    lastMelodyEndBeat = -1.0;
}

void MadScalerProcessor::playMelody (double startBeat, double endBeat, double beatsPerSample, int numSamples, juce::MidiBuffer& out)
{
    if (melodyChanged.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (progressionLock);
        if (lock.isLocked())
        {
            audioMelody = pendingMelody;
            melodyChanged.store (false);
            stopMelodyNotes (out, 0);
        }
    }

    // A jump in the DAW's position: release what is sounding and carry on from there.
    if (lastMelodyEndBeat >= 0.0 && std::abs (startBeat - lastMelodyEndBeat) > 0.01)
        stopMelodyNotes (out, 0);
    lastMelodyEndBeat = endBeat;

    auto samplePosFor = [&] (double beat)
    {
        return juce::jlimit (0, numSamples - 1, (int) std::ceil ((beat - startBeat) / beatsPerSample - 1.0e-6));
    };

    const auto playMelodyPart = melodyPlaying.load(), playBassPart = bassPlaying.load();

    // Note-offs due in this block, and for a part that was switched off.
    for (int i = 0; i < numActiveMelodyNotes;)
    {
        auto& active = activeMelodyNotes[(size_t) i];
        const auto partOff = active.bass ? ! playBassPart : ! playMelodyPart;
        if (active.endBeat < endBeat || partOff)
        {
            out.addEvent (juce::MidiMessage::noteOff (active.channel, active.note), partOff ? 0 : samplePosFor (active.endBeat));
            active = activeMelodyNotes[(size_t) --numActiveMelodyNotes];
        }
        else
        {
            ++i;
        }
    }

    const auto total = audioMelody.totalBeats;
    if (audioMelody.numNotes == 0 || total <= 0.0)
        return;

    const auto melodyChannel = juce::jlimit (1, 16, (int) melodyChannelParam->load());
    const auto bassChannel = juce::jlimit (1, 16, (int) bassChannelParam->load());

    // Note-ons starting in this block, looping over the melody's length.
    for (auto loop = std::floor (startBeat / total); loop * total < endBeat; loop += 1.0)
    {
        for (int i = 0; i < audioMelody.numNotes; ++i)
        {
            const auto& note = audioMelody.notes[(size_t) i];
            if (note.bass ? ! playBassPart : ! playMelodyPart)
                continue;

            const auto start = loop * total + note.start;
            if (start < startBeat - 1.0e-9 || start >= endBeat - 1.0e-9)
                continue;

            const auto onPos = samplePosFor (start);
            const auto channel = note.bass ? bassChannel : melodyChannel;

            // The same pitch still sounding on this channel is released first.
            for (int a = 0; a < numActiveMelodyNotes; ++a)
            {
                if (activeMelodyNotes[(size_t) a].note == note.note && activeMelodyNotes[(size_t) a].channel == channel)
                {
                    out.addEvent (juce::MidiMessage::noteOff (activeMelodyNotes[(size_t) a].channel, note.note), onPos);
                    activeMelodyNotes[(size_t) a] = activeMelodyNotes[(size_t) --numActiveMelodyNotes];
                    break;
                }
            }

            out.addEvent (juce::MidiMessage::noteOn (channel, note.note, (juce::uint8) note.velocity), onPos);

            const auto end = start + note.length;
            if (end < endBeat)
                out.addEvent (juce::MidiMessage::noteOff (channel, note.note),
                              juce::jmin (numSamples - 1, juce::jmax (onPos + 1, samplePosFor (end))));
            else if (numActiveMelodyNotes < (int) activeMelodyNotes.size())
                activeMelodyNotes[(size_t) numActiveMelodyNotes++] = { note.note, channel, end, note.bass };
        }
    }
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

                    case UiEvent::Kind::melodyNote:
                    case UiEvent::Kind::bassNote:
                    {
                        const auto melodyChannel = juce::jlimit (1, 16, (int) (event.kind == UiEvent::Kind::bassNote ? bassChannelParam
                                                                                                                     : melodyChannelParam)->load());
                        out.addEvent (event.pressed ? juce::MidiMessage::noteOn (melodyChannel, event.index, velocity)
                                                    : juce::MidiMessage::noteOff (melodyChannel, event.index),
                                      0);
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

    juce::ValueTree padsTree ("Pads");
    {
        const juce::SpinLock::ScopedLockType lock (padColourLock);
        for (int degree = 0; degree < numDegrees; ++degree)
        {
            juce::ValueTree padTree ("Pad");
            padTree.setProperty ("colour", juce::String::toHexString ((int) padColours[(size_t) degree].load()), nullptr);
            padTree.setProperty ("suffix", juce::String::fromUTF8 (padColourSuffixes[(size_t) degree].c_str()), nullptr);
            padsTree.appendChild (padTree, nullptr);
        }
    }
    state.removeChild (state.getChildWithName ("Pads"), nullptr);
    state.appendChild (padsTree, nullptr);

    auto writeSlots = [] (juce::ValueTree& tree, const madscaler::Progression& source)
    {
        for (const auto& slot : source.slots)
        {
            juce::StringArray notes;
            for (auto note : slot.notes)
                notes.add (juce::String (note));

            juce::ValueTree slotTree ("Slot");
            slotTree.setProperty ("name", juce::String::fromUTF8 (slot.name.c_str()), nullptr);
            slotTree.setProperty ("notes", notes.joinIntoString (" "), nullptr);
            slotTree.setProperty ("beats", slot.lengthBeats, nullptr);
            tree.appendChild (slotTree, nullptr);
        }
    };

    juce::ValueTree progressionTree ("Progression");
    writeSlots (progressionTree, progression);
    state.removeChild (state.getChildWithName ("Progression"), nullptr);
    state.appendChild (progressionTree, nullptr);

    juce::ValueTree sectionsTree ("Sections");
    sectionsTree.setProperty ("current", currentSection, nullptr);
    sectionsTree.setProperty ("chain", chainSections, nullptr);
    for (const auto& section : sections)
    {
        juce::ValueTree sectionTree ("Section");
        writeSlots (sectionTree, section);
        sectionsTree.appendChild (sectionTree, nullptr);
    }
    state.removeChild (state.getChildWithName ("Sections"), nullptr);
    state.appendChild (sectionsTree, nullptr);

    juce::ValueTree melodyTree ("Melody");
    const auto& settings = melodyState.settings;
    melodyTree.setProperty ("mode", (int) settings.mode, nullptr);
    melodyTree.setProperty ("rhythm", settings.rhythm, nullptr);
    melodyTree.setProperty ("arpStyle", (int) settings.arpStyle, nullptr);
    melodyTree.setProperty ("density", settings.density, nullptr);
    melodyTree.setProperty ("low", settings.lowNote, nullptr);
    melodyTree.setProperty ("high", settings.highNote, nullptr);
    melodyTree.setProperty ("key", settings.keyPitchClass, nullptr);
    melodyTree.setProperty ("scale", settings.scaleIndex, nullptr);
    melodyTree.setProperty ("seed", (int) settings.seed, nullptr);
    melodyTree.setProperty ("gate", settings.gate, nullptr);
    melodyTree.setProperty ("swing", settings.swing, nullptr);
    melodyTree.setProperty ("accent", (int) settings.accent, nullptr);
    melodyTree.setProperty ("arpOctaves", settings.arpOctaves, nullptr);
    melodyTree.setProperty ("arpNotes", (int) settings.arpNotes, nullptr);
    melodyTree.setProperty ("variation", settings.variation, nullptr);
    melodyTree.setProperty ("bassNote", settings.bassNote, nullptr);
    melodyTree.setProperty ("keepFlowing", settings.keepFlowing, nullptr);
    melodyTree.setProperty ("total", melodyState.totalBeats, nullptr);
    melodyTree.setProperty ("playing", melodyPlaying.load(), nullptr);

    juce::StringArray melodyNotes;
    for (const auto& note : melodyState.notes)
        melodyNotes.add (juce::String (note.startBeat) + ":" + juce::String (note.lengthBeats) + ":"
                         + juce::String (note.note) + ":" + juce::String (note.velocity));
    melodyTree.setProperty ("notes", melodyNotes.joinIntoString (" "), nullptr);

    const auto& bass = melodyState.bass;
    melodyTree.setProperty ("bassStyle", (int) bass.style, nullptr);
    melodyTree.setProperty ("bassLow", bass.lowNote, nullptr);
    melodyTree.setProperty ("bassGate", bass.gate, nullptr);
    melodyTree.setProperty ("bassSwing", bass.swing, nullptr);
    melodyTree.setProperty ("bassHumanise", bass.humanise, nullptr);
    melodyTree.setProperty ("bassLeadIn", bass.leadIn, nullptr);
    melodyTree.setProperty ("bassSeed", (int) bass.seed, nullptr);
    melodyTree.setProperty ("bassPlaying", bassPlaying.load(), nullptr);
    melodyTree.setProperty ("humanise", settings.humanise, nullptr);

    juce::StringArray bassNotes;
    for (const auto& note : melodyState.bassNotes)
        bassNotes.add (juce::String (note.startBeat) + ":" + juce::String (note.lengthBeats) + ":"
                       + juce::String (note.note) + ":" + juce::String (note.velocity));
    melodyTree.setProperty ("bassNotes", bassNotes.joinIntoString (" "), nullptr);

    for (const auto& slot : melodyState.chords)
    {
        juce::StringArray notes;
        for (auto note : slot.notes)
            notes.add (juce::String (note));
        juce::ValueTree chordTree ("Chord");
        chordTree.setProperty ("name", juce::String::fromUTF8 (slot.name.c_str()), nullptr);
        chordTree.setProperty ("notes", notes.joinIntoString (" "), nullptr);
        chordTree.setProperty ("beats", slot.lengthBeats, nullptr);
        melodyTree.appendChild (chordTree, nullptr);
    }
    state.removeChild (state.getChildWithName ("Melody"), nullptr);
    state.appendChild (melodyTree, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MadScalerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);

    auto readSlots = [] (const juce::ValueTree& tree)
    {
        madscaler::Progression result;
        for (const auto& slotTree : tree)
        {
            madscaler::ProgressionSlot slot;
            slot.name = slotTree.getProperty ("name").toString().toStdString();
            for (const auto& token : juce::StringArray::fromTokens (slotTree.getProperty ("notes").toString(), " ", ""))
                if (token.isNotEmpty())
                    slot.notes.push_back (token.getIntValue());
            slot.lengthBeats = (double) slotTree.getProperty ("beats", 4.0);
            result.slots.push_back (slot);
            if ((int) result.slots.size() >= madscaler::Progression::maxSlots)
                break;
        }
        return result;
    };

    const auto progressionTree = state.getChildWithName ("Progression");
    const auto sectionsTree = state.getChildWithName ("Sections");
    std::array<madscaler::Progression, numSections> restoredSections;
    int restoredCurrent = 0;
    bool restoredChain = false;
    if (sectionsTree.isValid())
    {
        restoredCurrent = juce::jlimit (0, numSections - 1, (int) sectionsTree.getProperty ("current", 0));
        restoredChain = (bool) sectionsTree.getProperty ("chain", false);
        for (int i = 0; i < numSections && i < sectionsTree.getNumChildren(); ++i)
            restoredSections[(size_t) i] = readSlots (sectionsTree.getChild (i));
    }
    else
    {
        restoredSections[0] = readSlots (progressionTree); // saved before sections existed
    }

    MelodyState melody;
    const auto melodyTree = state.getChildWithName ("Melody");
    if (melodyTree.isValid())
    {
        auto& settings = melody.settings;
        settings.mode = (madscaler::MelodyMode) juce::jlimit (0, 1, (int) melodyTree.getProperty ("mode", 0));
        settings.rhythm = (int) melodyTree.getProperty ("rhythm", 0);
        settings.arpStyle = (madscaler::ArpStyle) juce::jlimit (0, (int) madscaler::getArpStyleNames().size() - 1,
                                                                (int) melodyTree.getProperty ("arpStyle", 0));
        settings.density = (int) melodyTree.getProperty ("density", 1);
        settings.lowNote = (int) melodyTree.getProperty ("low", 60);
        settings.highNote = (int) melodyTree.getProperty ("high", 79);
        settings.keyPitchClass = (int) melodyTree.getProperty ("key", 0);
        settings.scaleIndex = (int) melodyTree.getProperty ("scale", 0);
        settings.seed = (uint32_t) (int) melodyTree.getProperty ("seed", 1);
        settings.gate = juce::jlimit (0, 2, (int) melodyTree.getProperty ("gate", 1));
        settings.swing = juce::jlimit (0, 3, (int) melodyTree.getProperty ("swing", 0));
        settings.accent = (madscaler::Accent) juce::jlimit (0, 3, (int) melodyTree.getProperty ("accent", 0));
        settings.arpOctaves = juce::jlimit (1, 4, (int) melodyTree.getProperty ("arpOctaves", 2));
        settings.arpNotes = (madscaler::ArpNotes) juce::jlimit (0, (int) madscaler::getArpNotesNames().size() - 1,
                                                                (int) melodyTree.getProperty ("arpNotes", 0));
        settings.variation = juce::jlimit (0, 3, (int) melodyTree.getProperty ("variation", 0));
        settings.bassNote = (bool) melodyTree.getProperty ("bassNote", false);
        settings.keepFlowing = (bool) melodyTree.getProperty ("keepFlowing", false);
        melody.totalBeats = (double) melodyTree.getProperty ("total", 0.0);
        melodyPlaying.store ((bool) melodyTree.getProperty ("playing", true));

        auto readNotes = [&melodyTree] (const juce::Identifier& property, std::vector<madscaler::TimedNote>& notes)
        {
            for (const auto& token : juce::StringArray::fromTokens (melodyTree.getProperty (property).toString(), " ", ""))
            {
                const auto parts = juce::StringArray::fromTokens (token, ":", "");
                if (parts.size() == 4)
                    notes.push_back ({ parts[0].getDoubleValue(), parts[1].getDoubleValue(),
                                       parts[2].getIntValue(), parts[3].getIntValue() });
            }
        };
        readNotes ("notes", melody.notes);
        readNotes ("bassNotes", melody.bassNotes);

        settings.humanise = juce::jlimit (0, 3, (int) melodyTree.getProperty ("humanise", 0));
        auto& bass = melody.bass;
        bass.style = (madscaler::BassStyle) juce::jlimit (0, (int) madscaler::getBassStyleNames().size() - 1,
                                                          (int) melodyTree.getProperty ("bassStyle", (int) bass.style));
        bass.lowNote = juce::jlimit (12, 72, (int) melodyTree.getProperty ("bassLow", 36));
        bass.gate = juce::jlimit (0, 2, (int) melodyTree.getProperty ("bassGate", 1));
        bass.swing = juce::jlimit (0, 3, (int) melodyTree.getProperty ("bassSwing", 0));
        bass.humanise = juce::jlimit (0, 3, (int) melodyTree.getProperty ("bassHumanise", 0));
        bass.leadIn = (bool) melodyTree.getProperty ("bassLeadIn", false);
        bass.seed = (uint32_t) (int) melodyTree.getProperty ("bassSeed", 1);
        bass.keyPitchClass = settings.keyPitchClass;
        bass.scaleIndex = settings.scaleIndex;
        bassPlaying.store ((bool) melodyTree.getProperty ("bassPlaying", false));

        for (const auto& chordTree : melodyTree)
        {
            madscaler::ProgressionSlot slot;
            slot.name = chordTree.getProperty ("name").toString().toStdString();
            for (const auto& token : juce::StringArray::fromTokens (chordTree.getProperty ("notes").toString(), " ", ""))
                if (token.isNotEmpty())
                    slot.notes.push_back (token.getIntValue());
            slot.lengthBeats = (double) chordTree.getProperty ("beats", 4.0);
            melody.chords.push_back (slot);
        }
    }

    const auto padsTree = state.getChildWithName ("Pads");
    for (int degree = 0; degree < numDegrees; ++degree)
    {
        const auto padTree = padsTree.getChild (degree);
        const auto bits = padTree.isValid() ? (uint32_t) padTree.getProperty ("colour", "0").toString().getHexValue32() : 0u;
        {
            const juce::SpinLock::ScopedLockType lock (padColourLock);
            padColourSuffixes[(size_t) degree] = padTree.isValid() ? padTree.getProperty ("suffix").toString().toStdString() : std::string();
        }
        padColours[(size_t) degree].store (bits);
    }
    ++padColourVersion;

    state.removeChild (padsTree, nullptr);
    state.removeChild (progressionTree, nullptr);
    state.removeChild (sectionsTree, nullptr);
    state.removeChild (melodyTree, nullptr);
    parameters.replaceState (state);
    sections = restoredSections;
    currentSection = restoredCurrent;
    chainSections = restoredChain;
    setProgression (sections[(size_t) currentSection]);
    setMelodyState (melody);
}

juce::AudioProcessorEditor* MadScalerProcessor::createEditor()
{
    return new MadScalerEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MadScalerProcessor();
}
