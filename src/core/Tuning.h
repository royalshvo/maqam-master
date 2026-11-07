#pragma once

// Tuning math: resolve a maqam form to cents, convert cents to Hz for a selectable tonic,
// detect notes off the 12-TET grid, and look up note roles. UI-free.

#include <optional>
#include <string>
#include <vector>

#include "core/MaqamData.h"

namespace maqam {

constexpr double kDefaultA4Hz = 440.0;
constexpr int kDefaultTonicMidiNote = 65;   // F4

/** The playing tonic: a MIDI note plus a cents offset (e.g. E half-flat = MIDI 64, -50 cents). */
struct Tonic {
    int midiNote = kDefaultTonicMidiNote;
    double centsOffset = 0.0;

    /** Absolute pitch in cents above MIDI note 0. */
    double absoluteCents() const { return midiNote * 100.0 + centsOffset; }

    /** Tonic from a pitch class in cents above C (0..1199) in a given MIDI octave
        (octave 4 = the octave starting at middle C, MIDI 60). */
    static Tonic fromCentsAboveC(double centsAboveC, int octave = 4);
};

enum class Direction { Ascending, Descending };

/** One maqam form resolved to concrete cents. Degrees are listed in degree order (degree 1
    first) for both directions; playback order is the caller's choice. */
struct ResolvedScale {
    std::vector<double> cents;
    std::vector<JinsPlacement> ajnas;
    bool octaveRepeating = true;

    int numDegrees() const { return static_cast<int>(cents.size()); }
};

/** Resolves a maqam form.
    - Ascending: base scale, or the named ascending variant if variantId is not empty.
    - Descending: returns nullopt when the data does not specify a descending form.
    A tuning preset's per-degree offsets are applied last.
    Throws std::invalid_argument for an unknown variant, or a variant with Descending. */
std::optional<ResolvedScale> resolveScale(const Maqam& maqam, Direction direction,
                                          const std::string& variantId = {},
                                          const TuningPreset* preset = nullptr);

double centsToRatio(double cents);
double midiNoteToHz(double midiNote, double a4Hz = kDefaultA4Hz);

/** Frequency of a pitch given in cents above the tonic. */
double frequencyHz(const Tonic& tonic, double centsFromTonic, double a4Hz = kDefaultA4Hz);

/** Frequency of a 1-based degree, shifted by whole octaves (1200 cents each). */
double degreeFrequencyHz(const ResolvedScale& scale, const Tonic& tonic, int degree,
                         int octaveShift = 0, double a4Hz = kDefaultA4Hz);

/** True when the pitch is more than toleranceCents away from the nearest 12-TET note. */
bool isOffTwelveTetGrid(const Tonic& tonic, double centsFromTonic, double toleranceCents = 1.0);

/** Note roles, in priority order. Names match "note_roles" in maqams.json. */
enum class NoteRole { Tonic, Ghammaz, Characteristic, QuarterTone, Passing };

const char* toString(NoteRole role);
const std::vector<NoteRole>& allNoteRoles();

/** Every role a 1-based degree has for this tonic. Passing only when no other role applies.
    The octave of the tonic counts as tonic when the scale repeats at the octave. */
std::vector<NoteRole> rolesForDegree(const Maqam& maqam, const ResolvedScale& scale,
                                     const Tonic& tonic, int degree);

/** The highest-priority role of a degree (Tonic > Ghammaz > Characteristic > QuarterTone > Passing). */
NoteRole primaryRole(const Maqam& maqam, const ResolvedScale& scale, const Tonic& tonic, int degree);

} // namespace maqam
