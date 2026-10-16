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
    return layout;
}

void MadScalerProcessor::prepareToPlay (double, int)
{
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

madscaler::Chord MadScalerProcessor::getChordForDegree (int degree) const
{
    const auto key = getKey();
    const auto octave = (int) octaveParam->load();
    const auto size = chordSizeParam->load() > 0.5f ? madscaler::ChordSize::seventh
                                                    : madscaler::ChordSize::triad;
    // MIDI note 60 is C4, so the tonic of octave n is 12 * (n + 1) + key.
    return madscaler::buildDiatonicChord (key, getScaleIndex(), degree, size, 12 * (octave + 1) + key);
}

void MadScalerProcessor::setPadPressed (int degree, bool pressed)
{
    if (! juce::isPositiveAndBelow (degree, numDegrees))
        return;

    const auto scope = padFifo.write (1);
    if (scope.blockSize1 > 0)
        padEvents[(size_t) scope.startIndex1] = { degree, pressed };
    else if (scope.blockSize2 > 0)
        padEvents[(size_t) scope.startIndex2] = { degree, pressed };
}

void MadScalerProcessor::startChord (HeldChord& held, int degree, juce::MidiBuffer& out, int samplePos)
{
    if (held.active)
        stopChord (held, out, samplePos);

    // Building a chord allocates a little; chords are tiny and this only runs on
    // pad presses and key presses, not every block.
    const auto chord = getChordForDegree (degree);
    const auto channel = juce::jlimit (1, 16, (int) channelParam->load());
    const auto velocity = (juce::uint8) juce::jlimit (1, 127, (int) velocityParam->load());

    held.active = true;
    held.channel = channel;
    held.degree = degree;
    held.numNotes = juce::jmin ((int) chord.notes.size(), (int) held.notes.size());

    for (int i = 0; i < held.numNotes; ++i)
    {
        const auto note = chord.notes[(size_t) i];
        held.notes[(size_t) i] = note;

        auto& count = noteRefCount[(size_t) channel - 1][(size_t) note];
        if (count++ == 0)
            out.addEvent (juce::MidiMessage::noteOn (channel, note, velocity), samplePos);
    }
}

void MadScalerProcessor::stopChord (HeldChord& held, juce::MidiBuffer& out, int samplePos)
{
    if (! held.active)
        return;

    for (int i = 0; i < held.numNotes; ++i)
    {
        const auto note = held.notes[(size_t) i];
        auto& count = noteRefCount[(size_t) held.channel - 1][(size_t) note];
        if (count > 0 && --count == 0)
            out.addEvent (juce::MidiMessage::noteOff (held.channel, note), samplePos);
    }

    held.active = false;
}

void MadScalerProcessor::stopAllChords (juce::MidiBuffer& out, int samplePos)
{
    for (auto& held : padChords)
        stopChord (held, out, samplePos);
    for (auto& held : keyChords)
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

    if (needsAllNotesOff)
    {
        stopAllChords (out, 0);
        needsAllNotesOff = false;
    }

    // Pad presses from the editor.
    {
        const auto ready = padFifo.getNumReady();
        const auto scope = padFifo.read (ready);

        auto handle = [&] (int start, int size)
        {
            for (int i = start; i < start + size; ++i)
            {
                const auto event = padEvents[(size_t) i];
                auto& held = padChords[(size_t) event.degree];
                if (event.pressed)
                    startChord (held, event.degree, out, 0);
                else
                    stopChord (held, out, 0);
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
                continue;
            }
        }

        if (message.isAllNotesOff() || message.isAllSoundOff())
            stopAllChords (out, pos);

        out.addEvent (message, pos);
    }

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
