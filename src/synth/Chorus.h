#pragma once

// A gentle stereo chorus for the synth voices: the dry sound plus two slowly moving delayed
// copies, one per side. A moving delay wobbles the copy's pitch a little (under 5 cents at
// most), but the delay always comes back to where it started, so over each LFO cycle the
// copies travel exactly as far as the dry note: the average pitch stays exactly in tune, and
// the dry note always dominates, so the copies never cancel it. The tonic drone is not sent through it.
// UI-free, no JUCE. prepare() allocates; process() is real-time safe.

#include <vector>

namespace maqam::synth {

class Chorus {
public:
    static constexpr double kRateHz = 0.45;      // LFO rate
    static constexpr double kBaseDelayMs = 8.0;  // centre of the moving delay
    static constexpr double kDepthMs = 1.0;      // delay swing either side

    void prepare(double sampleRate);
    void reset();

    /** 0 = off (output is exactly the input), 1 = full: 65% dry, 35% chorus. Changes are
        eased so turning the knob never clicks. */
    void setAmount(float amount);

    /** Reads mono from left and writes the stereo result to left and right. With right ==
        nullptr (a mono output) the two sides are mixed back into left. */
    void process(float* left, float* right, int numSamples);

private:
    float tap(double delaySamples) const;

    double sampleRate_ = 44100.0;
    std::vector<float> line_;
    int mask_ = 0;
    int write_ = 0;
    double phase_ = 0.0;  // 0..1
    float amount_ = 0.0f, target_ = 0.0f, smoothCoef_ = 0.0f;
};

} // namespace maqam::synth
