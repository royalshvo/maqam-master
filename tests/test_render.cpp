#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdio>
#include <fstream>

#include "core/MaqamData.h"
#include "core/Render.h"
#include "core/Tuning.h"

using namespace maqam;
using Catch::Approx;

TEST_CASE("M1: rendered Hijaz on F sounds at the right pitches") {
    const auto lib = loadLibraryFromFile(MAQAM_DATA_FILE);
    const auto scale = resolveScale(*lib.findMaqam("hijaz"), Direction::Ascending);
    REQUIRE(scale.has_value());

    std::vector<double> freqs;
    for (int d = 1; d <= scale->numDegrees(); ++d)
        freqs.push_back(degreeFrequencyHz(*scale, Tonic{}, d));

    RenderSettings settings;
    settings.numHarmonics = 1;   // pure sine, so zero crossings give the pitch
    const auto audio = renderTones(freqs, settings);

    const auto noteLen = static_cast<size_t>(settings.noteSeconds * settings.sampleRate);
    const auto stride = noteLen + static_cast<size_t>(settings.gapSeconds * settings.sampleRate);
    REQUIRE(audio.size() == stride * freqs.size());

    for (size_t i = 0; i < freqs.size(); ++i) {
        // Measure the middle of each note, away from the attack and release ramps.
        const size_t begin = i * stride + noteLen / 10;
        const size_t end = i * stride + noteLen * 9 / 10;
        const double measured = estimateFrequencyHz(audio, settings.sampleRate, begin, end);
        // Within 1 cent of the target.
        CHECK(measured == Approx(freqs[i]).epsilon(0.0006));
    }
}

TEST_CASE("WAV writer produces a valid header") {
    const std::string path = "maqam_test_tone.wav";
    const std::vector<float> samples(441, 0.25f);
    REQUIRE(writeWav16(path, samples, 44100.0));

    std::ifstream in(path, std::ios::binary | std::ios::ate);
    REQUIRE(in);
    CHECK(static_cast<size_t>(in.tellg()) == 44 + samples.size() * 2);
    in.seekg(0);
    char riff[4];
    in.read(riff, 4);
    CHECK(std::string(riff, 4) == "RIFF");
    in.close();
    std::remove(path.c_str());
}
