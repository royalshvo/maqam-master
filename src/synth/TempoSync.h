#pragma once

// Tempo sync: note values (1/4, 1/8T, 1/16. ...) turned into seconds or a rate at a given
// tempo, so glide and vibrato can follow the song. The plugin lists these names in its
// "Glide note" and "Vibrato note" menus. UI-free, no JUCE.

#include <array>

namespace maqam::synth {

struct NoteValue {
    const char* name;  // "1/4", "1/8." (dotted), "1/8T" (triplet)
    double beats;      // length in quarter notes
};

inline constexpr std::array<NoteValue, 15> kNoteValues{{
    {"1/1", 4.0},          {"1/2", 2.0},          {"1/2.", 3.0},
    {"1/4", 1.0},          {"1/4.", 1.5},         {"1/4T", 2.0 / 3.0},
    {"1/8", 0.5},          {"1/8.", 0.75},        {"1/8T", 1.0 / 3.0},
    {"1/16", 0.25},        {"1/16.", 0.375},      {"1/16T", 1.0 / 6.0},
    {"1/32", 0.125},       {"1/32T", 1.0 / 12.0}, {"1/64", 0.0625},
}};

/** Tempo used when the host gives none (or a nonsense one). */
inline constexpr double kDefaultBpm = 120.0;

/** Index into kNoteValues of a name, or -1. */
int noteValueIndex(const char* name);

/** Length of a note value in seconds at this tempo. The index is clamped to the list. */
double noteValueSeconds(int index, double bpm);

/** Vibrato rate in Hz for one cycle per note value, clamped to minHz..maxHz. */
double noteValueRateHz(int index, double bpm, double minHz = 0.25, double maxHz = 20.0);

} // namespace maqam::synth
