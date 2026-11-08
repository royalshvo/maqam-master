#include "core/NoteNames.h"

#include <cmath>
#include <cstdlib>
#include <sstream>

namespace maqam {

namespace {

constexpr char kLetters[7] = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};
constexpr int kNaturalSemitones[7] = {0, 2, 4, 5, 7, 9, 11};

int floorDiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        --q;
    return q;
}

int letterIndex(char letter) {
    for (int i = 0; i < 7; ++i)
        if (kLetters[i] == letter)
            return i;
    return 0;
}

// Letter index of a natural note at this quarter-tone pitch class (0..23), or -1.
int naturalAt(int pcQ) {
    for (int i = 0; i < 7; ++i)
        if (kNaturalSemitones[i] * 2 == pcQ)
            return i;
    return -1;
}

std::string accidentalWords(int q) {
    switch (q) {
        case -4: return "double-flat";
        case -3: return "three-quarter-flat";
        case -2: return "flat";
        case -1: return "half-flat";
        case 0:  return "";
        case 1:  return "half-sharp";
        case 2:  return "sharp";
        case 3:  return "three-quarter-sharp";
        case 4:  return "double-sharp";
        default: return q < 0 ? "flat x" + std::to_string(-q) : "sharp x" + std::to_string(q);
    }
}

std::string accidentalSymbols(int q) {
    const std::string flat = "\xE2\x99\xAD";          // U+266D
    const std::string sharp = "\xE2\x99\xAF";         // U+266F
    const std::string halfFlat = "\xF0\x9D\x84\xB3";  // U+1D133 quarter-tone flat
    const std::string halfSharp = "\xF0\x9D\x84\xB2"; // U+1D132 quarter-tone sharp
    switch (q) {
        case -4: return flat + flat;
        case -3: return flat + halfFlat;
        case -2: return flat;
        case -1: return halfFlat;
        case 0:  return "";
        case 1:  return halfSharp;
        case 2:  return sharp;
        case 3:  return sharp + halfSharp;
        case 4:  return sharp + sharp;
        default: return accidentalWords(q);
    }
}

std::string residualText(double cents) {
    if (std::abs(cents) < 0.5)
        return "";
    std::ostringstream s;
    s << " (" << (cents > 0 ? "+" : "") << std::lround(cents) << " cents)";
    return s.str();
}

// Absolute cents (above MIDI 0) of a natural letter in a scientific octave.
double naturalCents(int letterIdx, int octave) {
    return ((octave + 1) * 12 + kNaturalSemitones[letterIdx]) * 100.0;
}

} // namespace

std::string NoteName::words(bool withOctave) const {
    std::string s(1, letter);
    const std::string acc = accidentalWords(quarterTones);
    if (!acc.empty())
        s += " " + acc;
    if (withOctave)
        s += " " + std::to_string(octave);
    return s + residualText(residualCents);
}

std::string NoteName::symbols(bool withOctave) const {
    std::string s(1, letter);
    s += accidentalSymbols(quarterTones);
    if (withOctave)
        s += std::to_string(octave);
    return s + residualText(residualCents);
}

std::string NoteName::compact(bool withOctave, bool withResidual) const {
    const std::string flat = "\xE2\x99\xAD";   // ♭
    const std::string sharp = "\xE2\x99\xAF";  // ♯
    const std::string half = "\xC2\xBD";       // ½
    std::string acc;
    switch (quarterTones) {
        case -4: acc = flat + flat; break;
        case -3: acc = "1" + half + flat; break;
        case -2: acc = flat; break;
        case -1: acc = half + flat; break;
        case 0:  break;
        case 1:  acc = half + sharp; break;
        case 2:  acc = sharp; break;
        case 3:  acc = "1" + half + sharp; break;
        case 4:  acc = sharp + sharp; break;
        default: acc = accidentalWords(quarterTones); break;
    }
    std::string s = std::string(1, letter) + acc;
    if (withOctave)
        s += std::to_string(octave);
    return withResidual ? s + residualText(residualCents) : s;
}

NoteName spellTonic(const Tonic& tonic) {
    const double abs = tonic.absoluteCents();
    const int qt = static_cast<int>(std::lround(abs / 50.0));
    const int pcQ = qt - floorDiv(qt, 24) * 24;
    const int octave = floorDiv(qt, 24) - 1;

    NoteName n;
    n.residualCents = abs - qt * 50.0;
    n.octave = octave;

    if (const int nat = naturalAt(pcQ); nat >= 0) {
        n.letter = kLetters[nat];
        return n;
    }
    if (pcQ % 2 == 1) {
        // Quarter tone: half-flat of the natural above, except above E and B (whose upper
        // neighbours F and C are a semitone away), where a half-sharp reads better.
        const int upper = naturalAt((pcQ + 1) % 24);
        if (upper >= 0 && kLetters[upper] != 'F' && kLetters[upper] != 'C') {
            n.letter = kLetters[upper];
            n.quarterTones = -1;
            return n;
        }
        const int lower = naturalAt(pcQ - 1);
        n.letter = kLetters[lower];
        n.quarterTones = 1;
        return n;
    }
    // Semitone between naturals: flat of the natural above.
    n.letter = kLetters[naturalAt(pcQ + 2)];
    n.quarterTones = -2;
    return n;
}

std::vector<NoteName> spellScale(const ResolvedScale& scale, const Tonic& tonic) {
    const NoteName t = spellTonic(tonic);
    const int tonicLetter = letterIndex(t.letter);
    const double tonicAbs = tonic.absoluteCents();

    std::vector<NoteName> out;
    for (int d = 1; d <= scale.numDegrees(); ++d) {
        const int steps = tonicLetter + (d - 1);
        const int letter = steps % 7;
        const int octave = t.octave + steps / 7;
        const double abs = tonicAbs + scale.cents[static_cast<size_t>(d - 1)];
        const double accidental = abs - naturalCents(letter, octave);

        NoteName n;
        n.letter = kLetters[letter];
        n.octave = octave;
        n.quarterTones = static_cast<int>(std::lround(accidental / 50.0));
        n.residualCents = accidental - n.quarterTones * 50.0;
        // Double and three-quarter accidentals (Saba on F would give B double-flat) are
        // correct by letter but unreadable: respell those on the nearest simple name.
        if (std::abs(n.quarterTones) >= 3)
            n = spellTonic(Tonic{0, abs});
        out.push_back(n);
    }
    return out;
}

NoteName spellKey(int midiNote) {
    return spellTonic(Tonic{midiNote, 0.0});
}

} // namespace maqam
