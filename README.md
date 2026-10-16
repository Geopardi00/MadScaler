# MadScaler

A chord and scale MIDI plugin (VST3, Windows first). Pick a key and a scale,
and MadScaler lays out the diatonic chords of that scale as a grid of pads.
Click and hold a pad to play the chord. The chords go out as MIDI, so you route
them to any synth in your DAW: MadScaler has no sounds of its own.

This is an independent project. It is not affiliated with, and contains no
code, presets or assets from, any commercial chord plugin.

## Features (v0.1)

- Key picker (all 12 keys) and 11 seven-note scales: Major, Natural/Harmonic/
  Melodic Minor, the modes, Harmonic Major and Phrygian Dominant.
- Grid of the seven diatonic chords with chord names, roman numerals and notes.
- Triads or 7th chords, octave, velocity and MIDI channel controls.
- Optional: trigger the chords from MIDI keys (C = I ... B = vii).
- On-screen keyboard showing the scale and the notes being sent.
- Settings are saved with your DAW project.

Planned next: chord detection, voicings and inversions, performance patterns.

## Download

Every push builds the plugin in GitHub Actions. Open the latest run under the
**Actions** tab, download **MadScaler-Windows**, unzip it and copy
`MadScaler.vst3` to `C:\Program Files\Common Files\VST3\`. Then rescan plugins
in your DAW.

See [docs/ROUTING.md](docs/ROUTING.md) for how to send MadScaler's MIDI to a
synth in REAPER, Ableton Live, FL Studio, Cubase and Bitwig.

## Building

Requirements: CMake 3.22+, a C++17 compiler (Visual Studio 2022 on Windows).
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
- `Source/PluginProcessor.*` turns pad presses and MIDI input into MIDI chords.
- `Source/PluginEditor.*` the user interface.
- `tests/` unit tests for the theory code and the processor's MIDI output.
