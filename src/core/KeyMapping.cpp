#include "core/KeyMapping.h"

#include "core/NoteNames.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace maqam {

namespace {

int floorDiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        --q;
    return q;
}

int positiveMod(int a, int b) {
    return a - floorDiv(a, b) * b;
}

std::string formatCents(double cents) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(3) << cents;
    return s.str();
}

std::string formatHz(double hz) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(6) << hz;
    return s.str();
}

// Scala description lines must be a single line.
std::string oneLine(std::string text) {
    for (char& c : text)
        if (c == '\n' || c == '\r')
            c = ' ';
    return text;
}

TuningTable emptyTwelveKeyTable(const Tonic& tonic, double a4Hz) {
    TuningTable t;
    t.layout = KeyLayout::TwelveKey;
    t.tonicMidiNote = tonic.midiNote;
    t.tonicHz = frequencyHz(tonic, 0.0, a4Hz);
    t.periodCents = 1200.0;
    t.stepCents.resize(12);
    t.degreeOfStep.assign(12, 0);
    t.alternateOfStep.assign(12, false);
    // Keys outside the maqam stay in 12-TET, measured from the (possibly detuned) tonic.
    for (int k = 0; k < 12; ++k)
        t.stepCents[static_cast<size_t>(k)] = k * 100.0 - tonic.centsOffset;
    return t;
}

// Puts a note (cents above the tonic) on the nearest free key: the nearest key first (a tie,
// i.e. a half-flat or half-sharp, goes to the upper key), then the other neighbour.
bool placeOnKey(TuningTable& t, const Tonic& tonic, double cents, int degree, bool alternate) {
    const double tonicAbs = tonic.absoluteCents();
    const double abs = tonicAbs + cents;
    const int nearest = static_cast<int>(std::floor(abs / 100.0 + 0.5));
    const double frac = abs / 100.0 - std::floor(abs / 100.0);
    const int other = (std::abs(frac - 0.5) < 1e-6) ? nearest - 1
                      : (abs / 100.0 < nearest ? nearest - 1 : nearest + 1);
    for (int key : {nearest, other}) {
        const auto step = static_cast<size_t>(positiveMod(key - tonic.midiNote, 12));
        if (t.degreeOfStep[step] == 0) {
            // Cents of this note above the tonic, within the period that starts on the tonic key.
            t.stepCents[step] = abs - tonicAbs - 1200.0 * floorDiv(key - tonic.midiNote, 12);
            t.degreeOfStep[step] = degree;
            t.alternateOfStep[step] = alternate;
            return true;
        }
    }
    return false;
}

bool isOctave(const ResolvedScale& scale, int degree) {
    return scale.octaveRepeating &&
           std::abs(scale.cents[static_cast<size_t>(degree - 1)] - 1200.0) < 0.5;
}

} // namespace

TuningTable makeTwelveKeyTable(const ResolvedScale& scale, const Tonic& tonic, double a4Hz) {
    TuningTable t = emptyTwelveKeyTable(tonic, a4Hz);
    // The tonic always sits on its own key.
    t.stepCents[0] = 0.0;
    t.degreeOfStep[0] = 1;
    for (int d = 2; d <= scale.numDegrees(); ++d) {
        if (isOctave(scale, d))
            continue;   // the octave is the tonic again
        if (!placeOnKey(t, tonic, scale.cents[static_cast<size_t>(d - 1)], d, false))
            throw std::runtime_error("degree " + std::to_string(d) + " has no free key in the 12-key layout");
    }
    return t;
}

TuningTable makeTwelveKeyTable(const Maqam& maqam, const Tonic& tonic, double a4Hz,
                               const std::string& variantId, const TuningPreset* preset) {
    const auto up = resolveScale(maqam, Direction::Ascending, variantId, preset);
    TuningTable t = makeTwelveKeyTable(*up, tonic, a4Hz);

    if (const auto down = resolveScale(maqam, Direction::Descending, {}, preset)) {
        for (int d = 1; d <= down->numDegrees(); ++d) {
            const double cents = down->cents[static_cast<size_t>(d - 1)];
            if (isOctave(*down, d) || std::abs(cents - up->cents[static_cast<size_t>(d - 1)]) < 0.5)
                continue;   // same note as ascending
            if (!placeOnKey(t, tonic, cents, d, true))
                t.unplacedAlternateDegrees.push_back(d);
        }
    }
    return t;
}

TuningTable makeDegreesTable(const ResolvedScale& scale, const Tonic& tonic, double a4Hz) {
    TuningTable t;
    t.layout = KeyLayout::Degrees;
    t.tonicMidiNote = tonic.midiNote;
    t.tonicHz = frequencyHz(tonic, 0.0, a4Hz);
    t.periodCents = 1200.0;
    int n = scale.numDegrees();
    if (scale.octaveRepeating && std::abs(scale.cents.back() - 1200.0) < 0.5)
        --n;   // the last degree is the octave, i.e. the next period
    for (int d = 1; d <= n; ++d) {
        t.stepCents.push_back(scale.cents[static_cast<size_t>(d - 1)]);
        t.degreeOfStep.push_back(d);
        t.alternateOfStep.push_back(false);
    }
    return t;
}

double keyFrequencyHz(const TuningTable& t, int midiNote) {
    const int n = t.stepsPerPeriod();
    const int steps = midiNote - t.tonicMidiNote;
    const int period = floorDiv(steps, n);
    const int index = steps - period * n;
    return t.tonicHz * centsToRatio(period * t.periodCents + t.stepCents[static_cast<size_t>(index)]);
}

int keyDegree(const TuningTable& t, int midiNote) {
    return t.degreeOfStep[static_cast<size_t>(positiveMod(midiNote - t.tonicMidiNote, t.stepsPerPeriod()))];
}

std::string toScl(const TuningTable& t, const std::string& description) {
    std::ostringstream s;
    s << "! Generated by Maqam Master\n";
    s << oneLine(description) << "\n";
    s << " " << t.stepsPerPeriod() << "\n";
    s << "!\n";
    for (int i = 1; i < t.stepsPerPeriod(); ++i)
        s << " " << formatCents(t.stepCents[static_cast<size_t>(i)]) << "\n";
    s << " " << formatCents(t.periodCents) << "\n";
    return s.str();
}

std::string toKbm(const TuningTable& t) {
    std::ostringstream s;
    s << "! Generated by Maqam Master. Linear mapping, tonic on its own key.\n";
    s << "! Map size (0 = linear)\n0\n";
    s << "! First and last MIDI note\n0\n127\n";
    s << "! Middle note (scale degree 0)\n" << t.tonicMidiNote << "\n";
    s << "! Reference note and frequency\n" << t.tonicMidiNote << "\n" << formatHz(t.tonicHz) << "\n";
    s << "! Scale degree of the formal octave\n" << t.stepsPerPeriod() << "\n";
    return s.str();
}

std::vector<std::string> stepNoteNames(const TuningTable& t, const ResolvedScale& primary,
                                       const ResolvedScale* alternate, const Tonic& tonic) {
    const auto up = spellScale(primary, tonic);
    const auto down = alternate ? spellScale(*alternate, tonic) : up;
    std::vector<std::string> names;
    for (int k = 0; k < t.stepsPerPeriod(); ++k) {
        const auto i = static_cast<size_t>(k);
        const int d = t.degreeOfStep[i];
        if (d > 0) {
            const bool alt = i < t.alternateOfStep.size() && t.alternateOfStep[i];
            names.push_back((alt ? down : up)[static_cast<size_t>(d - 1)].compact(false, false));
        } else {
            names.push_back(spellKey(t.tonicMidiNote + k).compact(false, false));
        }
    }
    return names;
}

std::string toAscl(const TuningTable& t, const std::string& description, const std::string& source,
                   const std::vector<std::string>& noteNames) {
    const int n = t.stepsPerPeriod();
    // Live names MIDI 60 "C3", so the octave label of a key is floor(midi / 12) - 2.
    // Octaves here count periods starting at the tonic key.
    const int refOctave = floorDiv(t.tonicMidiNote, 12) - 2;
    const int minOctave = refOctave + floorDiv(-t.tonicMidiNote, n);
    const int minIndex = positiveMod(-t.tonicMidiNote, n);

    std::string s = toScl(t, description);
    s += "!\n";
    s += "! @ABL REFERENCE_PITCH " + std::to_string(refOctave) + " 0 " + formatHz(t.tonicHz) + "\n";
    s += "! @ABL NOTE_RANGE_BY_INDEX " + std::to_string(minOctave) + " " + std::to_string(minIndex) + "\n";
    if (static_cast<int>(noteNames.size()) == n) {
        s += "! @ABL NOTE_NAMES";
        for (const auto& name : noteNames)
            s += " \"" + oneLine(name) + "\"";
        s += "\n";
    }
    s += "! @ABL SOURCE " + oneLine(source) + "\n";
    return s;
}

} // namespace maqam
