#pragma once

#include <vector>

// Chord voicing for MadScaler: inversions, open and drop voicings, an added bass
// note and voice leading. Plain C++ with no JUCE dependency.
namespace madscaler
{
    enum class VoicingStyle
    {
        close,  // notes stacked as tightly as possible
        open,   // every other note raised an octave, for a wider sound
        drop2,  // second-highest note dropped an octave
        drop3   // third-highest note dropped an octave
    };

    struct VoicingSettings
    {
        int inversion = 0; // 0 = root position, 1 = first inversion ...
        VoicingStyle style = VoicingStyle::close;
        bool addBass = false;     // add the chord root an octave below the voicing
        bool voiceLeading = false; // pick the inversion closest to the previous chord
    };

    // Takes a root-position close chord (ascending MIDI notes, root first) and
    // returns the voiced notes, ascending and limited to 0-127.
    // previousNotes is the last chord played; it is only used for voice leading.
    std::vector<int> voiceChord (const std::vector<int>& rootPositionNotes,
                                 const VoicingSettings& settings,
                                 const std::vector<int>& previousNotes);

    // Movement between two chords: for each note of the new chord, the distance
    // in semitones to the nearest note of the old chord, summed.
    int voiceLeadingDistance (const std::vector<int>& from, const std::vector<int>& to);
} // namespace madscaler
