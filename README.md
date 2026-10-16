# MadScaler

A chord and scale MIDI plugin (VST3, Windows first). Pick a key and a scale,
and MadScaler lays out the diatonic chords of that scale as a grid of pads.
Click and hold a pad to play the chord. The chords go out as MIDI, so you route
them to any synth in your DAW: MadScaler has no sounds of its own.

This is an independent project. It is not affiliated with, and contains no
code, presets or assets from, any commercial chord plugin.

## Features (v0.7)

- Key picker (all 12 keys) and 11 seven-note scales: Major, Natural/Harmonic/
  Melodic Minor, the modes, Harmonic Major and Phrygian Dominant.
- Grid of the seven diatonic chords with chord names, roman numerals and notes.
- Triads or 7th chords, octave, velocity and MIDI channel controls.
- Voicings: root position or 1st/2nd/3rd inversion, Close, Open, Drop 2 or
  Drop 3 spacing, an optional bass note an octave below, and voice leading,
  which picks the inversion that moves least from the previous chord.
- Patterns: Strum Up/Down (adjustable strum time), Arp Up, Down, Up/Down and
  Random over 1-3 octaves, or rhythmic Pulse. Rate from 1/4 to 1/32 including
  triplets, with a gate control. Steps lock to the DAW's tempo and beat grid
  while it plays and run at the DAW tempo (or 120 bpm) when stopped. Patterns
  apply to chords from the pads, the MIDI-key trigger and detected chords;
  notes passed straight through are not patterned.
- Optional: trigger the chords from MIDI keys (C = I ... B = vii).
- On-screen keyboard showing the scale and the notes being sent. Click a key
  to play a single note.
- Chord detection: play chords into MadScaler (MIDI keyboard, a MIDI clip on
  its track, or the on-screen keys) and it names them (triads, sevenths,
  sus, 6, add9, 9ths, 11ths, 13ths, slash chords). The last 8 chords are kept
  as clickable chips that replay what you played, and MadScaler suggests the
  keys and scales that fit them; click one to set the chord grid to it.
- Keys Lock: MIDI notes you play are snapped to the nearest note of the
  selected scale, so every key on your keyboard is "right".
- Progression builder: turn on "Add chords", then click chord pads or
  detected chips to add them to a strip of up to 16 slots. Click a slot to
  change its length (1 to 16 beats), move, duplicate or delete it. "Play"
  loops the progression in time with your DAW (using the current pattern
  and voicing settings), "Export MIDI" saves it as a .mid file and the
  "Drag MIDI" handle drags it straight onto a DAW track.
- Chord colours: press a pad and the Colours panel shows richer versions of
  that chord (6, 7, 9, add9, 11, 13, sus2, sus4), the same chord borrowed
  from the parallel minor/major and other modes, and secondary dominants
  (V7 of the chord, vii°7 of the chord and the tritone sub). Click a colour
  to play it, or add it to the progression with "Add chords" on.
- Next-chord hints: after a chord is played, the pads that usually come next
  are marked "next".
- Circle of Fifths: the chords of the current scale are highlighted. Click a
  segment to play that chord; right-click it to change key.
- Progression library: 117 of MadScaler's own progressions, sorted by style
  (pop, rock, metal, EDM, synthwave, hip-hop, R&B, gospel, funk, jazz, lo-fi,
  blues, folk, Latin, reggae, classical), mood (happy, heroic, epic, dark,
  sad, dreamy and more) and use (film score, trailer, game music, battle,
  fantasy, sci-fi, horror, ambient, meditation and more). Pick one in the
  Library tab and Load (or Append) it: it arrives in the current key,
  voice-led, and the pads switch to its scale.
- Edit any chord of the progression: click it to hear it and select it, then
  press a pad, colour, Circle of Fifths or detected chord to replace it
  (the length stays). Right-click a chord for "Replace with" the scale's
  chords, length, move, duplicate and delete. Then Drag MIDI into your DAW.
- Settings and the progression are saved with your DAW project.

Next up: motions (basslines and melodies).

## Download

Every push builds the plugin in GitHub Actions. Open the latest run under the
**Actions** tab, download **MadScaler-Windows**, unzip it and copy
`MadScaler.vst3` to `C:\Program Files\Common Files\VST3\`. Then rescan plugins
in your DAW.

See [docs/ROUTING.md](docs/ROUTING.md) for how to send MadScaler's MIDI to a
synth in REAPER, Ableton Live, FL Studio, Cubase and Bitwig.

## Building

Requirements: CMake 3.22+, a C++17 compiler (Visual Studio 2022 or newer on Windows).
JUCE 8 is downloaded automatically during configure.

```
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

The VST3 ends up in `build/MadScaler_artefacts/Release/VST3/`.

## Code layout

- `Source/ChordEngine.*` music theory: scales, diatonic chords, chord names.
  Plain C++ without JUCE.
- `Source/ChordDetector.*` names played chords and ranks fitting keys/scales.
- `Source/Voicing.*` inversions, open/drop voicings, bass note, voice leading.
- `Source/PatternEngine.*` tempo-synced strums, arpeggios and pulses.
- `Source/Progression.*` chord progressions and their MIDI rendering.
- `Source/ChordColours.*` extensions, borrowed and secondary chords, next-chord hints.
- `Source/ChordLibrary.*` the progression library and its Roman numeral parser.
- `Source/PluginProcessor.*` turns pad presses and MIDI input into MIDI chords.
- `Source/PluginEditor.*` the user interface.
- `tests/` unit tests for the theory code, chord detection, voicings,
  patterns, progressions, chord colours, the library and the processor's MIDI output.
