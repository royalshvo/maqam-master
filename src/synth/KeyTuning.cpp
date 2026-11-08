#include "synth/KeyTuning.h"

#include <cmath>
#include <limits>

namespace maqam::synth {

KeyTuning makeKeyTuning(const TuningTable& table) {
    KeyTuning t;
    std::array<double, kNumKeys> cents{};
    for (size_t k = 0; k < static_cast<size_t>(kNumKeys); ++k) {
        t.hz[k] = keyFrequencyHz(table, static_cast<int>(k));
        t.degree[k] = keyDegree(table, static_cast<int>(k));
        cents[k] = 1200.0 * std::log2(t.hz[k]);
        t.hasMaqam = t.hasMaqam || t.degree[k] != 0;
    }
    for (size_t k = 0; k < static_cast<size_t>(kNumKeys); ++k) {
        t.snapKey[k] = static_cast<int>(k);
        if (t.degree[k] != 0)
            continue;
        double best = std::numeric_limits<double>::max();
        for (size_t j = 0; j < static_cast<size_t>(kNumKeys); ++j) {
            if (t.degree[j] == 0)
                continue;
            const double d = std::abs(cents[j] - cents[k]);
            // Scanning upwards with a strict '<' keeps the lower key on an exact tie; the small
            // margin stops rounding from picking the upper key of a true tie.
            if (d < best - 1e-6) {
                best = d;
                t.snapKey[k] = static_cast<int>(j);
            }
        }
    }
    return t;
}

KeyTuning twelveTetKeyTuning(double a4Hz) {
    KeyTuning t;
    for (int k = 0; k < kNumKeys; ++k) {
        t.hz[static_cast<size_t>(k)] = midiNoteToHz(k, a4Hz);
        t.snapKey[static_cast<size_t>(k)] = k;
    }
    return t;
}

TuningExchange::TuningExchange(const KeyTuning& initial) {
    buffers_.fill(initial);
}

void TuningExchange::publish(const KeyTuning& tuning) {
    buffers_[static_cast<size_t>(writer_)] = tuning;
    writer_ = middle_.exchange(writer_ | kDirty, std::memory_order_acq_rel) & kIndexMask;
}

bool TuningExchange::fetch() {
    if ((middle_.load(std::memory_order_acquire) & kDirty) == 0)
        return false;
    reader_ = middle_.exchange(reader_, std::memory_order_acq_rel) & kIndexMask;
    return true;
}

} // namespace maqam::synth
