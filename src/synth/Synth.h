#pragma once

// The built-in polyphonic synth. Every voice plays an exact frequency from the KeyTuning
// (never MIDI pitch bend), so quarter tones are exact and a retune moves held notes too.
// UI-free, no JUCE; real-time safe after prepare(): no allocation or locks in the note and
// render calls.
//
// Sounds:
//   Pluck - plucked string (Karplus-Strong) played through an oud-like body (fixed
//           resonances). The default.
//   Synth - saw + square + a sub an octave down, through a low-pass filter with an envelope.
//           Two more saws sit a few cents above and below, mirrored so they only swell and
//           fade the note: the pitch stays exact.
//   Ney   - reed flute: a mostly-fundamental tone whose harmonics build with the breath, a
//           lot of breath noise ringing in a tube tuned to the note, and a hiss. Each new note
//           cracks briefly into the octave and is blown up to pitch from slightly flat (settled
//           within ~0.1 s). Its vibrato is mostly breath (loudness and tone), with about a
//           third of the pitch swing the other sounds get.
// The summed voices go through a soft clipper: untouched below a knee, rounded off above it.
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
//   Pitch bend - the pitch wheel bends every sounding note, either by a fixed range in cents
//                (e.g. +-50 for quarter-tone inflections) or, with "bend to maqam", up to the next
//                maqam degree above / below the note, so a full bend always lands on a maqam
//                pitch. Smoothed over a few ms so a stepping wheel never clicks.
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
    float bendRangeCents = 200.0f;  // full wheel travel, each way
    bool bendToMaqam = false;       // full travel reaches the next maqam degree instead
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
    /** Pitch bend in cents, eased in over a few ms (or set at once, for a note starting bent). */
    void setBend(double cents, bool immediate) {
        bendTarget_ = cents;
        if (immediate)
            bendNow_ = cents;
    }
    double bendCents() const { return bendNow_; }
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
    double bendTarget_ = 0.0, bendNow_ = 0.0;  // pitch bend in cents

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
    double phaseUp_ = 0.0, phaseDown_ = 0.0, subPhase_ = 0.0;  // Synth: unison saws and sub
    // Ney: breath flutter, the attack's pitch scoop, bore and hiss filters, harmonic wander
    static constexpr int kNeyPartials = 6;
    float flutter_ = 0.0f;
    double scoopCents_ = 0.0, scoopCoef_ = 0.0;
    float boreLp_ = 0.0f, hissLp_ = 0.0f;
    std::array<float, kNeyPartials> wander_{};
    double lastVib_ = 0.0;  // vibrato offset now, in cents
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

    /** Pitch wheel, -1..1 (0 = centre). How far it bends is set by the params. */
    void setPitchBend(float value);
    float pitchBend() const { return pitchBend_; }

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
    /** Pitch bend that voice is applying right now, in cents (0 if none). */
    double bendCents(int key) const;

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
    double bendCentsFor(const Voice& v) const;
    double hzOf(int soundingKey) const { return tuning_->hz[static_cast<size_t>(soundingKey)]; }
    bool mono() const { return params_.mode != VoiceMode::Poly; }

    double sampleRate_ = 44100.0;
    SynthParams params_;
    float modWheel_ = 0.0f;
    float pitchBend_ = 0.0f;
    int keyShift_ = 0;
    KeyTuning defaultTuning_;
    const KeyTuning* tuning_ = nullptr;
    std::array<Voice, kMaxVoices> voices_;
    uint64_t noteCounter_ = 0;
    bool sustain_ = false;

    // Pluck: the instrument body's resonances (state-variable band-passes).
    struct BodyFilter {
        float g = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
    };
    std::array<BodyFilter, 4> body_{};

    // Mono/Legato: keys held down, most recent last.
    std::array<int, kNumKeys> held_{};
    int heldCount_ = 0;
};

} // namespace maqam::synth
