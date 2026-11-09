#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "core/KeyMapping.h"
#include "core/MaqamData.h"
#include "synth/Drone.h"
#include "synth/KeyTuning.h"
#include "synth/Synth.h"

using namespace maqam;
using namespace maqam::synth;
using Catch::Approx;

namespace {

constexpr double kPi = 3.14159265358979323846;

const MaqamLibrary& library() {
    static const MaqamLibrary lib = loadLibraryFromFile(MAQAM_DATA_FILE);
    return lib;
}

TuningTable rastOnF() {
    const Maqam* m = library().findMaqam("rast");
    REQUIRE(m != nullptr);
    return makeTwelveKeyTable(*m, Tonic{});
}

std::vector<float> renderSeconds(Synth& synth, double sampleRate, double seconds, int block = 256) {
    std::vector<float> out(static_cast<size_t>(seconds * sampleRate));
    for (size_t pos = 0; pos < out.size(); pos += static_cast<size_t>(block)) {
        const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), out.size() - pos));
        synth.render(out.data() + pos, n);
    }
    return out;
}

// Fundamental frequency near a guess, from the phase advance of a Hann-windowed DFT bin between
// two windows. Accurate to a small fraction of a cent on harmonic-rich tones.
double measureHz(const std::vector<float>& x, double sampleRate, double guessHz, size_t start,
                 double windowSeconds = 0.1, double gapSeconds = 0.1) {
    const size_t n = static_cast<size_t>(windowSeconds * sampleRate);  // several periods even for low notes
    const size_t gap = static_cast<size_t>(gapSeconds * sampleRate);
    REQUIRE(start + gap + n <= x.size());
    auto phaseAt = [&](size_t s) {
        std::complex<double> sum = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double w = 0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(n - 1));
            sum += w * x[s + i] * std::polar(1.0, -2.0 * kPi * guessHz * static_cast<double>(i) / sampleRate);
        }
        return std::arg(sum);
    };
    const double expected = 2.0 * kPi * guessHz * static_cast<double>(gap) / sampleRate;
    double d = phaseAt(start + gap) - phaseAt(start) - expected;
    d = std::remainder(d, 2.0 * kPi);
    return guessHz + d * sampleRate / (2.0 * kPi * static_cast<double>(gap));
}

double centsBetween(double a, double b) {
    return 1200.0 * std::log2(a / b);
}

double peak(const std::vector<float>& x) {
    double p = 0.0;
    for (float v : x)
        p = std::max(p, static_cast<double>(std::abs(v)));
    return p;
}

} // namespace

TEST_CASE("KeyTuning follows the 12-key table, with 12-TET outside the maqam") {
    const TuningTable table = rastOnF();
    const KeyTuning t = makeKeyTuning(table);
    for (int k = 0; k < kNumKeys; ++k) {
        CHECK(t.hz[static_cast<size_t>(k)] == Approx(keyFrequencyHz(table, k)));
        CHECK(t.degree[static_cast<size_t>(k)] == keyDegree(table, k));
    }
    // Rast on F: the A key plays A half-flat (degree 3); F# is outside the maqam and stays 12-TET.
    CHECK(t.degree[69] == 3);
    CHECK(centsBetween(t.hz[69], midiNoteToHz(69)) == Approx(-50.0).margin(0.01));
    CHECK(t.degree[66] == 0);
    CHECK(t.hz[66] == Approx(midiNoteToHz(66)));
}

TEST_CASE("TuningExchange hands over the newest tuning") {
    TuningExchange ex;
    CHECK_FALSE(ex.fetch());
    CHECK(ex.current().hz[69] == Approx(440.0));

    KeyTuning a = twelveTetKeyTuning(440.0);
    a.hz[69] = 111.0;
    KeyTuning b = twelveTetKeyTuning(440.0);
    b.hz[69] = 222.0;
    ex.publish(a);
    ex.publish(b);  // two publishes before a fetch: the reader gets the newest
    REQUIRE(ex.fetch());
    CHECK(ex.current().hz[69] == Approx(222.0));
    CHECK_FALSE(ex.fetch());
    CHECK(ex.current().hz[69] == Approx(222.0));

    ex.publish(a);
    REQUIRE(ex.fetch());
    CHECK(ex.current().hz[69] == Approx(111.0));
}

TEST_CASE("Both sounds play the exact frequency of the key") {
    const double sampleRate = GENERATE(44100.0, 48000.0, 96000.0);
    const SoundType sound = GENERATE(SoundType::Pluck, SoundType::Synth, SoundType::Ney);
    const double brightness = GENERATE(0.0, 0.5, 1.0);
    const KeyTuning tuning = makeKeyTuning(rastOnF());

    // Low F, the half-flats A and E in two octaves, and a high note.
    for (int key : {41, 57, 64, 69, 76, 81, 93}) {
        Synth synth;
        synth.prepare(sampleRate);
        synth.setParams({sound, static_cast<float>(brightness), 4.0f, 0.5f});
        synth.setTuning(tuning);
        synth.noteOn(key, 0.8f);
        const auto out = renderSeconds(synth, sampleRate, 0.6);
        const double want = tuning.hz[static_cast<size_t>(key)];
        const double got = measureHz(out, sampleRate, want, static_cast<size_t>(0.2 * sampleRate));
        INFO("sound " << static_cast<int>(sound) << ", " << sampleRate << " Hz, brightness " << brightness
                      << ", key " << key << ": want " << want << " Hz, got " << got << " Hz");
        // The Synth oscillators are exact, but while its filter envelope closes the filter's
        // phase lag grows, which reads as up to ~0.6 cent flat on a dark, low note. It stops
        // once the filter settles, so allow 1 cent there.
        // The Ney's breath noise adds a little measurement scatter, so it gets 1 cent too.
        const double tolerance = sound == SoundType::Pluck ? 0.5 : 1.0;
        CHECK(std::abs(centsBetween(got, want)) < tolerance);
        CHECK(peak(out) > 0.01);
        CHECK(peak(out) < 1.0);
    }
}

TEST_CASE("A retune moves a held note to the new pitch") {
    const SoundType sound = GENERATE(SoundType::Pluck, SoundType::Synth, SoundType::Ney);
    const double sr = 48000.0;
    Synth synth;
    synth.prepare(sr);
    synth.setParams({sound, 0.5f, 6.0f, 0.5f});
    const KeyTuning plain = twelveTetKeyTuning();
    synth.setTuning(plain);
    synth.noteOn(69, 0.8f);
    renderSeconds(synth, sr, 0.1);

    const KeyTuning rast = makeKeyTuning(rastOnF());  // A becomes A half-flat
    synth.setTuning(rast);
    CHECK(synth.playingHz(69) == Approx(rast.hz[69]));
    const auto out = renderSeconds(synth, sr, 0.5);
    const double got = measureHz(out, sr, rast.hz[69], static_cast<size_t>(0.15 * sr));
    CHECK(std::abs(centsBetween(got, rast.hz[69])) < 0.5);
}

TEST_CASE("Released notes fade out and free their voice") {
    const SoundType sound = GENERATE(SoundType::Pluck, SoundType::Synth, SoundType::Ney);
    const double sr = 44100.0;
    Synth synth;
    synth.prepare(sr);
    synth.setParams({sound, 0.5f, 8.0f, 0.5f});
    synth.noteOn(60, 1.0f);
    renderSeconds(synth, sr, 0.2);
    CHECK(synth.activeVoiceCount() == 1);
    synth.noteOff(60);
    renderSeconds(synth, sr, 2.0);
    CHECK(synth.activeVoiceCount() == 0);
    const auto tail = renderSeconds(synth, sr, 0.1);
    CHECK(peak(tail) == 0.0);
}

TEST_CASE("The sustain pedal holds released notes until it is lifted") {
    const double sr = 44100.0;
    Synth synth;
    synth.prepare(sr);
    synth.setParams({SoundType::Synth, 0.5f, 8.0f, 0.5f});
    synth.setSustain(true);
    synth.noteOn(60, 1.0f);
    synth.noteOff(60);
    renderSeconds(synth, sr, 1.0);
    CHECK(synth.activeVoiceCount() == 1);
    synth.setSustain(false);
    renderSeconds(synth, sr, 2.0);
    CHECK(synth.activeVoiceCount() == 0);
}

TEST_CASE("Polyphony is capped and the same key restrikes its own voice") {
    const double sr = 44100.0;
    Synth synth;
    synth.prepare(sr);
    synth.setParams({SoundType::Pluck, 0.5f, 8.0f, 0.5f});
    for (int k = 40; k < 40 + Synth::kMaxVoices + 8; ++k)
        synth.noteOn(k, 0.7f);
    CHECK(synth.activeVoiceCount() == Synth::kMaxVoices);
    const auto out = renderSeconds(synth, sr, 0.2);
    for (float v : out)
        REQUIRE(std::isfinite(v));

    Synth one;
    one.prepare(sr);
    one.noteOn(60, 0.7f);
    one.noteOn(60, 0.7f);
    CHECK(one.activeVoiceCount() == 1);
}

TEST_CASE("Silence when nothing is playing, and a note-on with velocity 0 is a note-off") {
    Synth synth;
    synth.prepare(44100.0);
    CHECK(peak(renderSeconds(synth, 44100.0, 0.1)) == 0.0);
    synth.noteOn(60, 0.0f);
    CHECK(synth.activeVoiceCount() == 0);
    synth.noteOn(200, 1.0f);  // out of range: ignored
    CHECK(synth.activeVoiceCount() == 0);
}

// ---------------------------------------------------------------- note limit

TEST_CASE("Snap keys point at the nearest maqam note") {
    const KeyTuning t = makeKeyTuning(rastOnF());
    CHECK(t.hasMaqam);
    CHECK_FALSE(twelveTetKeyTuning().hasMaqam);
    // Rast on F: G# (68) is 100 cents above G but only 50 below A half-flat (69).
    CHECK(t.snapKey[68] == 69);
    // F# (66) is exactly 100 cents from F and from G: the tie goes to the lower key.
    CHECK(t.snapKey[66] == 65);
    // Maqam keys snap to themselves.
    CHECK(t.snapKey[69] == 69);
    for (int k = 0; k < kNumKeys; ++k)
        CHECK(t.degree[static_cast<size_t>(t.snapKey[static_cast<size_t>(k)])] != 0);
}

TEST_CASE("Note limit: Off plays every key, Mute silences outside keys, Snap moves them") {
    const KeyTuning tuning = makeKeyTuning(rastOnF());
    Synth synth;
    synth.prepare(44100.0);
    synth.setTuning(tuning);

    SynthParams p;
    p.limit = NoteLimit::Off;
    synth.setParams(p);
    synth.noteOn(68, 0.8f);
    CHECK(synth.playingHz(68) == Approx(tuning.hz[68]));
    synth.killAll();

    p.limit = NoteLimit::Mute;
    synth.setParams(p);
    synth.noteOn(68, 0.8f);
    CHECK(synth.activeVoiceCount() == 0);
    synth.noteOn(69, 0.8f);  // maqam keys still play
    CHECK(synth.playingHz(69) == Approx(tuning.hz[69]));
    synth.killAll();

    p.limit = NoteLimit::Snap;
    synth.setParams(p);
    synth.noteOn(68, 0.8f);
    CHECK(synth.playingHz(68) == Approx(tuning.hz[69]));  // G# plays A half-flat
    synth.noteOff(68);                                    // and releases on its own key
    CHECK(synth.playingHz(68) == Approx(tuning.hz[69]));
}

TEST_CASE("Note limit does nothing on plain 12-TET, where no key is outside a maqam") {
    Synth synth;
    synth.prepare(44100.0);
    SynthParams p;
    p.limit = NoteLimit::Mute;
    synth.setParams(p);
    synth.noteOn(66, 0.8f);
    CHECK(synth.activeVoiceCount() == 1);
}

// ---------------------------------------------------------------- mono / legato / glide

TEST_CASE("Mono plays one note at a time and returns to a key still held") {
    const SoundType sound = GENERATE(SoundType::Pluck, SoundType::Synth, SoundType::Ney);
    const KeyTuning tuning = makeKeyTuning(rastOnF());
    Synth synth;
    synth.prepare(44100.0);
    synth.setTuning(tuning);
    SynthParams p;
    p.sound = sound;
    p.mode = GENERATE(VoiceMode::Mono, VoiceMode::Legato);
    p.glideSeconds = 0.0f;
    synth.setParams(p);

    synth.noteOn(65, 0.8f);
    synth.noteOn(69, 0.8f);
    CHECK(synth.activeVoiceCount() == 1);
    CHECK(synth.playingHz(65) == 0.0);
    CHECK(synth.playingHz(69) == Approx(tuning.hz[69]));
    synth.noteOff(69);  // F is still held
    CHECK(synth.playingHz(65) == Approx(tuning.hz[65]));
    synth.noteOff(65);
    std::vector<float> out(2 * 44100);
    synth.render(out.data(), static_cast<int>(out.size()));
    CHECK(synth.activeVoiceCount() == 0);
}

TEST_CASE("Legato glide slides between the exact pitches and lands on the target") {
    const SoundType sound = GENERATE(SoundType::Pluck, SoundType::Synth, SoundType::Ney);
    const double sr = 48000.0;
    const KeyTuning tuning = makeKeyTuning(rastOnF());
    Synth synth;
    synth.prepare(sr);
    synth.setTuning(tuning);
    SynthParams p;
    p.sound = sound;
    p.decaySeconds = 8.0f;
    p.mode = VoiceMode::Legato;
    p.glideSeconds = 0.1f;
    synth.setParams(p);

    synth.noteOn(65, 0.8f);  // F
    renderSeconds(synth, sr, 0.2);
    synth.noteOn(69, 0.8f);  // A half-flat, overlapping: slide
    renderSeconds(synth, sr, 0.05, 48);
    const double mid = synth.currentHz(69);
    CHECK(mid > tuning.hz[65] * 1.01);
    CHECK(mid < tuning.hz[69] * 0.99);
    // Halfway through a glide that is linear in cents, the pitch is halfway in cents.
    CHECK(centsBetween(mid, tuning.hz[65]) == Approx(0.5 * centsBetween(tuning.hz[69], tuning.hz[65])).margin(15.0));

    const auto out = renderSeconds(synth, sr, 0.5);
    CHECK(synth.currentHz(69) == Approx(tuning.hz[69]));
    const double got = measureHz(out, sr, tuning.hz[69], static_cast<size_t>(0.1 * sr));
    CHECK(std::abs(centsBetween(got, tuning.hz[69])) < (sound == SoundType::Pluck ? 0.5 : 1.0));
}

TEST_CASE("Mono re-attacks but still glides from the last pitch; separate notes in Legato do not glide") {
    const KeyTuning tuning = makeKeyTuning(rastOnF());
    Synth synth;
    synth.prepare(48000.0);
    synth.setTuning(tuning);
    SynthParams p;
    p.sound = SoundType::Synth;
    p.mode = VoiceMode::Mono;
    p.glideSeconds = 0.2f;
    synth.setParams(p);
    synth.noteOn(65, 0.8f);
    renderSeconds(synth, 48000.0, 0.1);
    synth.noteOff(65);
    synth.noteOn(69, 0.8f);
    CHECK(synth.currentHz(69) == Approx(tuning.hz[65]).epsilon(0.001));  // starts from F

    p.mode = VoiceMode::Legato;
    synth.setParams(p);  // a mode change releases everything
    renderSeconds(synth, 48000.0, 1.0);
    synth.noteOn(65, 0.8f);
    synth.noteOff(65);
    synth.noteOn(69, 0.8f);  // not overlapping: no glide
    CHECK(synth.currentHz(69) == Approx(tuning.hz[69]));
}

// ---------------------------------------------------------------- vibrato

namespace {

// Pitch spread (max - min, in cents) of short-window measurements across one vibrato cycle.
double vibratoSpreadCents(const std::vector<float>& out, double sr, double hz, double fromSeconds) {
    double lo = 1e9, hi = -1e9;
    for (int i = 0; i < 20; ++i) {
        const size_t start = static_cast<size_t>((fromSeconds + i * 0.01) * sr);
        const double c = centsBetween(measureHz(out, sr, hz, start, 0.02, 0.01), hz);
        lo = std::min(lo, c);
        hi = std::max(hi, c);
    }
    return hi - lo;
}

} // namespace

TEST_CASE("Vibrato swings around the exact pitch and its average stays in tune") {
    const SoundType sound = GENERATE(SoundType::Pluck, SoundType::Synth, SoundType::Ney);
    const double sr = 48000.0;
    const KeyTuning tuning = makeKeyTuning(rastOnF());
    Synth synth;
    synth.prepare(sr);
    synth.setTuning(tuning);
    SynthParams p;
    p.sound = sound;
    p.decaySeconds = 10.0f;
    p.vibrato = 1.0f;
    p.vibratoRateHz = 5.0f;
    synth.setParams(p);
    synth.noteOn(69, 0.8f);  // A half-flat
    const auto out = renderSeconds(synth, sr, 1.6);
    const double hz = tuning.hz[69];

    // Windows two vibrato cycles long, two cycles apart: the swing cancels, the mean remains.
    const double mean = measureHz(out, sr, hz, static_cast<size_t>(0.6 * sr), 0.4, 0.4);
    INFO("mean pitch " << centsBetween(mean, hz) << " cents");
    CHECK(std::abs(centsBetween(mean, hz)) < 0.5);

    // It really moves: close to the full +-35 cents swing.
    CHECK(vibratoSpreadCents(out, sr, hz, 0.8) > 40.0);
}

TEST_CASE("The mod wheel adds vibrato; with both at zero the pitch is steady") {
    const double sr = 48000.0;
    const KeyTuning tuning = makeKeyTuning(rastOnF());
    for (float wheel : {0.0f, 1.0f}) {
        Synth synth;
        synth.prepare(sr);
        synth.setTuning(tuning);
        SynthParams p;
        p.sound = SoundType::Synth;
        p.vibrato = 0.0f;
        synth.setParams(p);
        synth.setModWheel(wheel);
        synth.noteOn(69, 0.8f);
        const auto out = renderSeconds(synth, sr, 1.2);
        const double spread = vibratoSpreadCents(out, sr, tuning.hz[69], 0.8);
        INFO("mod wheel " << wheel << ": spread " << spread << " cents");
        if (wheel == 0.0f)
            CHECK(spread < 1.0);
        else
            CHECK(spread > 40.0);
    }
}

TEST_CASE("Vibrato starts after the note has begun") {
    const double sr = 48000.0;
    Synth synth;
    synth.prepare(sr);
    SynthParams p;
    p.sound = SoundType::Synth;
    p.vibrato = 1.0f;
    synth.setParams(p);
    synth.noteOn(69, 0.8f);
    const auto out = renderSeconds(synth, sr, 0.4);
    // The first 0.1 s are straight.
    double lo = 1e9, hi = -1e9;
    for (int i = 0; i < 6; ++i) {
        const double c = centsBetween(measureHz(out, sr, 440.0, static_cast<size_t>((0.01 + i * 0.01) * sr), 0.02, 0.01), 440.0);
        lo = std::min(lo, c);
        hi = std::max(hi, c);
    }
    CHECK(hi - lo < 1.0);
}

namespace {

std::vector<float> renderDrone(Drone& drone, double sampleRate, double seconds, int block = 256) {
    std::vector<float> out(static_cast<size_t>(seconds * sampleRate), 0.0f);
    for (size_t pos = 0; pos < out.size(); pos += static_cast<size_t>(block)) {
        const int n = static_cast<int>(std::min<size_t>(static_cast<size_t>(block), out.size() - pos));
        drone.render(out.data() + pos, n);
    }
    return out;
}

double maxStep(const std::vector<float>& x) {
    double m = 0.0;
    for (size_t i = 1; i < x.size(); ++i)
        m = std::max(m, static_cast<double>(std::abs(x[i] - x[i - 1])));
    return m;
}

} // namespace

TEST_CASE("KeyTuning carries the maqam's tonic for the drone; plain 12-TET has none") {
    const KeyTuning t = makeKeyTuning(rastOnF());
    CHECK(t.tonicHz == Approx(midiNoteToHz(65)).epsilon(1e-12));
    CHECK(twelveTetKeyTuning().tonicHz == 0.0);
}

TEST_CASE("The drone plays the exact tonic in the chosen register") {
    const double sr = GENERATE(44100.0, 48000.0);
    const int octave = GENERATE(-2, -1, 0);
    const KeyTuning t = makeKeyTuning(rastOnF());
    Drone drone;
    drone.prepare(sr);
    drone.setParams({true, 0.25f, octave});
    drone.setTonicHz(t.tonicHz);
    const auto x = renderDrone(drone, sr, 1.0);
    const double expected = t.tonicHz * std::exp2(octave);
    CHECK(drone.targetHz() == Approx(expected));
    CHECK(std::abs(centsBetween(measureHz(x, sr, expected, static_cast<size_t>(0.6 * sr)), expected)) < 0.2);
    CHECK(peak(x) > 0.1);
    CHECK(peak(x) < 0.5);
}

TEST_CASE("The drone slides to a new tonic without a jump and lands exactly on it") {
    const double sr = 48000.0;
    const Maqam* rast = library().findMaqam("rast");
    REQUIRE(rast != nullptr);
    const KeyTuning onF = makeKeyTuning(makeTwelveKeyTable(*rast, Tonic{65, 0.0}));
    const KeyTuning onG = makeKeyTuning(makeTwelveKeyTable(*rast, Tonic{67, 0.0}));
    Drone drone;
    drone.prepare(sr);
    drone.setParams({true, 0.25f, -1});
    drone.setTonicHz(onF.tonicHz);
    const auto before = renderDrone(drone, sr, 1.5);
    drone.setTonicHz(onG.tonicHz, 0.3);
    const auto after = renderDrone(drone, sr, 1.0);
    // No discontinuity: the sample-to-sample step stays within what the steady tone shows.
    CHECK(maxStep(after) < 1.2 * maxStep(before) + 1e-3);
    CHECK(std::abs(after.front() - before.back()) < 1.2 * maxStep(before) + 1e-3);
    const double expected = onG.tonicHz / 2.0;
    CHECK(std::abs(centsBetween(measureHz(after, sr, expected, static_cast<size_t>(0.5 * sr)), expected)) < 0.2);
}

TEST_CASE("The drone is silent when off or without a maqam, and fades in and out smoothly") {
    const double sr = 44100.0;
    Drone drone;
    drone.prepare(sr);

    drone.setParams({false, 0.25f, -1});
    drone.setTonicHz(makeKeyTuning(rastOnF()).tonicHz);
    CHECK(peak(renderDrone(drone, sr, 0.2)) == 0.0);

    Drone plain;
    plain.prepare(sr);
    plain.setParams({true, 0.25f, -1});
    plain.setTonicHz(twelveTetKeyTuning().tonicHz);
    CHECK(peak(renderDrone(plain, sr, 0.2)) == 0.0);

    drone.setParams({true, 0.25f, -1});
    const auto in = renderDrone(drone, sr, 2.0);
    CHECK(std::abs(in.front()) < 1e-3);
    const std::vector<float> firstMs(in.begin(), in.begin() + static_cast<long>(sr * 0.001));
    const std::vector<float> lastPart(in.end() - static_cast<long>(sr * 0.1), in.end());
    CHECK(peak(firstMs) < 0.05 * peak(lastPart));

    drone.setParams({false, 0.25f, -1});
    const auto out = renderDrone(drone, sr, 3.0);
    CHECK(maxStep(out) <= maxStep(in) + 1e-6);
    const std::vector<float> tail(out.end() - static_cast<long>(sr * 0.1), out.end());
    CHECK(peak(tail) < 1e-4);
    CHECK_FALSE(drone.sounding());
}
