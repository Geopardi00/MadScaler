# MadScaler

A chord and scale MIDI plugin (VST3, Windows first). Pick a key and a scale,
and MadScaler lays out the diatonic chords of that scale as a grid of pads.
Click and hold a pad to play the chord. The chords go out as MIDI, so you route
them to any synth in your DAW: MadScaler has no sounds of its own.

This is an independent project. It is not affiliated with, and contains no
code, presets or assets from, any commercial chord plugin.

## Features (v0.5)

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
- Settings and the progression are saved with your DAW project.

Next up: chord colours (extensions, borrowed chords), a chord set library
and motions (basslines and melodies).

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
- `Source/PluginProcessor.*` turns pad presses and MIDI input into MIDI chords.
- `Source/PluginEditor.*` the user interface.
- `tests/` unit tests for the theory code, chord detection, voicings,
  patterns, progressions and the processor's MIDI output.
