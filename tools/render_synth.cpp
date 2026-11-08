// Plays every maqam through the plugin's built-in synth and writes WAV files, so the real
// sound and its tuning can be heard without a DAW. Notes are played on the 12-key layout the
// plugin uses: up the ascending keys, then down using the descending-form keys where the
// maqam has them.
//
// Usage: render_synth [output_dir] [--sound pluck|synth|ney] [--vibrato 0..1] [--legato GLIDE_S]
//                     [--tonic-midi N] [--a4 HZ] [--data PATH]
//   Default: Pluck, no vibrato, separate notes, tonic F4 (MIDI 65), A4 = 440 Hz, output to ./scales/synth
//   --legato plays each note before releasing the last, gliding over GLIDE_S seconds.

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/KeyMapping.h"
#include "core/MaqamData.h"
#include "core/Render.h"
#include "synth/KeyTuning.h"
#include "synth/Synth.h"

using namespace maqam;
namespace fs = std::filesystem;

int main(int argc, char** argv) {
    std::string outDir = "scales/synth";
    std::string dataPath = MAQAM_DATA_FILE;
    synth::SoundType sound = synth::SoundType::Pluck;
    int tonicMidi = kDefaultTonicMidiNote;
    double a4 = kDefaultA4Hz;
    float vibrato = 0.0f;
    float legato = -1.0f;  // < 0: separate notes
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--sound" && i + 1 < argc) {
            const std::string s = argv[++i];
            sound = s == "synth" ? synth::SoundType::Synth : s == "ney" ? synth::SoundType::Ney : synth::SoundType::Pluck;
        } else if (arg == "--vibrato" && i + 1 < argc) {
            vibrato = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--legato" && i + 1 < argc) {
            legato = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--tonic-midi" && i + 1 < argc) {
            tonicMidi = std::atoi(argv[++i]);
        } else if (arg == "--a4" && i + 1 < argc) {
            a4 = std::atof(argv[++i]);
        } else if (arg == "--data" && i + 1 < argc) {
            dataPath = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "usage: render_synth [output_dir] [--sound pluck|synth|ney] [--vibrato 0..1] [--legato GLIDE_S] "
                         "[--tonic-midi N] [--a4 HZ] [--data PATH]\n";
            return 2;
        } else {
            outDir = arg;
        }
    }

    try {
        const MaqamLibrary lib = loadLibraryFromFile(dataPath);
        fs::create_directories(outDir);
        const double sr = 44100.0;
        const int step = static_cast<int>(0.5 * sr);
        const int hold = static_cast<int>(0.42 * sr);

        for (const Maqam& m : lib.maqamat) {
            const Tonic tonic{tonicMidi, 0.0};
            const TuningTable table = makeTwelveKeyTable(m, tonic, a4);
            const synth::KeyTuning tuning = synth::makeKeyTuning(table);
            const int steps = table.stepsPerPeriod();

            std::set<int> degreesWithAlternate;
            for (int s = 0; s < steps; ++s)
                if (table.alternateOfStep[static_cast<size_t>(s)])
                    degreesWithAlternate.insert(table.degreeOfStep[static_cast<size_t>(s)]);

            std::vector<int> keys;
            for (int k = tonicMidi; k <= tonicMidi + 12; ++k) {
                const int s = ((k - tonicMidi) % steps + steps) % steps;
                if (keyDegree(table, k) != 0 && !table.alternateOfStep[static_cast<size_t>(s)])
                    keys.push_back(k);
            }
            for (int k = tonicMidi + 11; k >= tonicMidi; --k) {
                const int s = ((k - tonicMidi) % steps + steps) % steps;
                const int d = keyDegree(table, k);
                if (d != 0 && (table.alternateOfStep[static_cast<size_t>(s)] || degreesWithAlternate.count(d) == 0))
                    keys.push_back(k);
            }

            synth::Synth player;
            player.prepare(sr);
            synth::SynthParams params;
            params.sound = sound;
            params.decaySeconds = 3.0f;
            params.gain = 0.6f;
            params.vibrato = vibrato;
            if (legato >= 0.0f) {
                params.mode = synth::VoiceMode::Legato;
                params.glideSeconds = legato;
            }
            player.setParams(params);
            player.setTuning(tuning);
            const int noteLength = legato >= 0.0f ? 2 * step : step;  // legato: slower, sung lines
            std::vector<float> out(static_cast<size_t>(noteLength) * (keys.size() + 4));
            size_t pos = 0;
            int previous = -1;
            for (int k : keys) {
                player.noteOn(k, 0.8f);
                if (legato >= 0.0f) {  // overlap: the new note starts before the old one ends
                    if (previous >= 0 && previous != k)
                        player.noteOff(previous);
                    player.render(out.data() + pos, noteLength);
                } else {
                    player.render(out.data() + pos, hold);
                    player.noteOff(k);
                    player.render(out.data() + pos + static_cast<size_t>(hold), step - hold);
                }
                previous = k;
                pos += static_cast<size_t>(noteLength);
            }
            if (previous >= 0)
                player.noteOff(previous);
            player.render(out.data() + pos, static_cast<int>(out.size() - pos));

            const char* soundName = sound == synth::SoundType::Pluck ? "_pluck"
                                    : sound == synth::SoundType::Synth ? "_synth" : "_ney";
            const std::string name = m.id + soundName + (vibrato > 0.0f ? "_vibrato" : "") +
                                     (legato >= 0.0f ? "_legato" : "") + (m.reviewed ? "" : "_DRAFT") + ".wav";
            const std::string path = (fs::path(outDir) / name).string();
            if (!writeWav16(path, out, sr)) {
                std::cerr << "error: cannot write " << path << "\n";
                return 1;
            }
            std::cout << path << "  (" << m.name << ", " << keys.size() << " notes up and down)\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
