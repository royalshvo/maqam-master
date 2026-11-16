#include "synth/Synth.h"

#include <algorithm>
#include <cmath>

namespace maqam::synth {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMinPluckHz = 20.0;
constexpr float kSilence = 3.0e-5f;
constexpr int kPluckChunk = 32;           // the pluck loop is retuned at most this often (samples)
constexpr double kVibratoDelay = 0.15;    // seconds of straight tone before vibrato starts
constexpr double kVibratoFadeIn = 0.35;   // seconds for vibrato to reach full depth
constexpr double kBendSmoothSeconds = 0.005;  // time constant easing the pitch wheel
// Synth unison: two extra saws this fraction above and below the note (about 4 cents). They sit
// symmetrically (in Hz) and start at mirrored phases, so together they only swell and fade the
// centre saw: the pitch stays exactly on the note.
constexpr double kUnisonSpread = 0.0023;
constexpr float kUnisonLevel = 0.16f;
constexpr float kSubLevel = 0.3f;         // Synth: sine one octave below
constexpr double kNeyVibratoPitch = 0.35;  // share of the vibrato depth the Ney puts into pitch
constexpr float kClipKnee = 0.7f;         // output is untouched below this level

// The oud-like body the Pluck sound plays through: fixed resonances (Hz, 1/Q, gain) mixed in
// beside the direct string sound.
struct BodyMode { double hz; float k; float gain; };
constexpr BodyMode kBodyModes[] = {{105.0, 0.22f, 0.55f}, {195.0, 0.2f, 0.5f}, {310.0, 0.25f, 0.4f}, {520.0, 0.3f, 0.3f}};

// Phase delay (in samples) at w radians/sample of the two-point loop filter (1-s) + s z^-1.
double twoPointDelay(double s, double w) {
    return std::atan2(s * std::sin(w), (1.0 - s) + s * std::cos(w)) / w;
}

// Phase delay of the first-order allpass (a + z^-1) / (1 + a z^-1).
double allpassDelay(double a, double w) {
    const double num = std::atan2(-std::sin(w), a + std::cos(w));
    const double den = std::atan2(-a * std::sin(w), 1.0 + a * std::cos(w));
    return -(num - den) / w;
}

// Levels pass unchanged up to the knee, then round off smoothly towards 1: single notes stay
// clean and big chords are held in without hard clipping.
float softClip(float x) {
    const float a = std::abs(x);
    if (a <= kClipKnee)
        return x;
    const float over = (a - kClipKnee) / (1.0f - kClipKnee);
    return std::copysign(kClipKnee + (1.0f - kClipKnee) * std::tanh(over), x);
}

float polyBlep(double t, double dt) {
    if (t < dt) {
        t /= dt;
        return static_cast<float>(t + t - t * t - 1.0);
    }
    if (t > 1.0 - dt) {
        t = (t - 1.0) / dt;
        return static_cast<float>(t * t + t + t + 1.0);
    }
    return 0.0f;
}

} // namespace

// ---------------------------------------------------------------- Voice

void Voice::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    line_.assign(static_cast<size_t>(sampleRate / kMinPluckHz) + 8, 0.0f);
    active_ = false;
}

float Voice::white() {
    noise_ ^= noise_ << 13;
    noise_ ^= noise_ >> 17;
    noise_ ^= noise_ << 5;
    return static_cast<float>(noise_) / 2147483648.0f - 1.0f;
}

void Voice::start(int key, double hz, float velocity, const SynthParams& params, uint64_t age) {
    params_ = params;
    key_ = key;
    age_ = age;
    velocity_ = 0.25f + 0.75f * std::clamp(velocity, 0.0f, 1.0f);
    active_ = true;
    released_ = false;
    sustained = false;
    baseHz_ = hz;
    glideCents_ = 0.0;
    glideStep_ = 0.0;
    vibPhase_ = 0.0;
    elapsed_ = 0.0;
    bendTarget_ = bendNow_ = 0.0;
    noise_ = 0x9E3779B9u ^ static_cast<uint32_t>(age * 2654435761u + static_cast<uint64_t>(key));
    const float bright = std::clamp(params.brightness, 0.0f, 1.0f);
    const double decay = std::max(0.05, static_cast<double>(params.decaySeconds));

    switch (params.sound) {
    case SoundType::Pluck: {
        // String damping. The loop filter acts once per period, so left alone it would damp a
        // high note's harmonics much faster than a low note's (dull trebles, buzzy basses) and
        // change with the sample rate. Scale it so the harmonics die at a similar rate across
        // the keyboard, as on a real instrument; brightness sets that rate.
        {
            const double s0 = 0.5 - 0.42 * bright;
            const double rate = sampleRate_ / 48000.0;
            const double loss = std::min(0.25, s0 * (1.0 - s0) * (350.0 / std::max(hz, kMinPluckHz)) * rate * rate);
            lpCoef_ = static_cast<float>(0.5 - std::sqrt(0.25 - loss));
        }
        tunePluck(hz);

        // Excitation: one period of filtered noise, with a pluck-position comb for a string-like
        // spectrum. Laid at the start of the line so it is read back over the first period.
        std::fill(line_.begin(), line_.end(), 0.0f);
        const int n = delay_;
        const float k = 0.15f + 0.85f * bright * (0.5f + 0.5f * velocity);
        float y = 0.0f;
        for (int i = 0; i < n; ++i) {
            y += k * (white() - y);
            line_[static_cast<size_t>(i)] = y;
        }
        const int m = std::max(1, static_cast<int>(std::lround(0.13 * n)));
        for (int i = n - 1; i >= m; --i)
            line_[static_cast<size_t>(i)] -= line_[static_cast<size_t>(i - m)];
        double mean = 0.0, power = 0.0;
        for (int i = 0; i < n; ++i)
            mean += line_[static_cast<size_t>(i)];
        mean /= n;
        for (int i = 0; i < n; ++i) {
            line_[static_cast<size_t>(i)] -= static_cast<float>(mean);
            power += static_cast<double>(line_[static_cast<size_t>(i)]) * line_[static_cast<size_t>(i)];
        }
        const float norm = power > 0.0 ? static_cast<float>(0.5 / std::sqrt(power / n)) : 0.0f;
        for (int i = 0; i < n; ++i)
            line_[static_cast<size_t>(i)] *= norm;
        write_ = n;
        apIn_ = apOut_ = lpPrev_ = 0.0f;
        break;
    }
    case SoundType::Synth:
        phase_ = 0.0;
        phaseUp_ = 0.37;    // mirrored start phases: see kUnisonSpread
        phaseDown_ = 0.63;
        subPhase_ = 0.0;
        amp_ = 0.0f;
        attacking_ = true;
        attackStep_ = static_cast<float>(1.0 / (0.004 * sampleRate_));
        decayCoef_ = static_cast<float>(std::exp(-1.0 / (decay / 3.0 * sampleRate_)));
        releaseCoef_ = static_cast<float>(std::exp(-1.0 / (0.12 * sampleRate_)));
        filterEnv_ = 1.0f;
        filterCoef_ = static_cast<float>(std::exp(-1.0 / (decay * 0.3 * sampleRate_)));
        ic1_ = ic2_ = 0.0f;
        break;
    case SoundType::Ney:
        phase_ = 0.0;
        flutter_ = 0.0f;
        amp_ = 0.0f;
        attackCoef_ = static_cast<float>(std::exp(-1.0 / (0.035 * sampleRate_)));  // breath swell
        releaseCoef_ = static_cast<float>(std::exp(-1.0 / ((0.04 + 0.012 * decay) * sampleRate_)));
        chiff_ = 0.5f + 0.5f * velocity;
        chiffCoef_ = static_cast<float>(std::exp(-1.0 / (0.03 * sampleRate_)));
        // The note is blown up to pitch: it starts a little flat and settles within ~0.1 s.
        scoopCents_ = -12.0;
        scoopCoef_ = std::exp(-1.0 / (0.02 * sampleRate_));
        std::fill(line_.begin(), line_.end(), 0.0f);  // the bore the breath noise rings in
        write_ = 0;
        boreLp_ = hissLp_ = 0.0f;
        wander_.fill(0.0f);
        break;
    }
}

double Voice::currentHz() const {
    return baseHz_ * std::exp2(glideCents_ / 1200.0);
}

void Voice::slideTo(double hz, double seconds) {
    if (!active_)
        return;
    const double from = currentHz();
    baseHz_ = hz;
    glideFrom(from, seconds);
}

void Voice::glideFrom(double fromHz, double seconds) {
    glideCents_ = 1200.0 * std::log2(fromHz / baseHz_);
    if (seconds <= 0.0 || std::abs(glideCents_) < 1e-9) {
        glideCents_ = 0.0;
        glideStep_ = 0.0;
    } else {
        glideStep_ = std::abs(glideCents_) / (seconds * sampleRate_);
    }
}

double Voice::advancePitch(int samples) {
    if (glideCents_ != 0.0) {
        const double step = glideStep_ * samples;
        glideCents_ = std::abs(glideCents_) <= step ? 0.0 : glideCents_ - std::copysign(step, glideCents_);
    }
    const double dt = samples / sampleRate_;
    elapsed_ += dt;
    double vib = 0.0;
    if (vibDepthCents_ > 0.0) {
        const double fade = std::clamp((elapsed_ - kVibratoDelay) / kVibratoFadeIn, 0.0, 1.0);
        vib = vibDepthCents_ * fade * std::sin(2.0 * kPi * vibPhase_);
    }
    lastVib_ = vib;
    // A ney's vibrato is mostly breath (loudness and tone, see renderNey), with only a little
    // pitch: a wide pitch vibrato is what a violin does.
    if (params_.sound == SoundType::Ney)
        vib *= kNeyVibratoPitch;
    vibPhase_ += vibRateHz_ * dt;
    vibPhase_ -= std::floor(vibPhase_);
    if (bendNow_ != bendTarget_) {
        bendNow_ = bendTarget_ + (bendNow_ - bendTarget_) * std::exp(-dt / kBendSmoothSeconds);
        if (std::abs(bendNow_ - bendTarget_) < 1e-4)
            bendNow_ = bendTarget_;
    }
    return glideCents_ + vib + bendNow_;
}

void Voice::tunePluck(double hz) {
    pluckHz_ = hz;
    const double f = std::clamp(hz, kMinPluckHz, sampleRate_ * 0.2);
    // Total loop delay must be exactly one period. The loop filter and the allpass both delay
    // by a frequency-dependent amount, so solve for them at the fundamental.
    const double period = sampleRate_ / f;
    const double w = 2.0 * kPi * f / sampleRate_;
    const double rest = period - twoPointDelay(lpCoef_, w);
    const int maxDelay = static_cast<int>(line_.size()) - 2;
    delay_ = std::clamp(static_cast<int>(std::floor(rest - 0.5)), 1, maxDelay);
    const double frac = rest - delay_;  // ~0.5..1.5 samples, where a first-order allpass is well behaved
    double lo = -0.95, hi = 0.95;       // allpass delay falls as the coefficient rises
    for (int i = 0; i < 32; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (allpassDelay(mid, w) > frac)
            lo = mid;
        else
            hi = mid;
    }
    apCoef_ = static_cast<float>(0.5 * (lo + hi));
    const double decay = std::max(0.05, static_cast<double>(params_.decaySeconds));
    loopGain_ = static_cast<float>(std::min(0.99999, std::pow(10.0, -3.0 / (decay * f))));
    releaseGain_ = static_cast<float>(std::pow(10.0, -3.0 / (0.25 * f)));
}

void Voice::release() {
    released_ = true;
}

void Voice::render(float* out, int numSamples) {
    if (!active_)
        return;
    switch (params_.sound) {
    case SoundType::Pluck: renderPluck(out, numSamples); break;
    case SoundType::Synth: renderSynth(out, numSamples); break;
    case SoundType::Ney: renderNey(out, numSamples); break;
    }
}

void Voice::renderPluck(float* out, int numSamples) {
    const int len = static_cast<int>(line_.size());
    const float outGain = 1.1f * velocity_;
    for (int done = 0; done < numSamples;) {
        float peak = 0.0f;
        const int chunk = std::min(kPluckChunk, numSamples - done);
        // Glide and vibrato move the string's tuning chunk by chunk.
        const double hz = baseHz_ * std::exp2(advancePitch(chunk) / 1200.0);
        if (std::abs(1200.0 * std::log2(hz / pluckHz_)) > 0.01)
            tunePluck(hz);
        const float gain = released_ ? releaseGain_ : loopGain_;
        for (int i = done; i < done + chunk; ++i) {
            int r = write_ - delay_;
            if (r < 0)
                r += len;
            const float x = line_[static_cast<size_t>(r)];
            const float ap = apCoef_ * x + apIn_ - apCoef_ * apOut_;
            apIn_ = x;
            apOut_ = ap;
            const float y = ((1.0f - lpCoef_) * ap + lpCoef_ * lpPrev_) * gain;
            lpPrev_ = ap;
            line_[static_cast<size_t>(write_)] = y;
            if (++write_ == len)
                write_ = 0;
            out[i] += y * outGain;
            peak = std::max(peak, std::abs(y));
        }
        done += chunk;
        if (peak < kSilence) {  // the string has died away: free the voice
            active_ = false;
            return;
        }
    }
}

void Voice::renderSynth(float* out, int numSamples) {
    const float bright = std::clamp(params_.brightness, 0.0f, 1.0f);
    const float outGain = 0.62f * velocity_;  // matches the other sounds' loudness
    auto sawAt = [](double phase, double dt) { return static_cast<float>(2.0 * phase - 1.0) - polyBlep(phase, dt); };
    auto wrap = [](double& phase) {
        if (phase >= 1.0)
            phase -= 1.0;
    };
    const double nyquistGuard = 0.45 * sampleRate_;
    const float k = 1.2f;  // 1/Q: a gentle resonance
    for (int i = 0; i < numSamples; ++i) {
        const double hz = baseHz_ * std::exp2(advancePitch(1) / 1200.0);
        const double dt = hz / sampleRate_;
        const float saw = sawAt(phase_, dt);
        const double dtUp = dt * (1.0 + kUnisonSpread), dtDown = dt * (1.0 - kUnisonSpread);
        const float sides = sawAt(phaseUp_, dtUp) + sawAt(phaseDown_, dtDown);
        const float sub = static_cast<float>(std::sin(2.0 * kPi * subPhase_));
        double half = phase_ + 0.5;
        if (half >= 1.0)
            half -= 1.0;
        const float square = (phase_ < 0.5 ? 1.0f : -1.0f) + polyBlep(phase_, dt) - polyBlep(half, dt);
        // The square is subtracted: a rising saw and this square have opposite fundamentals, so
        // this way they reinforce each other instead of thinning the note out.
        const float osc = 0.5f * saw + kUnisonLevel * sides - 0.3f * square - kSubLevel * sub;
        phase_ += dt;
        wrap(phase_);
        phaseUp_ += dtUp;
        wrap(phaseUp_);
        phaseDown_ += dtDown;
        wrap(phaseDown_);
        subPhase_ += 0.5 * dt;
        wrap(subPhase_);

        // The filter opens on the attack and settles a few harmonics above the note, so the
        // held sound keeps its body.
        const double cutoff =
            std::min(nyquistGuard, hz * (2.5 + 5.0 * bright + (2.0 + 30.0 * bright) * filterEnv_));
        filterEnv_ *= filterCoef_;
        const float g = static_cast<float>(std::tan(kPi * cutoff / sampleRate_));
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float v3 = osc - ic2_;
        const float v1 = a1 * ic1_ + a2 * v3;
        const float v2 = ic2_ + a2 * ic1_ + a3 * v3;
        ic1_ = 2.0f * v1 - ic1_;
        ic2_ = 2.0f * v2 - ic2_;

        if (attacking_) {
            amp_ += attackStep_;
            if (amp_ >= 1.0f) {
                amp_ = 1.0f;
                attacking_ = false;
            }
        } else if (!released_) {
            amp_ = 0.7f + (amp_ - 0.7f) * decayCoef_;
        } else {
            amp_ *= releaseCoef_;
        }
        out[i] += v2 * amp_ * outGain;
    }
    if (released_ && amp_ < 1.0e-4f)
        active_ = false;
}

void Voice::renderNey(float* out, int numSamples) {
    // A reed flute, built from three things that move together with the player's breath:
    //  - the tone: harmonics that each wander a little, and come in from the bottom up as the
    //    breath builds (a soft breath is nearly a sine, a full one is rich);
    //  - the breath in the tube: noise ringing in a delay line one period long, so the air
    //    itself carries the note's pitch, pulsing with the tone;
    //  - the air at the lips: a quiet hiss.
    // Vibrato also moves the loudness and brightness, as breath vibrato does.
    // A flute is mostly fundamental: strong upper harmonics are what make a bowed string. So
    // the harmonics fall away quickly and the breath does the rest.
    static constexpr float kPartials[kNeyPartials] = {1.0f, 0.3f, 0.12f, 0.05f, 0.022f, 0.01f};
    const float bright = std::clamp(params_.brightness, 0.0f, 1.0f);
    const float tilt = 0.7f + 0.6f * bright;  // extra roll-off (or lift) per harmonic
    const float outGain = 0.6f * velocity_;
    const double nyquistGuard = 0.45 * sampleRate_;
    const int len = static_cast<int>(line_.size());
    const float flutterCoef = static_cast<float>(1.0 - std::exp(-2.0 * kPi * 6.0 / sampleRate_));
    const float wanderCoef = static_cast<float>(1.0 - std::exp(-2.0 * kPi * 3.0 / sampleRate_));
    // Scales white noise so each harmonic's level wanders by about 5%.
    const float wanderIn = 0.05f / (0.577f * std::sqrt(0.5f * wanderCoef));
    const float hissCoef = static_cast<float>(1.0 - std::exp(-2.0 * kPi * 2500.0 / sampleRate_));
    constexpr float kBoreDamp = 0.3f;        // one-pole low-pass in the bore loop
    constexpr float kBoreFeedback = 0.9f;
    const float boreGain = 0.11f * (0.7f + 0.9f * bright);
    const float hissGain = 0.06f * (0.5f + bright);

    for (int i = 0; i < numSamples; ++i) {
        const double hz = baseHz_ * std::exp2((advancePitch(1) + scoopCents_) / 1200.0);
        scoopCents_ *= scoopCoef_;
        const double theta = 2.0 * kPi * phase_;
        phase_ += hz / sampleRate_;
        if (phase_ >= 1.0)
            phase_ -= 1.0;

        // Breath pressure: swell, flutter, and the vibrato's push.
        const float w = white();
        flutter_ += flutterCoef * (w - flutter_);
        amp_ = released_ ? amp_ * releaseCoef_ : 1.0f + (amp_ - 1.0f) * attackCoef_;
        const float vib = static_cast<float>(lastVib_ / kMaxVibratoCents);  // -1..1
        const float pressure = std::max(0.0f, amp_ * (1.0f + 1.5f * flutter_) * (1.0f + 0.3f * vib));

        // Tone.
        const float grow = std::sqrt(std::min(1.0f, pressure));  // upper harmonics need more breath
        float tone = 0.0f, level = 1.0f;
        for (int n = 0; n < kNeyPartials; ++n) {
            if ((n + 1) * hz > nyquistGuard)
                break;
            float& wander = wander_[static_cast<size_t>(n)];
            wander += wanderCoef * (white() * wanderIn - wander);
            tone += kPartials[n] * level * (1.0f + wander) * static_cast<float>(std::sin((n + 1) * theta));
            level *= tilt * grow;
        }
        // The attack briefly cracks into the octave before the note settles.
        tone += 0.5f * chiff_ * static_cast<float>(std::sin(2.0 * theta));

        // Breath in the tube: noise (pulsing with the tone, plus the attack's chiff) rings in a
        // delay one period long.
        const float breathIn = w * (1.0f + 0.5f * static_cast<float>(std::sin(theta))) + white() * chiff_ * 2.0f;
        chiff_ *= chiffCoef_;
        const double delay = std::clamp(sampleRate_ / hz - kBoreDamp / (1.0 - kBoreDamp), 2.0, static_cast<double>(len - 3));
        double readPos = static_cast<double>(write_) - delay;
        if (readPos < 0.0)
            readPos += len;
        const int r0 = static_cast<int>(readPos);
        const int r1 = r0 + 1 == len ? 0 : r0 + 1;
        const float frac = static_cast<float>(readPos - r0);
        const float delayed = line_[static_cast<size_t>(r0)] + frac * (line_[static_cast<size_t>(r1)] - line_[static_cast<size_t>(r0)]);
        boreLp_ += (1.0f - kBoreDamp) * (delayed - boreLp_);
        const float bore = breathIn + kBoreFeedback * boreLp_;
        line_[static_cast<size_t>(write_)] = bore;
        if (++write_ == len)
            write_ = 0;

        // Air at the lips.
        hissLp_ += hissCoef * (w - hissLp_);
        const float hiss = w - hissLp_;

        // The tone needs pressure to speak (so the breath is heard first); the air follows it
        // directly.
        out[i] += (0.62f * tone * std::min(1.0f, pressure) + boreGain * bore + hissGain * hiss) * pressure * outGain;
    }
    if (released_ && amp_ < 1.0e-4f)
        active_ = false;
}

// ---------------------------------------------------------------- Synth

Synth::Synth() : defaultTuning_(twelveTetKeyTuning()), tuning_(&defaultTuning_) {}

void Synth::prepare(double sampleRate) {
    sampleRate_ = sampleRate;
    for (auto& v : voices_)
        v.prepare(sampleRate);
    heldCount_ = 0;
    for (size_t m = 0; m < body_.size(); ++m) {
        const double hz = std::min(kBodyModes[m].hz, 0.4 * sampleRate);
        body_[m] = BodyFilter{};
        body_[m].g = static_cast<float>(std::tan(kPi * hz / sampleRate));
    }
}

void Synth::setParams(const SynthParams& params) {
    if (params.mode != params_.mode) {
        allNotesOff();
        heldCount_ = 0;
    }
    params_ = params;
}

void Synth::setModWheel(float value) {
    modWheel_ = std::clamp(value, 0.0f, 1.0f);
}

void Synth::setPitchBend(float value) {
    pitchBend_ = std::clamp(value, -1.0f, 1.0f);
}

double Synth::bendCentsFor(const Voice& v) const {
    const double amount = pitchBend_;
    if (amount == 0.0)
        return 0.0;
    const double fixed = amount * params_.bendRangeCents;
    const int sk = soundingKey(v.key(), v.shift());
    if (!params_.bendToMaqam || !tuning_->hasMaqam || sk < 0)
        return fixed;
    // Full travel reaches the nearest maqam degree above (or below) the note's pitch.
    const int dir = amount > 0.0 ? 1 : -1;
    const double from = hzOf(sk);
    for (int k = sk + dir; k >= 0 && k < kNumKeys; k += dir) {
        if (tuning_->degree[static_cast<size_t>(k)] == 0)
            continue;
        const double cents = 1200.0 * std::log2(hzOf(k) / from);
        if (cents * dir > 1.0)
            return std::abs(amount) * cents;
    }
    return fixed;  // no degree that way within the keyboard
}

int Synth::soundingKey(int incoming, int shift) const {
    if (incoming < 0 || incoming >= kNumKeys)
        return -1;
    const int key = incoming + shift;
    if (key < 0 || key >= kNumKeys)
        return -1;
    const auto k = static_cast<size_t>(key);
    if (params_.limit == NoteLimit::Off || !tuning_->hasMaqam || tuning_->degree[k] != 0)
        return key;
    return params_.limit == NoteLimit::Mute ? -1 : tuning_->snapKey[k];
}

void Synth::setTuning(const KeyTuning& tuning) {
    tuning_ = &tuning;
    for (auto& v : voices_) {
        if (!v.active())
            continue;
        const int sk = soundingKey(v.key(), v.shift());
        if (sk < 0)
            v.release();
        else
            v.retune(hzOf(sk));
    }
}

Voice* Synth::findVoiceFor(int key) {
    for (auto& v : voices_)
        if (v.active() && v.key() == key)
            return &v;  // restrike the same string
    for (auto& v : voices_)
        if (!v.active())
            return &v;
    Voice* oldest = nullptr;
    for (auto& v : voices_)
        if (v.released() && (oldest == nullptr || v.age() < oldest->age()))
            oldest = &v;
    if (oldest != nullptr)
        return oldest;
    for (auto& v : voices_)
        if (oldest == nullptr || v.age() < oldest->age())
            oldest = &v;
    return oldest;
}

void Synth::noteOn(int key, float velocity) {
    if (key < 0 || key >= kNumKeys)
        return;
    if (velocity <= 0.0f) {
        noteOff(key);
        return;
    }
    if (mono())
        monoNoteOn(key, velocity);
    else
        polyNoteOn(key, velocity);
}

void Synth::polyNoteOn(int key, float velocity) {
    const int sk = soundingKey(key);
    if (sk < 0)
        return;
    Voice* v = findVoiceFor(key);
    v->start(key, hzOf(sk), velocity, params_, ++noteCounter_);
    v->setShift(keyShift_);
    v->setBend(bendCentsFor(*v), true);  // a note played with the wheel held starts bent
}

void Synth::removeHeld(int key) {
    int n = 0;
    for (int i = 0; i < heldCount_; ++i)
        if (held_[static_cast<size_t>(i)] != key)
            held_[static_cast<size_t>(n++)] = held_[static_cast<size_t>(i)];
    heldCount_ = n;
}

void Synth::monoNoteOn(int key, float velocity) {
    const int sk = soundingKey(key);
    if (sk < 0)
        return;
    const bool overlapping = heldCount_ > 0;
    removeHeld(key);
    held_[static_cast<size_t>(heldCount_++)] = key;

    Voice& v = voices_[0];
    const double glide = params_.glideSeconds;
    if (params_.mode == VoiceMode::Legato && overlapping && v.active() && !v.released()) {
        v.setKey(key);
        v.setShift(keyShift_);
        v.sustained = false;
        v.slideTo(hzOf(sk), glide);  // no re-attack
        return;
    }
    const double previousHz = v.active() ? v.currentHz() : 0.0;
    v.start(key, hzOf(sk), velocity, params_, ++noteCounter_);
    v.setShift(keyShift_);
    v.setBend(bendCentsFor(v), true);
    if (params_.mode == VoiceMode::Mono && previousHz > 0.0 && glide > 0.0)
        v.glideFrom(previousHz, glide);
}

void Synth::monoNoteOff(int key) {
    removeHeld(key);
    Voice& v = voices_[0];
    if (!v.active() || v.key() != key || v.released())
        return;  // the voice is already on another key
    // Go back to the most recent key still held, without re-attacking.
    while (heldCount_ > 0) {
        const int k = held_[static_cast<size_t>(heldCount_ - 1)];
        const int sk = soundingKey(k);
        if (sk >= 0) {
            v.setKey(k);
            v.setShift(keyShift_);
            v.slideTo(hzOf(sk), params_.glideSeconds);
            return;
        }
        --heldCount_;
    }
    if (sustain_)
        v.sustained = true;
    else
        v.release();
}

void Synth::noteOff(int key) {
    if (key < 0 || key >= kNumKeys)
        return;
    if (mono()) {
        monoNoteOff(key);
        return;
    }
    for (auto& v : voices_) {
        if (!v.active() || v.key() != key || v.released())
            continue;
        if (sustain_)
            v.sustained = true;
        else
            v.release();
    }
}

void Synth::setSustain(bool down) {
    sustain_ = down;
    if (down)
        return;
    for (auto& v : voices_) {
        if (v.active() && v.sustained) {
            v.sustained = false;
            v.release();
        }
    }
}

void Synth::allNotesOff() {
    sustain_ = false;
    heldCount_ = 0;
    for (auto& v : voices_)
        if (v.active())
            v.release();
}

void Synth::killAll() {
    sustain_ = false;
    heldCount_ = 0;
    for (auto& v : voices_)
        v.kill();
}

void Synth::render(float* out, int numSamples) {
    std::fill(out, out + numSamples, 0.0f);
    const double depth = std::clamp(static_cast<double>(params_.vibrato) + modWheel_, 0.0, 1.0) * kMaxVibratoCents;
    for (auto& v : voices_) {
        if (!v.active())
            continue;
        v.setVibrato(depth, params_.vibratoRateHz);
        v.setBend(bendCentsFor(v), false);
        v.render(out, numSamples);
    }
    bool bodyRinging = activeVoiceCount() > 0;
    for (auto& f : body_)
        bodyRinging = bodyRinging || std::abs(f.ic1) > 1.0e-6f || std::abs(f.ic2) > 1.0e-6f;
    if (!bodyRinging) {
        for (auto& f : body_)
            f.ic1 = f.ic2 = 0.0f;  // fully died away: back to true silence
    } else if (params_.sound == SoundType::Pluck) {
        // The instrument's body: band-pass resonances, all fed by the strings, beside the
        // direct sound.
        for (int i = 0; i < numSamples; ++i) {
            const float dry = out[i];
            float sum = dry;
            for (size_t m = 0; m < body_.size(); ++m) {
                BodyFilter& f = body_[m];
                const float k = kBodyModes[m].k;
                const float a1 = 1.0f / (1.0f + f.g * (f.g + k));
                const float a2 = f.g * a1;
                const float a3 = f.g * a2;
                const float v3 = dry - f.ic2;
                const float v1 = a1 * f.ic1 + a2 * v3;
                const float v2 = f.ic2 + a2 * f.ic1 + a3 * v3;
                f.ic1 = 2.0f * v1 - f.ic1;
                f.ic2 = 2.0f * v2 - f.ic2;
                sum += kBodyModes[m].gain * k * v1;  // k scales the band-pass peak to unity
            }
            out[i] = sum;
        }
    }
    const float gain = params_.gain;
    for (int i = 0; i < numSamples; ++i)
        out[i] = softClip(out[i]) * gain;
}

int Synth::activeVoiceCount() const {
    int n = 0;
    for (const auto& v : voices_)
        n += v.active() ? 1 : 0;
    return n;
}

double Synth::bendCents(int key) const {
    for (const auto& v : voices_)
        if (v.active() && v.key() == key)
            return v.bendCents();
    return 0.0;
}

double Synth::playingHz(int key) const {
    for (const auto& v : voices_)
        if (v.active() && v.key() == key)
            return v.targetHz();
    return 0.0;
}

double Synth::currentHz(int key) const {
    for (const auto& v : voices_)
        if (v.active() && v.key() == key)
            return v.currentHz();
    return 0.0;
}

} // namespace maqam::synth
