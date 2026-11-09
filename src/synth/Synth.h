#pragma once

// The built-in polyphonic synth. Every voice plays an exact frequency from the KeyTuning
// (never MIDI pitch bend), so quarter tones are exact and a retune moves held notes too.
// UI-free, no JUCE; real-time safe after prepare(): no allocation or locks in the note and
// render calls.
//
// Sounds:
//   Pluck - plucked string (Karplus-Strong), oud/qanun-like. The default.
//   Synth - saw + square through a low-pass filter with an envelope. Both oscillators run at
//           the same frequency (no detune), so the pitch stays exact.
//   Ney   - breathy flute: a soft harmonic tone plus band-passed breath noise and a chiff.
//
// Playing controls:
//   Note limit - Off: every key plays. Mute: keys outside the maqam are silent. Snap: they play
//                the nearest maqam note.
//   Voice mode - Poly; Mono (each note re-attacks, gliding from the last pitch); Legato
//                (overlapping notes slide to the new pitch without re-attacking).
//   Glide      - slide time in Mono/Legato. The slide is linear in cents and ends exactly on the
//                target pitch.
//   Key shift  - octave and transpose: key k plays the pitch of key k + shift, so the maqam
//                layout can sit anywhere under the hands (e.g. +5 puts F's tonic on a C pad).
//                The maqam itself does not move. A note keeps the shift it started with.
//   Vibrato    - sine LFO on pitch, centred on the exact note so the average pitch stays in
//                tune. It fades in after the note starts, as players do. Depth = the Vibrato
//                amount plus the mod wheel.

#include <array>
#include <cstdint>
#include <vector>

#include "synth/KeyTuning.h"

namespace maqam::synth {

enum class SoundType { Pluck = 0, Synth = 1, Ney = 2 };
enum class NoteLimit { Off = 0, Mute = 1, Snap = 2 };
enum class VoiceMode { Poly = 0, Mono = 1, Legato = 2 };

/** Full-depth vibrato, in cents either side of the note. */
constexpr double kMaxVibratoCents = 35.0;

struct SynthParams {
    SoundType sound = SoundType::Pluck;
    float brightness = 0.5f;    // 0..1
    float decaySeconds = 3.0f;  // Pluck: fade by 60 dB; Synth: settle; Ney: sets the release
    float gain = 0.5f;          // linear output gain
    NoteLimit limit = NoteLimit::Off;
    VoiceMode mode = VoiceMode::Poly;
    float glideSeconds = 0.12f;
    float vibrato = 0.0f;       // 0..1, added to the mod wheel
    float vibratoRateHz = 5.5f;
};

class Voice {
public:
    void prepare(double sampleRate);
    void start(int key, double hz, float velocity, const SynthParams& params, uint64_t age);
    void release();
    void kill() { active_ = false; }

    /** Moves the pitch to hz over the given time (0 = jump), without re-attacking. */
    void slideTo(double hz, double seconds);
    /** Starts the pitch at fromHz and slides to the current target (Mono mode). */
    void glideFrom(double fromHz, double seconds);
    /** A tuning change: a very short slide so held notes move without a click. */
    void retune(double hz) { slideTo(hz, 0.015); }
    void setKey(int key) { key_ = key; }
    /** The key shift in force when this note started (see Synth::setKeyShift). */
    void setShift(int semitones) { shift_ = semitones; }
    int shift() const { return shift_; }
    void setVibrato(double depthCents, double rateHz) {
        vibDepthCents_ = depthCents;
        vibRateHz_ = rateHz;
    }

    /** Adds this voice into out. */
    void render(float* out, int numSamples);

    bool active() const { return active_; }
    bool released() const { return released_; }
    int key() const { return key_; }
    uint64_t age() const { return age_; }
    double targetHz() const { return baseHz_; }
    /** Pitch now, including any glide still under way (vibrato excluded). */
    double currentHz() const;
    bool sustained = false;  // note-off arrived while the sustain pedal was down

private:
    double advancePitch(int samples);  // returns the pitch offset in cents (glide + vibrato)
    void tunePluck(double hz);
    void renderPluck(float* out, int numSamples);
    void renderSynth(float* out, int numSamples);
    void renderNey(float* out, int numSamples);
    float white();

    double sampleRate_ = 44100.0;
    SynthParams params_;
    bool active_ = false;
    bool released_ = false;
    int key_ = -1;
    int shift_ = 0;
    uint64_t age_ = 0;
    float velocity_ = 1.0f;

    // Pitch: target frequency, a glide offset that runs linearly to 0, and vibrato.
    double baseHz_ = 440.0;
    double glideCents_ = 0.0;
    double glideStep_ = 0.0;   // cents per sample
    double vibDepthCents_ = 0.0, vibRateHz_ = 5.5, vibPhase_ = 0.0;
    double elapsed_ = 0.0;     // seconds since the note started (for the vibrato fade-in)

    // Pluck
    std::vector<float> line_;
    int write_ = 0;
    int delay_ = 1;        // integer part of the loop delay
    float apCoef_ = 0.0f;  // first-order allpass for the fractional part
    float apIn_ = 0.0f, apOut_ = 0.0f;
    float lpCoef_ = 0.5f;  // two-point loop filter weight (higher = darker)
    float lpPrev_ = 0.0f;
    float loopGain_ = 0.999f;
    float releaseGain_ = 0.99f;
    double pluckHz_ = 440.0;      // frequency the loop is tuned to now

    // Synth and Ney
    double phase_ = 0.0;
    float amp_ = 0.0f, filterEnv_ = 0.0f, chiff_ = 0.0f;
    float attackStep_ = 0.0f, attackCoef_ = 0.0f, decayCoef_ = 0.0f, releaseCoef_ = 0.0f;
    float filterCoef_ = 0.0f, chiffCoef_ = 0.0f;
    float ic1_ = 0.0f, ic2_ = 0.0f;  // SVF state (Synth filter / Ney breath band-pass)
    bool attacking_ = false;

    uint32_t noise_ = 0x9E3779B9u;
};

class Synth {
public:
    static constexpr int kMaxVoices = 16;

    Synth();

    /** Not real-time safe (allocates). Call before rendering and on sample-rate changes. */
    void prepare(double sampleRate);

    /** Changing the voice mode releases everything that is playing. */
    void setParams(const SynthParams& params);
    const SynthParams& params() const { return params_; }

    /** Octave and transpose, in semitones: key k then plays the pitch of key k + semitones.
        Applies to notes started after the call; held notes keep their pitch. */
    void setKeyShift(int semitones) { keyShift_ = semitones; }
    int keyShift() const { return keyShift_; }

    /** Mod wheel, 0..1: adds vibrato depth. */
    void setModWheel(float value);

    /** Points the synth at a tuning (which must outlive its use) and retunes held notes. */
    void setTuning(const KeyTuning& tuning);
    const KeyTuning& tuning() const { return *tuning_; }

    void noteOn(int key, float velocity);
    void noteOff(int key);
    void setSustain(bool down);
    void allNotesOff();   // release everything
    void killAll();       // stop everything at once

    /** Writes numSamples of mono output (overwrites out). */
    void render(float* out, int numSamples);

    int activeVoiceCount() const;
    /** Target frequency of the voice started by this key, or 0 if none. */
    double playingHz(int key) const;
    /** Pitch of that voice right now, including a glide under way (vibrato excluded). */
    double currentHz(int key) const;

    /** The key whose pitch this key plays under the key shift and note limit, or -1 if muted
        (or shifted off the 128 keys). */
    int soundingKey(int key) const { return soundingKey(key, keyShift_); }
    int soundingKey(int key, int shift) const;

private:
    Voice* findVoiceFor(int key);
    void polyNoteOn(int key, float velocity);
    void monoNoteOn(int key, float velocity);
    void monoNoteOff(int key);
    void removeHeld(int key);
    double hzOf(int soundingKey) const { return tuning_->hz[static_cast<size_t>(soundingKey)]; }
    bool mono() const { return params_.mode != VoiceMode::Poly; }

    double sampleRate_ = 44100.0;
    SynthParams params_;
    float modWheel_ = 0.0f;
    int keyShift_ = 0;
    KeyTuning defaultTuning_;
    const KeyTuning* tuning_ = nullptr;
    std::array<Voice, kMaxVoices> voices_;
    uint64_t noteCounter_ = 0;
    bool sustain_ = false;

    // Mono/Legato: keys held down, most recent last.
    std::array<int, kNumKeys> held_{};
    int heldCount_ = 0;
};

} // namespace maqam::synth
