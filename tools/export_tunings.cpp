// Exports every maqam form as tuning files for Ableton Live 12 and Scala-aware synths.
//
// Usage: export_tunings [output_dir] [--tonic-midi N] [--reference-tonic] [--a4 HZ] [--data PATH]
//   Default: tonic F4 (MIDI 65), A4 = 440 Hz, output to ./tunings
//
// For each maqam it writes:
//   <id>[_variant]_12key   12-key layout; ascending plus the descending notes on free keys
//   <id>[_form]_degrees    one degree per key; ascending, descending and variants separately
// each as:
//   <name>.ascl   Ableton Live 12 (Tuning in the Live browser, or drag onto a track), with note names
//   <name>.scl + <name>.kbm   Scala, for Surge XT, Pianoteq, MTS-ESP Mini and others

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "core/KeyMapping.h"
#include "core/MaqamData.h"
#include "core/NoteNames.h"
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

            const auto down = resolveScale(m, Direction::Descending);
            const std::string tonicName = spellTonic(tonic).compact(true);

            auto write = [&](const std::string& base, const std::string& label, const TuningTable& table,
                             const std::vector<std::string>& names, bool approximated) {
                std::string desc = "Maqam " + m.name + " (" + label + ") on " + tonicName;
                if (!m.reviewed)
                    desc += ". DRAFT: not yet reviewed by a player or teacher";
                if (approximated)
                    desc += ". Upper register approximated";
                const std::string name = base + (m.reviewed ? "" : "_DRAFT");
                const fs::path dir(outDir);
                const bool ok = writeText(dir / (name + ".ascl"), toAscl(table, desc, "Maqam Master", names)) &&
                                writeText(dir / (name + ".scl"), toScl(table, desc)) &&
                                writeText(dir / (name + ".kbm"), toKbm(table));
                if (!ok)
                    throw std::runtime_error("cannot write " + name + " in " + outDir);
                files += 3;
            };

            // 12-key layout: ascending (or a variant) plus the descending notes on free keys.
            std::vector<std::pair<std::string, std::string>> variants{{"", "ascending"}};
            for (const auto& v : m.ascendingVariants)
                variants.emplace_back(v.id, v.name);
            for (const auto& [id, label] : variants) {
                const auto up = resolveScale(m, Direction::Ascending, id);
                const TuningTable table = makeTwelveKeyTable(m, tonic, a4, id);
                for (int d : table.unplacedAlternateDegrees)
                    std::cerr << "warning: " << m.id << " descending degree " << d << " has no free key\n";
                const std::string fullLabel = std::string(label) + (down ? " + descending" : "") + ", 12-key layout";
                write(m.id + (id.empty() ? "" : "_" + id) + "_12key", fullLabel, table,
                      stepNoteNames(table, *up, down ? &*down : nullptr, tonic), false);
            }

            // One-degree-per-key layout: each form separately.
            struct Form { std::string suffix, label; ResolvedScale scale; };
            std::vector<Form> forms{{"", "ascending", *resolveScale(m, Direction::Ascending)}};
            if (down)
                forms.push_back({"_descending", "descending", *down});
            for (const auto& v : m.ascendingVariants)
                forms.push_back({"_" + v.id, v.name, *resolveScale(m, Direction::Ascending, v.id)});
            for (const auto& f : forms) {
                const TuningTable table = makeDegreesTable(f.scale, tonic, a4);
                write(m.id + f.suffix + "_degrees", f.label + ", one degree per key", table,
                      stepNoteNames(table, f.scale, nullptr, tonic), !f.scale.octaveRepeating);
            }
        }
        std::cout << "Wrote " << files << " files to " << outDir << "\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
