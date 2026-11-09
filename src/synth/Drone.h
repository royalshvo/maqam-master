#pragma once

// A sustained drone on the maqam's tonic: the tonic plus its octave (and a little of the
// fifth above the octave, from the 3rd harmonic), all phase-locked to one exact frequency, so
// it is always in tune with the keys. It follows maqam and tonic changes with a short slide
// (linear in cents) and fades in and out instead of clicking. A slow swell on the octave
// partial keeps it alive without moving the pitch.
// UI-free, no JUCE; real-time safe: no allocation or locks.

namespace maqam::synth {

struct DroneParams {
    bool on = false;
    float gain = 0.25f;   // linear output gain
    int octave = -1;      // register relative to the tonic key: -2, -1 or 0 octaves
};

class Drone {
public:
    void prepare(double sampleRate);

    void setParams(const DroneParams& params);

    /** The tonic frequency (KeyTuning::tonicHz); 0 means no maqam, and the drone fades out.
        A change after the first one slides there over glideSeconds. */
    void setTonicHz(double hz, double glideSeconds = 0.3);

    /** Adds the drone into out. */
    void render(float* out, int numSamples);

    /** The fundamental the drone is aiming for (tonic moved by the octave setting), or 0. */
    double targetHz() const;
    /** True while it is sounding or fading out. */
    bool sounding() const { return level_ > 1e-5f || wantOn(); }

private:
    bool wantOn() const { return params_.on && hasTonic_; }

    double sampleRate_ = 44100.0;
    DroneParams params_;
    double tonicHz_ = 0.0;     // target tonic (kept while fading out after the maqam goes)
    bool hasTonic_ = false;
    double glideCents_ = 0.0;  // offset from the target, running linearly to 0
    double glideStep_ = 0.0;   // cents per sample
    double phase_ = 0.0;       // 0..1, fundamental
    double swellPhase_ = 0.0;  // 0..1, slow octave swell
    float level_ = 0.0f;       // fade in/out
    float fadeCoef_ = 0.0f;
    bool started_ = false;     // a tonic has been set: later changes slide
};

} // namespace maqam::synth
