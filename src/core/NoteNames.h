#pragma once

// Note spelling with quarter-tone accidentals, for any tonic. UI-free.
//
// Degrees are spelled on consecutive letters from the tonic (D E F G A B C for a maqam on D),
// with accidentals in quarter tones. A half-flat is the usual Arabic quarter-tone accidental,
// so Rast on F reads F G A-half-flat B-flat C D E-half-flat. A degree that would need a double or
// three-quarter accidental is respelled on its simplest name. Values that are not a whole number
// of quarter tones (from tuning presets) keep the nearest spelling plus a cents remainder.

#include <string>
#include <vector>

#include "core/Tuning.h"

namespace maqam {

struct NoteName {
    char letter = 'C';            // 'A'..'G'
    int quarterTones = 0;         // accidental: -1 half-flat, -2 flat, +1 half-sharp, +2 sharp ...
    int octave = 4;               // scientific octave: MIDI 60 = C4
    double residualCents = 0.0;   // remainder after the accidental, e.g. -10 for a lowered preset

    /** "E half-flat", "B flat", "F sharp 4" ... (ASCII-safe words). */
    std::string words(bool withOctave = false) const;

    /** "E𝄳", "B♭", "F♯4" ... (Unicode accidentals; check the font supports U+1D132/U+1D133). */
    std::string symbols(bool withOctave = false) const;

    /** "A½♭", "B♭", "F♯" ... using only widely supported characters (½ ♭ ♯), for UIs and
        Ableton note names. */
    std::string compact(bool withOctave = false, bool withResidual = true) const;

    bool operator==(const NoteName& o) const {
        return letter == o.letter && quarterTones == o.quarterTones && octave == o.octave;
    }
};

/** Spelling of the tonic itself: naturals first, then half-flats, half-sharps and flats. */
NoteName spellTonic(const Tonic& tonic);

/** Spelling of every degree (degree 1 first) on consecutive letters from the tonic. */
std::vector<NoteName> spellScale(const ResolvedScale& scale, const Tonic& tonic);

/** Plain 12-TET spelling of a MIDI key, preferring flats (for keys outside the maqam). */
NoteName spellKey(int midiNote);

} // namespace maqam
