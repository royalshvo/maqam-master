// Renders every maqam scale to a WAV file for listening to the tuning.
//
// Usage: render_scales [output_dir] [--tonic-midi N] [--reference-tonic] [--a4 HZ] [--data PATH]
//   Default: tonic F4 (MIDI 65), A4 = 440 Hz, output to ./scales
//   --reference-tonic  play each maqam on its traditional tonic instead (e.g. Sikah on E half-flat)
//
// Each file plays the ascending scale, then the descending form if the data specifies one,
// then each ascending variant. Every file name carries "DRAFT" while the data is unreviewed.

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include "core/MaqamData.h"
#include "core/Render.h"
#include "core/Tuning.h"

using namespace maqam;
namespace fs = std::filesystem;

namespace {

void appendScale(std::vector<double>& freqs, const ResolvedScale& scale, const Tonic& tonic,
                 double a4, bool descending) {
    const int n = scale.numDegrees();
    for (int i = 0; i < n; ++i) {
        const int degree = descending ? n - i : i + 1;
        freqs.push_back(degreeFrequencyHz(scale, tonic, degree, 0, a4));
    }
}

int usage() {
    std::cerr << "usage: render_scales [output_dir] [--tonic-midi N] [--reference-tonic] "
                 "[--a4 HZ] [--data PATH]\n";
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    std::string outDir = "scales";
    std::string dataPath = MAQAM_DATA_FILE;
    int tonicMidi = kDefaultTonicMidiNote;
    bool useReferenceTonic = false;
    double a4 = kDefaultA4Hz;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        if (arg == "--tonic-midi") {
            const char* v = next();
            if (!v) return usage();
            tonicMidi = std::atoi(v);
        } else if (arg == "--a4") {
            const char* v = next();
            if (!v) return usage();
            a4 = std::atof(v);
        } else if (arg == "--data") {
            const char* v = next();
            if (!v) return usage();
            dataPath = v;
        } else if (arg == "--reference-tonic") {
            useReferenceTonic = true;
        } else if (!arg.empty() && arg[0] == '-') {
            return usage();
        } else {
            outDir = arg;
        }
    }

    try {
        const MaqamLibrary lib = loadLibraryFromFile(dataPath);
        fs::create_directories(outDir);
        const RenderSettings settings;

        for (const Maqam& m : lib.maqamat) {
            const Tonic tonic = useReferenceTonic ? Tonic::fromCentsAboveC(m.referenceTonic.centsAboveC)
                                                  : Tonic{tonicMidi, 0.0};
            std::vector<double> freqs;
            appendScale(freqs, *resolveScale(m, Direction::Ascending), tonic, a4, false);
            if (auto down = resolveScale(m, Direction::Descending))
                appendScale(freqs, *down, tonic, a4, true);
            for (const auto& v : m.ascendingVariants)
                appendScale(freqs, *resolveScale(m, Direction::Ascending, v.id), tonic, a4, false);

            const std::string name = m.id + (m.reviewed ? "" : "_DRAFT") + ".wav";
            const fs::path path = fs::path(outDir) / name;
            if (!writeWav16(path.string(), renderTones(freqs, settings), settings.sampleRate)) {
                std::cerr << "error: cannot write " << path << "\n";
                return 1;
            }
            std::cout << path.string() << "  (" << m.name << ", tonic MIDI " << tonic.midiNote;
            if (tonic.centsOffset != 0.0)
                std::cout << (tonic.centsOffset > 0 ? " +" : " ") << tonic.centsOffset << " cents";
            std::cout << ")\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
