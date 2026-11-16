#include "synth/Chorus.h"

#include <algorithm>
#include <cmath>

namespace maqam::synth {

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr float kSmoothSeconds = 0.02f;
}

void Chorus::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    const int needed = static_cast<int>(std::ceil((kBaseDelayMs + kDepthMs) * 0.001 * sampleRate)) + 4;
    int size = 1;
    while (size < needed)
        size <<= 1;
    line_.assign(static_cast<size_t>(size), 0.0f);
    mask_ = size - 1;
    smoothCoef_ = 1.0f - std::exp(-1.0f / (kSmoothSeconds * static_cast<float>(sampleRate)));
    reset();
}

void Chorus::reset() {
    std::fill(line_.begin(), line_.end(), 0.0f);
    write_ = 0;
    phase_ = 0.0;
    amount_ = target_;
}

void Chorus::setAmount(float amount) {
    target_ = std::clamp(amount, 0.0f, 1.0f);
}

float Chorus::tap(double delaySamples) const {
    const double pos = static_cast<double>(write_) - delaySamples;
    const double fl = std::floor(pos);
    const float frac = static_cast<float>(pos - fl);
    const int i0 = static_cast<int>(fl) & mask_;
    const int i1 = (i0 + 1) & mask_;
    return line_[static_cast<size_t>(i0)] + frac * (line_[static_cast<size_t>(i1)] - line_[static_cast<size_t>(i0)]);
}

void Chorus::process(float* left, float* right, int numSamples) {
    if (line_.empty())
        return;
    if (amount_ == 0.0f && target_ == 0.0f) {
        // Off: keep the line filled so turning it on later doesn't start from silence.
        for (int i = 0; i < numSamples; ++i) {
            line_[static_cast<size_t>(write_)] = left[i];
            write_ = (write_ + 1) & mask_;
            if (right != nullptr)
                right[i] = left[i];
        }
        phase_ = std::fmod(phase_ + kRateHz * numSamples / sampleRate_, 1.0);
        return;
    }
    const double base = kBaseDelayMs * 0.001 * sampleRate_;
    const double depth = kDepthMs * 0.001 * sampleRate_;
    const double step = kRateHz / sampleRate_;
    for (int i = 0; i < numSamples; ++i) {
        amount_ += (target_ - amount_) * smoothCoef_;
        if (std::abs(amount_ - target_) < 1e-6f)
            amount_ = target_;
        const float dry = left[i];
        line_[static_cast<size_t>(write_)] = dry;
        // Left and right LFOs a quarter cycle apart for width.
        const float wetL = tap(base + depth * std::sin(kTwoPi * phase_));
        const float wetR = tap(base + depth * std::cos(kTwoPi * phase_));
        write_ = (write_ + 1) & mask_;
        phase_ += step;
        if (phase_ >= 1.0)
            phase_ -= 1.0;

        // Full = 65% dry, 35% chorus: the copies never cancel the note.
        const float dryGain = 1.0f - 0.35f * amount_;
        const float wetGain = 0.35f * amount_;
        const float l = dryGain * dry + wetGain * wetL;
        const float r = dryGain * dry + wetGain * wetR;
        if (right != nullptr) {
            left[i] = l;
            right[i] = r;
        } else {
            left[i] = 0.5f * (l + r);
        }
    }
}

} // namespace maqam::synth
