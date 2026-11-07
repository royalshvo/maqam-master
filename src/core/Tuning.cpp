#include "core/Tuning.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace maqam {

Tonic Tonic::fromCentsAboveC(double centsAboveC, int octave) {
    const double absolute = (octave + 1) * 1200.0 + centsAboveC;   // MIDI 60 = C4
    const int midi = static_cast<int>(std::lround(absolute / 100.0));
    return Tonic{midi, absolute - midi * 100.0};
}

std::optional<ResolvedScale> resolveScale(const Maqam& maqam, Direction direction,
                                          const std::string& variantId,
                                          const TuningPreset* preset) {
    ResolvedScale out;
    out.cents = maqam.ascending;
    out.ajnas = maqam.ajnas;
    out.octaveRepeating = maqam.octaveRepeating;

    const ScaleChange* change = nullptr;
    if (direction == Direction::Descending) {
        if (!variantId.empty())
            throw std::invalid_argument("variants apply to the ascending form only");
        if (!maqam.descending)
            return std::nullopt;
        change = &*maqam.descending;
    } else if (!variantId.empty()) {
        change = maqam.findVariant(variantId);
        if (change == nullptr)
            throw std::invalid_argument("maqam '" + maqam.id + "' has no variant '" + variantId + "'");
    }

    if (change != nullptr) {
        for (const auto& [degree, cents] : change->changes)
            out.cents[static_cast<size_t>(degree - 1)] = cents;
        if (change->ajnas)
            out.ajnas = *change->ajnas;
    }

    if (preset != nullptr) {
        auto it = preset->maqamOffsets.find(maqam.id);
        if (it != preset->maqamOffsets.end())
            for (const auto& [degree, offset] : it->second)
                out.cents[static_cast<size_t>(degree - 1)] += offset;
    }
    return out;
}

double centsToRatio(double cents) {
    return std::pow(2.0, cents / 1200.0);
}

double midiNoteToHz(double midiNote, double a4Hz) {
    return a4Hz * std::pow(2.0, (midiNote - 69.0) / 12.0);
}

double frequencyHz(const Tonic& tonic, double centsFromTonic, double a4Hz) {
    return midiNoteToHz((tonic.absoluteCents() + centsFromTonic) / 100.0, a4Hz);
}

double degreeFrequencyHz(const ResolvedScale& scale, const Tonic& tonic, int degree,
                         int octaveShift, double a4Hz) {
    if (degree < 1 || degree > scale.numDegrees())
        throw std::out_of_range("degree out of range");
    return frequencyHz(tonic, scale.cents[static_cast<size_t>(degree - 1)] + 1200.0 * octaveShift,
                       a4Hz);
}

bool isOffTwelveTetGrid(const Tonic& tonic, double centsFromTonic, double toleranceCents) {
    const double absolute = tonic.absoluteCents() + centsFromTonic;
    const double distance = std::abs(absolute - 100.0 * std::round(absolute / 100.0));
    return distance > toleranceCents;
}

const char* toString(NoteRole role) {
    switch (role) {
        case NoteRole::Tonic:          return "tonic";
        case NoteRole::Ghammaz:        return "ghammaz";
        case NoteRole::Characteristic: return "characteristic";
        case NoteRole::QuarterTone:    return "quarter_tone";
        case NoteRole::Passing:        return "passing";
    }
    return "passing";
}

const std::vector<NoteRole>& allNoteRoles() {
    static const std::vector<NoteRole> roles{NoteRole::Tonic, NoteRole::Ghammaz,
                                             NoteRole::Characteristic, NoteRole::QuarterTone,
                                             NoteRole::Passing};
    return roles;
}

std::vector<NoteRole> rolesForDegree(const Maqam& maqam, const ResolvedScale& scale,
                                     const Tonic& tonic, int degree) {
    if (degree < 1 || degree > scale.numDegrees())
        throw std::out_of_range("degree out of range");

    std::vector<NoteRole> roles;
    const double cents = scale.cents[static_cast<size_t>(degree - 1)];
    const bool isOctaveOfTonic = scale.octaveRepeating && std::abs(cents - 1200.0) < 0.5;

    if (degree == 1 || isOctaveOfTonic)
        roles.push_back(NoteRole::Tonic);
    if (degree == maqam.ghammazDegree)
        roles.push_back(NoteRole::Ghammaz);
    const auto& ch = maqam.characteristicDegrees;
    if (std::find(ch.begin(), ch.end(), degree) != ch.end())
        roles.push_back(NoteRole::Characteristic);
    if (isOffTwelveTetGrid(tonic, cents))
        roles.push_back(NoteRole::QuarterTone);
    if (roles.empty())
        roles.push_back(NoteRole::Passing);
    return roles;
}

NoteRole primaryRole(const Maqam& maqam, const ResolvedScale& scale, const Tonic& tonic, int degree) {
    return rolesForDegree(maqam, scale, tonic, degree).front();
}

} // namespace maqam
