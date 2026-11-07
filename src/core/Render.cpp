#include "core/Render.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>

namespace maqam {

namespace {
constexpr double kTwoPi = 6.283185307179586;

void writeLE(std::ofstream& out, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i)
        out.put(static_cast<char>((value >> (8 * i)) & 0xFF));
}
} // namespace

std::vector<float> renderTones(const std::vector<double>& frequenciesHz, const RenderSettings& s) {
    const auto noteLen = static_cast<size_t>(s.noteSeconds * s.sampleRate);
    const auto gapLen = static_cast<size_t>(s.gapSeconds * s.sampleRate);
    const auto attack = static_cast<size_t>(0.005 * s.sampleRate);
    std::vector<float> out;
    out.reserve(frequenciesHz.size() * (noteLen + gapLen));

    // Harmonic amplitudes: a soft, slightly bright tone that makes tuning easy to hear.
    const double harmonics[] = {1.0, 0.35, 0.15, 0.07};
    const auto numHarmonics = static_cast<size_t>(std::clamp(s.numHarmonics, 1, 4));

    for (double f : frequenciesHz) {
        for (size_t i = 0; i < noteLen; ++i) {
            const double t = static_cast<double>(i) / s.sampleRate;
            double v = 0.0;
            for (size_t h = 0; h < numHarmonics; ++h) {
                const double fh = f * static_cast<double>(h + 1);
                if (fh < s.sampleRate * 0.45)
                    v += harmonics[h] * std::sin(kTwoPi * fh * t);
            }
            double env = std::exp(-3.0 * t);
            if (i < attack)
                env *= static_cast<double>(i) / static_cast<double>(attack);
            const size_t remaining = noteLen - i;
            if (remaining < attack)
                env *= static_cast<double>(remaining) / static_cast<double>(attack);
            out.push_back(static_cast<float>(v * env * s.gain / 1.57));
        }
        out.insert(out.end(), gapLen, 0.0f);
    }
    return out;
}

bool writeWav16(const std::string& path, const std::vector<float>& samples, double sampleRate) {
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;
    const auto rate = static_cast<uint32_t>(sampleRate);
    const auto dataBytes = static_cast<uint32_t>(samples.size() * 2);
    out.write("RIFF", 4);
    writeLE(out, 36 + dataBytes, 4);
    out.write("WAVEfmt ", 8);
    writeLE(out, 16, 4);        // fmt chunk size
    writeLE(out, 1, 2);         // PCM
    writeLE(out, 1, 2);         // mono
    writeLE(out, rate, 4);
    writeLE(out, rate * 2, 4);  // byte rate
    writeLE(out, 2, 2);         // block align
    writeLE(out, 16, 2);        // bits per sample
    out.write("data", 4);
    writeLE(out, dataBytes, 4);
    for (float x : samples) {
        const auto v = static_cast<int16_t>(std::lround(std::clamp(x, -1.0f, 1.0f) * 32767.0f));
        writeLE(out, static_cast<uint16_t>(v), 2);
    }
    return static_cast<bool>(out);
}

double estimateFrequencyHz(const std::vector<float>& samples, double sampleRate, size_t begin, size_t end) {
    end = std::min(end, samples.size());
    size_t first = 0, last = 0, crossings = 0;
    for (size_t i = begin + 1; i < end; ++i) {
        if (samples[i - 1] < 0.0f && samples[i] >= 0.0f) {
            if (crossings == 0)
                first = i;
            last = i;
            ++crossings;
        }
    }
    if (crossings < 2)
        return 0.0;
    // Sub-sample position of a crossing, by linear interpolation.
    auto frac = [&](size_t i) {
        const double a = samples[i - 1], b = samples[i];
        return static_cast<double>(i - 1) + (-a / (b - a));
    };
    return static_cast<double>(crossings - 1) * sampleRate / (frac(last) - frac(first));
}

} // namespace maqam
