# Routing MadScaler's MIDI to a synth

MadScaler makes no sound. It loads as an instrument, sends out MIDI chords, and
you point that MIDI at any synth you like. The audio output of MadScaler is
always silent.

Two settings matter in every DAW:

- **MIDI Ch** in MadScaler picks the channel the chords go out on (default 1).
  Most synths listen on all channels, so you rarely need to change it.
- **Play chords from MIDI keys**: when it is off, notes from your keyboard pass
  straight through MadScaler to the synth. When it is on, white keys trigger
  the chord grid (C = I, D = ii, ... B = vii in any octave) and black keys
  still pass through.

## REAPER (simplest)

Put both plugins in one track's FX chain, MadScaler first:

1. Insert a new track and open its FX window.
2. Add **VST3i: MadScaler**, then add your synth below it.
3. Arm the track if you want to play from a MIDI keyboard.

REAPER passes MIDI down the FX chain, so the synth plays whatever MadScaler
sends. Record the chords as MIDI by recording the track with **Record: output
(MIDI)** on the track's record-arm menu.

Alternative with two tracks: MadScaler on track 1, synth on track 2, then on
track 1's **Route** button add a send to track 2 with audio set to **None** and
MIDI set to **All → All**.

## Ableton Live

1. Put MadScaler on a MIDI track (track A).
2. Put your synth on a second MIDI track (track B).
3. On track B, set **MIDI From** to track A, and in the dropdown under it pick
   **MadScaler**.
4. Set track B's **Monitor** to **In**.

To record the chords, arm track B and record there.

## FL Studio

1. Add MadScaler to the Channel Rack.
2. Open its plugin wrapper settings (the gear icon) and set **MIDI Output port**
   to a number, for example 1.
3. Add your synth, open its wrapper settings and set **MIDI Input port** to the
   same number.

## Cubase / Nuendo

1. Add MadScaler as an Instrument Track.
2. Add your synth as another Instrument Track.
3. On the synth track, set the input to **MadScaler – MIDI Out** and turn on
   monitoring.

## Bitwig Studio

Put MadScaler and your synth on the same instrument track, MadScaler first.
Bitwig forwards MadScaler's note output to the next device.

## Standalone app

`MadScaler.exe` runs without a DAW. Open **Options → Audio/MIDI Settings** and
pick a MIDI output (for example a loopMIDI port) to send chords to another
program.
