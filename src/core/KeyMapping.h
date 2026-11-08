#pragma once

// Keyboard mapping: which frequency each MIDI key plays for a maqam, and export of that
// mapping as Scala (.scl/.kbm) and Ableton Live 12 (.ascl) tuning files. UI-free.
//
// Two layouts, so both can be tried by ear before choosing the Play Mode default:
//   TwelveKey - every chromatic key keeps its place; keys nearest a maqam note are retuned to
//               it (a half-flat goes on the upper key: Rast on F puts A half-flat on the A key).
//               Keys outside the maqam stay in 12-TET.
//   Degrees   - consecutive keys play consecutive degrees (Live's native behaviour for scales
//               that don't have 12 notes); the tonic stays on the tonic key.

#include <string>
#include <vector>

#include "core/MaqamData.h"
#include "core/Tuning.h"

namespace maqam {

enum class KeyLayout { TwelveKey, Degrees };

/** A periodic key-to-pitch table anchored on the tonic key. */
struct TuningTable {
    KeyLayout layout = KeyLayout::TwelveKey;
    std::vector<double> stepCents;    // one entry per key in a period; [0] = 0 (the tonic key)
    std::vector<int> degreeOfStep;    // 1-based maqam degree on that step, 0 if outside the maqam
    std::vector<bool> alternateOfStep; // true when the step holds a descending-form note
    std::vector<int> unplacedAlternateDegrees; // descending notes that found no free key
    double periodCents = 1200.0;
    int tonicMidiNote = kDefaultTonicMidiNote;
    double tonicHz = 0.0;

    int stepsPerPeriod() const { return static_cast<int>(stepCents.size()); }
};

/** Builds the TwelveKey table. Throws std::runtime_error if two maqam notes need the same key
    and neither neighbouring key is free. */
TuningTable makeTwelveKeyTable(const ResolvedScale& scale, const Tonic& tonic, double a4Hz = kDefaultA4Hz);

/** Builds the TwelveKey table for a maqam: the ascending form (or an ascending variant), plus
    the descending form's changed notes on free neighbouring keys, so both are under the hands
    at once (Rast on F: E key = E half-flat going up, E-flat key = E-flat coming down).
    Descending notes with no free key are listed in unplacedAlternateDegrees. */
TuningTable makeTwelveKeyTable(const Maqam& maqam, const Tonic& tonic, double a4Hz = kDefaultA4Hz,
                               const std::string& variantId = {}, const TuningPreset* preset = nullptr);

/** Builds the Degrees table. For a scale that does not repeat at the octave (Saba), every listed
    degree becomes a step and the period is 1200 cents: an approximation until the upper register
    is reviewed. */
TuningTable makeDegreesTable(const ResolvedScale& scale, const Tonic& tonic, double a4Hz = kDefaultA4Hz);

/** Frequency of a MIDI key under the table. */
double keyFrequencyHz(const TuningTable& table, int midiNote);

/** 1-based maqam degree a MIDI key plays, or 0 if the key is outside the maqam. */
int keyDegree(const TuningTable& table, int midiNote);

/** Scala .scl text. */
std::string toScl(const TuningTable& table, const std::string& description);

/** Scala .kbm text: linear mapping with the tonic on its key and the tonic frequency as reference. */
std::string toKbm(const TuningTable& table);

/** Ableton Live 12 .ascl text (a .scl with @ABL directives). Pins MIDI 0 as the first note so
    the tonic lands on its own key. If noteNames is given (one per step, step 0 first) it is
    written as NOTE_NAMES. */
std::string toAscl(const TuningTable& table, const std::string& description, const std::string& source,
                   const std::vector<std::string>& noteNames = {});

/** Compact name of every step in a table (step 0 first): maqam steps use the maqam spelling
    (descending spelling for alternate steps), other keys their plain 12-TET name. */
std::vector<std::string> stepNoteNames(const TuningTable& table, const ResolvedScale& primary,
                                       const ResolvedScale* alternate, const Tonic& tonic);

} // namespace maqam
