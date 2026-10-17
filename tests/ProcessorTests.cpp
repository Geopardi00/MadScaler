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

    // Colour chords: Dm9 from the ii chord's colours, then a Circle of Fifths chord.
    {
        const auto groups = processor.getColoursForDegree (1);
        const madscaler::ColourChord* dm9 = nullptr;
        for (const auto& chord : groups.front().chords)
            if (chord.name == "Dm9")
                dm9 = &chord;
        expect (dm9 != nullptr && dm9->notes == std::vector<int> { 62, 65, 69, 72, 76 }, "Dm9 colour chord");

        if (dm9 != nullptr)
        {
            processor.setColourChordPressed (7, dm9->notes, true);
            notes = runBlock (processor);
            expect (hasNotes (notes, true, { 62, 65, 69, 72, 76 }), "colour chord plays its notes");
            expect (processor.getActiveColourChord() == 7, "colour chord chip is lit");
            processor.setColourChordPressed (7, {}, false);
            notes = runBlock (processor);
            expect (hasNotes (notes, false, { 62, 65, 69, 72, 76 }), "releasing a colour chord stops it");
            expect (processor.getActiveColourChord() == -1, "no colour chip lit after release");
        }

        const auto d = processor.getCircleChord (2, false);
        expect (d.name == "D" && d.notes == std::vector<int> { 62, 66, 69 }, "D major from the circle");

        processor.setPadPressed (5, true);
        runBlock (processor);
        processor.setPadPressed (5, false);
        runBlock (processor);
        expect (processor.getLastPlayedDegree() == 5, "last played degree is tracked for next-chord hints");
    }

    // Voicings: first inversion of I in C major, then voice leading from C to F.
    setParam (processor, "inversion", 1.0f);
    processor.setPadPressed (0, true);
    notes = runBlock (processor);
    expect (hasNotes (notes, true, { 64, 67, 72 }), "first inversion of C");
    processor.setPadPressed (0, false);
    runBlock (processor);
    setParam (processor, "inversion", 0.0f);

    setParam (processor, "voiceLeading", 1.0f);
    processor.setPadPressed (0, true);
    runBlock (processor);
    processor.setPadPressed (0, false);
    runBlock (processor);
    processor.setPadPressed (3, true);
    notes = runBlock (processor);
    expect (hasNotes (notes, true, { 65, 69, 72 }) || hasNotes (notes, true, { 60, 65, 69 }),
            "voice-led F stays close to the previous chord");
    processor.setPadPressed (3, false);
    runBlock (processor);
    setParam (processor, "voiceLeading", 0.0f);

    setParam (processor, "bass", 1.0f);
    processor.setPadPressed (4, true);
    notes = runBlock (processor);
    expect (hasNotes (notes, true, { 55, 67, 71, 74 }), "bass note under V");
    processor.setPadPressed (4, false);
    runBlock (processor);
    setParam (processor, "bass", 0.0f);

    // Patterns: Arp Up at the default 1/16 and 120 bpm (no host), 48 kHz, so one
    // step is 6000 samples. The first note plays at once, the next a step later.
    setParam (processor, "pattern", 3.0f); // Arp Up
    runBlock (processor);
    processor.setPadPressed (0, true);
    std::vector<Note> patternNotes;
    for (int block = 0; block < 30; ++block) // 7680 samples
        for (const auto& note : runBlock (processor))
            patternNotes.push_back (note);
    expect (hasNotes (patternNotes, true, { 60, 64 }), "arp plays the chord one note at a time");
    processor.setPadPressed (0, false);
    notes = runBlock (processor);
    std::vector<Note> afterRelease;
    for (int block = 0; block < 30; ++block)
        for (const auto& note : runBlock (processor))
            afterRelease.push_back (note);
    expect (hasNotes (afterRelease, true, {}), "arp stops when the pad is released");

    // Switching back to no pattern while nothing is held leaves nothing hanging.
    setParam (processor, "pattern", 0.0f);
    runBlock (processor);
    processor.setPadPressed (1, true);
    notes = runBlock (processor);
    expect (hasNotes (notes, true, { 62, 65, 69 }), "with patterns off, the chord sounds at once");
    processor.setPadPressed (1, false);
    runBlock (processor);

    // Keys Lock: C# in C major becomes C, and its release releases C.
    setParam (processor, "keysLock", 1.0f);
    in.clear();
    in.addEvent (juce::MidiMessage::noteOn (1, 61, (juce::uint8) 90), 0);
    notes = runBlock (processor, in);
    expect (hasNotes (notes, true, { 60 }), "Keys Lock moves C# to C");
    setParam (processor, "keysLock", 0.0f); // turning it off while held must still release C
    in.clear();
    in.addEvent (juce::MidiMessage::noteOff (1, 61), 0);
    notes = runBlock (processor, in);
    expect (hasNotes (notes, false, { 60 }), "releasing the key releases the moved note");

    // Progression: C for 4 beats then Am for 4, played without a host
    // (120 bpm, 48 kHz: 4 beats = 96000 samples = 375 blocks).
    {
        madscaler::Progression progression;
        progression.slots.push_back ({ "C", { 60, 64, 67 }, 4.0 });
        progression.slots.push_back ({ "Am", { 57, 60, 64 }, 4.0 });
        processor.setProgression (progression);
        processor.setProgressionPlaying (true);

        std::vector<Note> first, second;
        for (int block = 0; block < 375; ++block)
            for (const auto& note : runBlock (processor))
                first.push_back (note);
        expect (hasNotes (first, true, { 60, 64, 67 }), "progression starts with C");
        expect (processor.getCurrentProgressionSlot() == 0, "first slot is current");

        for (int block = 0; block < 10; ++block)
            for (const auto& note : runBlock (processor))
                second.push_back (note);
        expect (hasNotes (second, true, { 57 }) && hasNotes (second, false, { 67 }),
                "Am follows after four beats, keeping the shared notes");
        expect (processor.getCurrentProgressionSlot() == 1, "second slot is current");

        processor.setProgressionPlaying (false);
        notes = runBlock (processor);
        expect (hasNotes (notes, false, { 57, 60, 64 }), "stopping releases the chord");
        expect (processor.getCurrentProgressionSlot() == -1, "no slot after stopping");

        // The progression is saved with the plugin state.
        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        MadScalerProcessor reloaded;
        reloaded.setStateInformation (saved.getData(), (int) saved.getSize());
        expect (reloaded.getProgression().slots.size() == 2
                    && reloaded.getProgression().slots[1].name == "Am"
                    && reloaded.getProgression().slots[1].notes == std::vector<int> { 57, 60, 64 },
                "progression survives a save and reload");
    }

    // MelodyMaker: the melody plays along with the progression on its own channel.
    {
        madscaler::Progression progression;
        progression.slots.push_back ({ "C", { 60, 64, 67 }, 4.0 });
        processor.setProgression (progression);

        MadScalerProcessor::MelodyState melody;
        melody.chords = progression.slots;
        melody.notes = { { 0.0, 1.0, 72, 100 }, { 1.0, 1.0, 74, 100 }, { 2.0, 2.0, 76, 90 } };
        melody.totalBeats = 4.0;
        processor.setMelodyState (melody);
        processor.setMelodyPlaying (true);
        processor.setProgressionPlaying (true);

        std::vector<Note> played;
        for (int block = 0; block < 370; ++block)
            for (const auto& note : runBlock (processor))
                played.push_back (note);

        std::vector<Note> melodyNotes;
        for (const auto& note : played)
            if (note.channel == 2)
                melodyNotes.push_back (note);

        expect (hasNotes (melodyNotes, true, { 72, 74, 76 }), "melody plays on channel 2");
        expect (hasNotes (melodyNotes, false, { 72, 74 }), "melody notes end on time");
        expect (hasNotes (played, true, { 60, 64, 67, 72, 74, 76 }), "chords and melody together");

        processor.setProgressionPlaying (false);
        notes = runBlock (processor);
        expect (hasNotes (notes, false, { 76, 60, 64, 67 }) || hasNotes (notes, false, { 60, 64, 67, 76 }),
                "stopping releases the melody too");

        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        MadScalerProcessor reloaded;
        reloaded.setStateInformation (saved.getData(), (int) saved.getSize());
        const auto& restoredMelody = reloaded.getMelodyState();
        expect (restoredMelody.notes.size() == 3 && restoredMelody.notes[2].note == 76
                    && std::abs (restoredMelody.notes[2].lengthBeats - 2.0) < 1.0e-9 && restoredMelody.chords.size() == 1
                    && std::abs (restoredMelody.totalBeats - 4.0) < 1.0e-9,
                "melody survives a save and reload");
        processor.setProgression ({});
    }

    // Bass: off until switched on, then plays on channel 3 next to the melody.
    {
        madscaler::Progression progression;
        progression.slots.push_back ({ "C", { 60, 64, 67 }, 4.0 });
        processor.setProgression (progression);

        MadScalerProcessor::MelodyState melody;
        melody.chords = progression.slots;
        melody.notes = { { 0.0, 1.0, 72, 100 } };
        melody.bass.style = madscaler::BassStyle::rootFifth;
        melody.bass.leadIn = true;
        melody.bassNotes = { { 0.0, 2.0, 36, 100 }, { 2.0, 2.0, 43, 100 } };
        melody.totalBeats = 4.0;
        processor.setMelodyState (melody);
        processor.setMelodyPlaying (true);
        expect (! processor.isBassPlaying(), "bass is off by default");

        auto playFor = [&processor] (int blocks)
        {
            std::vector<Note> result;
            for (int block = 0; block < blocks; ++block)
                for (const auto& note : runBlock (processor))
                    result.push_back (note);
            return result;
        };

        processor.setProgressionPlaying (true);
        auto played = playFor (200);
        bool anyBass = false;
        for (const auto& note : played)
            anyBass = anyBass || note.channel == 3;
        expect (! anyBass, "no bass while it is off");

        processor.setProgressionPlaying (false);
        playFor (2);
        processor.setBassPlaying (true);
        processor.setProgressionPlaying (true);
        played = playFor (370);
        std::vector<Note> bassNotes, melodyNotes;
        for (const auto& note : played)
        {
            if (note.channel == 3) bassNotes.push_back (note);
            if (note.channel == 2) melodyNotes.push_back (note);
        }
        expect (hasNotes (bassNotes, true, { 36, 43 }), "bass plays on channel 3");
        expect (hasNotes (melodyNotes, true, { 72 }), "melody still plays on channel 2");

        // Switching the bass off mid-note releases it.
        processor.setProgressionPlaying (false);
        playFor (2);
        processor.setProgressionPlaying (true);
        playFor (10);
        processor.setBassPlaying (false);
        const auto released = playFor (1);
        bool bassOff = false;
        for (const auto& note : released)
            bassOff = bassOff || (note.channel == 3 && ! note.on && note.number == 36);
        expect (bassOff, "switching the bass off releases its note");
        processor.setProgressionPlaying (false);
        playFor (2);

        processor.setBassPlaying (true);
        juce::MemoryBlock saved;
        processor.getStateInformation (saved);
        MadScalerProcessor reloaded;
        reloaded.setStateInformation (saved.getData(), (int) saved.getSize());
        const auto& restored = reloaded.getMelodyState();
        expect (restored.bassNotes.size() == 2 && restored.bassNotes[1].note == 43 && restored.bass.leadIn
                    && restored.bass.style == madscaler::BassStyle::rootFifth && reloaded.isBassPlaying(),
                "bass survives a save and reload");
        processor.setBassPlaying (false);
        processor.setProgression ({});
    }

    // State round trip.
    setParam (processor, "scale", 1.0f);
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    MadScalerProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    expect (restored.getScaleIndex() == 1, "state restores the scale");

    // Library progressions load in the current key, voice-led.
    {
        setParam (processor, "key", 7.0f); // G
        setParam (processor, "inversion", 0.0f);
        const madscaler::LibraryProgression* anthem = nullptr;
        for (const auto& entry : madscaler::getLibrary())
            if (std::string (entry.name) == "Four chord anthem")
                anthem = &entry;

        expect (anthem != nullptr, "library has the four chord anthem");
        if (anthem != nullptr)
        {
            const auto loaded = processor.buildFromLibrary (*anthem);
            expect (loaded.slots.size() == 4 && loaded.slots[0].name == "G" && loaded.slots[3].name == "C",
                    "anthem loads in G");
            if (loaded.slots.size() == 4)
                expect (madscaler::voiceLeadingDistance (loaded.slots[0].notes, loaded.slots[1].notes)
                            < madscaler::voiceLeadingDistance (loaded.slots[0].notes, { 74, 78, 81 }),
                        "library chords are voice-led (closer than root-position D)");
        }
        setParam (processor, "key", 0.0f);
    }

    if (failures == 0)
        std::cout << "All processor tests passed\n";

    return failures == 0 ? 0 : 1;
}
