#include "synth/TempoSync.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace maqam::synth {

int noteValueIndex(const char* name) {
    for (size_t i = 0; i < kNoteValues.size(); ++i)
        if (std::strcmp(kNoteValues[i].name, name) == 0)
            return static_cast<int>(i);
    return -1;
}

double noteValueSeconds(int index, double bpm) {
    if (!(bpm >= 1.0 && bpm <= 999.0))  // also catches NaN
        bpm = kDefaultBpm;
    index = std::clamp(index, 0, static_cast<int>(kNoteValues.size()) - 1);
    return kNoteValues[static_cast<size_t>(index)].beats * 60.0 / bpm;
}

double noteValueRateHz(int index, double bpm, double minHz, double maxHz) {
    return std::clamp(1.0 / noteValueSeconds(index, bpm), minHz, maxHz);
}

} // namespace maqam::synth
