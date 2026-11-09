#pragma once

// The pitch of every MIDI key, as the synth sees it, and a lock-free way to hand a new one to
// the audio thread (maqam, tonic or mandal changes) without allocating. UI-free, no JUCE.

#include <array>
#include <atomic>

#include "core/KeyMapping.h"

namespace maqam::synth {

constexpr int kNumKeys = 128;

struct KeyTuning {
    std::array<double, kNumKeys> hz{};   // frequency each key plays
    std::array<int, kNumKeys> degree{};  // 1-based maqam degree, 0 if the key is outside the maqam
    std::array<int, kNumKeys> snapKey{}; // nearest maqam key in pitch (itself for maqam keys);
                                         // on an exact tie the lower one
    bool hasMaqam = false;               // false for plain 12-TET: no key counts as "outside"
    double tonicHz = 0.0;                // the maqam's tonic (for the drone); 0 for plain 12-TET
};

/** Every key from a tuning table. Keys outside the maqam play their 12-TET pitch (the table
    already holds that) and know their nearest maqam key, for the Snap note limit. */
KeyTuning makeKeyTuning(const TuningTable& table);

/** Plain 12-TET, used before any maqam is chosen. */
KeyTuning twelveTetKeyTuning(double a4Hz = kDefaultA4Hz);

/** Triple buffer: one writer thread publishes, the audio thread fetches. Neither locks or
    allocates, and the reader's tuning stays valid until its next fetch(). */
class TuningExchange {
public:
    explicit TuningExchange(const KeyTuning& initial = twelveTetKeyTuning());

    /** Writer thread only (one writer at a time). */
    void publish(const KeyTuning& tuning);

    /** Audio thread: true if a newer tuning was published since the last call; current() then
        returns it. */
    bool fetch();
    const KeyTuning& current() const { return buffers_[static_cast<size_t>(reader_)]; }

private:
    static constexpr int kIndexMask = 3;
    static constexpr int kDirty = 4;
    std::array<KeyTuning, 3> buffers_;
    int writer_ = 0;
    int reader_ = 1;
    std::atomic<int> middle_{2};
};

} // namespace maqam::synth
