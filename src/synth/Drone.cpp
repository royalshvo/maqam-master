#include "synth/Drone.h"

#include <algorithm>
#include <cmath>

namespace maqam::synth {

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kSwellHz = 0.13;
constexpr float kFadeSeconds = 0.25f;  // time constant of the fade in/out
}

void Drone::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    fadeCoef_ = 1.0f - std::exp(-1.0f / (kFadeSeconds * static_cast<float>(sampleRate)));
}

void Drone::setParams(const DroneParams& params) {
    params_ = params;
    params_.octave = std::clamp(params_.octave, -2, 0);
}

void Drone::setTonicHz(double hz, double glideSeconds) {
    if (hz <= 0.0) {  // no maqam: keep the last pitch while fading out
        hasTonic_ = false;
        return;
    }
    if (!started_ || level_ < 1e-5f || glideSeconds <= 0.0) {
        glideCents_ = 0.0;  // first tonic, or silent: just jump
        glideStep_ = 0.0;
    } else {
        const double fromHz = tonicHz_ * std::exp2(glideCents_ / 1200.0);
        glideCents_ = 1200.0 * std::log2(fromHz / hz);
        glideStep_ = glideCents_ / (glideSeconds * sampleRate_);
    }
    tonicHz_ = hz;
    hasTonic_ = true;
    started_ = true;
}

double Drone::targetHz() const {
    return wantOn() ? tonicHz_ * std::exp2(params_.octave) : 0.0;
}

void Drone::render(float* out, int numSamples) {
    if (!sounding() || tonicHz_ <= 0.0)
        return;
    const float goal = wantOn() ? 1.0f : 0.0f;
    const double baseHz = tonicHz_ * std::exp2(params_.octave);
    const double swellStep = kSwellHz / sampleRate_;
    const float gain = params_.gain * 0.42f;  // partial sum peaks near 2.4

    for (int i = 0; i < numSamples; ++i) {
        if (glideCents_ != 0.0) {
            glideCents_ -= glideStep_;
            if ((glideStep_ > 0.0 && glideCents_ < 0.0) || (glideStep_ < 0.0 && glideCents_ > 0.0))
                glideCents_ = 0.0;
        }
        level_ += (goal - level_) * fadeCoef_;
        const double hz = baseHz * (glideCents_ != 0.0 ? std::exp2(glideCents_ / 1200.0) : 1.0);
        phase_ += hz / sampleRate_;
        phase_ -= std::floor(phase_);
        swellPhase_ += swellStep;
        swellPhase_ -= std::floor(swellPhase_);

        const double p = kTwoPi * phase_;
        const double octave = 0.8 + 0.2 * std::sin(kTwoPi * swellPhase_);
        const double tone = std::sin(p) + octave * std::sin(2.0 * p) + 0.22 * std::sin(3.0 * p) +
                            0.12 * octave * std::sin(4.0 * p);
        out[i] += static_cast<float>(tone) * gain * level_;
    }
    if (goal == 0.0f && level_ < 1e-5f)
        level_ = 0.0f;
}

} // namespace maqam::synth
