// Exports every maqam form as tuning files for Ableton Live 12 and Scala-aware synths.
//
// Usage: export_tunings [output_dir] [--tonic-midi N] [--reference-tonic] [--a4 HZ] [--data PATH]
//   Default: tonic F4 (MIDI 65), A4 = 440 Hz, output to ./tunings
//
// For each maqam form (ascending, descending if specified, each ascending variant) it writes,
// in both layouts (12key, degrees):
//   <name>.ascl   Ableton Live 12 (Tuning in the Live browser, or drag onto a track)
//   <name>.scl + <name>.kbm   Scala, for Surge XT, Pianoteq, MTS-ESP Mini and others

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "core/KeyMapping.h"
#include "core/MaqamData.h"
#include "core/Tuning.h"

using namespace maqam;
namespace fs = std::filesystem;

namespace {

bool writeText(const fs::path& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary);
    out << text;
    return static_cast<bool>(out);
}

int usage() {
    std::cerr << "usage: export_tunings [output_dir] [--tonic-midi N] [--reference-tonic] "
                 "[--a4 HZ] [--data PATH]\n";
    return 2;
}

} // namespace

int main(int argc, char** argv) {
    std::string outDir = "tunings";
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
        int files = 0;

        for (const Maqam& m : lib.maqamat) {
            const Tonic tonic = useReferenceTonic ? Tonic::fromCentsAboveC(m.referenceTonic.centsAboveC)
                                                  : Tonic{tonicMidi, 0.0};

            struct Form { std::string suffix, label; ResolvedScale scale; };
            std::vector<Form> forms;
            forms.push_back({"", "ascending", *resolveScale(m, Direction::Ascending)});
            if (auto down = resolveScale(m, Direction::Descending))
                forms.push_back({"_descending", "descending", *down});
            for (const auto& v : m.ascendingVariants)
                forms.push_back({"_" + v.id, v.name, *resolveScale(m, Direction::Ascending, v.id)});

            for (const auto& f : forms) {
                for (KeyLayout layout : {KeyLayout::TwelveKey, KeyLayout::Degrees}) {
                    const bool twelve = layout == KeyLayout::TwelveKey;
                    const TuningTable table = twelve ? makeTwelveKeyTable(f.scale, tonic, a4)
                                                     : makeDegreesTable(f.scale, tonic, a4);
                    const std::string base = m.id + f.suffix + (twelve ? "_12key" : "_degrees") +
                                             (m.reviewed ? "" : "_DRAFT");
                    std::string desc = "Maqam " + m.name + " (" + f.label + "), " +
                                       (twelve ? "12-key layout" : "one degree per key") +
                                       ", tonic on MIDI " + std::to_string(tonic.midiNote);
                    if (!m.reviewed)
                        desc += ". DRAFT: not yet reviewed by a player or teacher";
                    if (!f.scale.octaveRepeating && !twelve)
                        desc += ". Upper register approximated";

                    const fs::path dir(outDir);
                    const bool ok = writeText(dir / (base + ".ascl"), toAscl(table, desc, "Maqam Master")) &&
                                    writeText(dir / (base + ".scl"), toScl(table, desc)) &&
                                    writeText(dir / (base + ".kbm"), toKbm(table));
                    if (!ok) {
                        std::cerr << "error: cannot write " << base << " in " << outDir << "\n";
                        return 1;
                    }
                    files += 3;
                }
            }
        }
        std::cout << "Wrote " << files << " files to " << outDir << "\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
