// Drives MadScalerProcessor directly, as a host would, and checks its MIDI output.
#include "PluginProcessor.h"

#include <iostream>
#include <vector>

static int failures = 0;

static void expect (bool condition, const char* what)
{
    if (! condition)
    {
        ++failures;
        std::cerr << "FAIL: " << what << '\n';
    }
}

struct Note
{
    bool on;
    int channel;
    int number;
};

static std::vector<Note> runBlock (MadScalerProcessor& processor, juce::MidiBuffer midi = {})
{
    juce::AudioBuffer<float> audio (2, 256);
    audio.clear();
    processor.processBlock (audio, midi);

    expect (audio.getMagnitude (0, audio.getNumSamples()) < 1.0e-9f, "output audio is silent");

    std::vector<Note> notes;
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();
        if (message.isNoteOnOrOff())
            notes.push_back ({ message.isNoteOn(), message.getChannel(), message.getNoteNumber() });
    }
    return notes;
}

static bool hasNotes (const std::vector<Note>& notes, bool on, std::vector<int> expected)
{
    std::vector<int> actual;
    for (const auto& note : notes)
        if (note.on == on)
            actual.push_back (note.number);
    return actual == expected;
}

static void setParam (MadScalerProcessor& processor, const char* id, float plainValue)
{
    auto* param = processor.parameters.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (plainValue));
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    MadScalerProcessor processor;
    processor.setPlayConfigDetails (0, 2, 48000.0, 256);
    processor.prepareToPlay (48000.0, 256);

    expect (processor.producesMidi() && processor.acceptsMidi(), "plugin has MIDI in and out");

    // Pad V in C major, octave 4: G4 B4 D5.
    processor.setPadPressed (4, true);
    auto notes = runBlock (processor);
    expect (hasNotes (notes, true, { 67, 71, 74 }), "pad press sends the V chord");
    expect (processor.getActiveDegreeMask() == (1u << 4), "pad V is lit");

    // Changing the key while the pad is held must still release the notes that sounded.
    setParam (processor, "key", 2.0f);
    processor.setPadPressed (4, false);
    notes = runBlock (processor);
    expect (hasNotes (notes, false, { 67, 71, 74 }), "pad release stops the held notes");
    expect (processor.getActiveDegreeMask() == 0, "no pad lit after release");
    setParam (processor, "key", 0.0f);

    // Overlapping chords sharing a note: C (C E G) and Em (E G B).
    processor.setPadPressed (0, true);
    processor.setPadPressed (2, true);
    notes = runBlock (processor);
    expect (hasNotes (notes, true, { 60, 64, 67, 71 }), "shared notes start once");
    processor.setPadPressed (0, false);
    notes = runBlock (processor);
    expect (hasNotes (notes, false, { 60 }), "shared notes keep sounding for the other chord");
    processor.setPadPressed (2, false);
    notes = runBlock (processor);
    expect (hasNotes (notes, false, { 64, 67, 71 }), "last chord releases the shared notes");

    // MIDI channel and 7th chords.
    setParam (processor, "channel", 5.0f);
    setParam (processor, "chordSize", 1.0f);
    processor.setPadPressed (1, true);
    notes = runBlock (processor);
    expect (hasNotes (notes, true, { 62, 65, 69, 72 }), "ii7 has four notes");
    expect (! notes.empty() && notes.front().channel == 5, "notes go out on the chosen channel");
    processor.setPadPressed (1, false);
    runBlock (processor);
    setParam (processor, "channel", 1.0f);
    setParam (processor, "chordSize", 0.0f);

    // Pass-through: incoming notes go straight out when key triggering is off.
    juce::MidiBuffer in;
    in.addEvent (juce::MidiMessage::noteOn (1, 61, (juce::uint8) 90), 10);
    notes = runBlock (processor, in);
    expect (hasNotes (notes, true, { 61 }), "incoming MIDI passes through");
    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 61), 0);
    runBlock (processor, in);

    // Key triggering: a D key plays the ii chord.
    setParam (processor, "keyTrigger", 1.0f);
    in.clear();
    in.addEvent (juce::MidiMessage::noteOn (1, 50, (juce::uint8) 90), 0);
    notes = runBlock (processor, in);
    expect (hasNotes (notes, true, { 62, 65, 69 }), "D key plays the ii chord");
    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 50), 0);
    notes = runBlock (processor, in);
    expect (hasNotes (notes, false, { 62, 65, 69 }), "releasing the key stops the chord");

    setParam (processor, "keyTrigger", 0.0f);

    // On-screen keyboard clicks behave like incoming MIDI and feed chord detection.
    processor.setKeyboardNotePressed (64, true);
    processor.setKeyboardNotePressed (67, true);
    processor.setKeyboardNotePressed (72, true);
    notes = runBlock (processor);
    expect (hasNotes (notes, true, { 64, 67, 72 }), "on-screen keys play their notes");
    expect (processor.getHeldInputNotes() == std::vector<int> { 64, 67, 72 }, "held input notes are tracked");
    expect (madscaler::detectChord (processor.getHeldInputNotes()).name == "C/E", "held notes are detected as C/E");
    for (auto note : { 64, 67, 72 })
        processor.setKeyboardNotePressed (note, false);
    notes = runBlock (processor);
    expect (hasNotes (notes, false, { 64, 67, 72 }), "releasing on-screen keys stops the notes");
    expect (processor.getHeldInputNotes().empty(), "no input notes held after release");

    // A detected chord replays the exact notes that were played.
    processor.setDetectedChordPressed (2, { 52, 55, 60, 64 }, true);
    notes = runBlock (processor);
    expect (hasNotes (notes, true, { 52, 55, 60, 64 }), "detected chord replays its notes");
    expect (processor.getActiveDetectedMask() == (1u << 2), "detected chord slot is lit");
    processor.setDetectedChordPressed (2, {}, false);
    notes = runBlock (processor);
    expect (hasNotes (notes, false, { 52, 55, 60, 64 }), "releasing a detected chord stops it");

    // State round trip.
    setParam (processor, "scale", 1.0f);
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    MadScalerProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    expect (restored.getScaleIndex() == 1, "state restores the scale");

    if (failures == 0)
        std::cout << "All processor tests passed\n";

    return failures == 0 ? 0 : 1;
}
