#pragma once

// Offline scale rendering for listening tests and audio unit tests. UI-free, no JUCE.
// This is a test tone generator, not the plugin's sound engine.

#include <string>
#include <vector>

namespace maqam {

struct RenderSettings {
    double sampleRate = 44100.0;
    double noteSeconds = 0.45;
    double gapSeconds = 0.05;
    float gain = 0.3f;
    int numHarmonics = 4;   // 1..4; use 1 (pure sine) for pitch-measuring tests
};

/** Renders each frequency in turn as a short tone (a few harmonics plus a pluck-like
    envelope). Returns mono samples. */
std::vector<float> renderTones(const std::vector<double>& frequenciesHz, const RenderSettings& settings);

/** Writes mono 16-bit PCM WAV. Returns false if the file could not be written. */
bool writeWav16(const std::string& path, const std::vector<float>& samples, double sampleRate);

/** Estimates the fundamental of a pure-ish tone by counting rising zero crossings
    between two sample positions. For tests. */
double estimateFrequencyHz(const std::vector<float>& samples, double sampleRate, size_t begin, size_t end);

} // namespace maqam
