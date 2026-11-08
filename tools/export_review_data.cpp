// Exports the data the tuning review page needs: every maqam form with note names, cents,
// frequencies on the reference tonic, roles, and the ajnas/ghammaz structure to confirm.
//
// Usage: export_review_data [output.json] [--a4 HZ] [--data PATH]
//   Default output: review/review-data.json

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "core/MaqamData.h"
#include "core/NoteNames.h"
#include "core/Tuning.h"

using namespace maqam;
using nlohmann::json;
namespace fs = std::filesystem;

namespace {

json notesJson(const Maqam& m, const ResolvedScale& s, const Tonic& tonic, double a4) {
    const auto names = spellScale(s, tonic);
    json notes = json::array();
    for (int d = 1; d <= s.numDegrees(); ++d) {
        const auto i = static_cast<size_t>(d - 1);
        json roles = json::array();
        for (auto r : rolesForDegree(m, s, tonic, d))
            roles.push_back(toString(r));
        notes.push_back({{"degree", d},
                         {"name", names[i].compact(false, false)},
                         {"name_words", names[i].words()},
                         {"cents", s.cents[i]},
                         {"hz", degreeFrequencyHz(s, tonic, d, 0, a4)},
                         {"roles", roles}});
    }
    return notes;
}

json ajnasJson(const MaqamLibrary& lib, const ResolvedScale& s, const Tonic& tonic) {
    const auto names = spellScale(s, tonic);
    json out = json::array();
    for (const auto& p : s.ajnas) {
        const JinsDef* def = lib.findJins(p.jinsId);
        out.push_back({{"jins", p.jinsId},
                       {"name", def ? def->name : p.jinsId},
                       {"name_ar", def ? def->nameAr : ""},
                       {"root_degree", p.rootDegree},
                       {"root_name", names[static_cast<size_t>(p.rootDegree - 1)].compact(false, false)}});
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string outPath = "review/review-data.json";
    std::string dataPath = MAQAM_DATA_FILE;
    double a4 = kDefaultA4Hz;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--a4" && i + 1 < argc)
            a4 = std::atof(argv[++i]);
        else if (arg == "--data" && i + 1 < argc)
            dataPath = argv[++i];
        else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "usage: export_review_data [output.json] [--a4 HZ] [--data PATH]\n";
            return 2;
        } else
            outPath = arg;
    }

    try {
        const MaqamLibrary lib = loadLibraryFromFile(dataPath);
        json maqamat = json::array();
        for (const Maqam& m : lib.maqamat) {
            const Tonic tonic = Tonic::fromCentsAboveC(m.referenceTonic.centsAboveC);
            const auto up = *resolveScale(m, Direction::Ascending);
            const auto names = spellScale(up, tonic);

            json forms = json::array();
            forms.push_back({{"id", "ascending"}, {"label", "Ascending"}, {"notes", notesJson(m, up, tonic, a4)},
                             {"ajnas", ajnasJson(lib, up, tonic)}});
            if (auto down = resolveScale(m, Direction::Descending))
                forms.push_back({{"id", "descending"}, {"label", "Descending"},
                                 {"notes", notesJson(m, *down, tonic, a4)}, {"ajnas", ajnasJson(lib, *down, tonic)}});
            for (const auto& v : m.ascendingVariants) {
                const auto s = *resolveScale(m, Direction::Ascending, v.id);
                forms.push_back({{"id", v.id}, {"label", v.name}, {"notes", notesJson(m, s, tonic, a4)},
                                 {"ajnas", ajnasJson(lib, s, tonic)}});
            }

            json characteristic = json::array();
            for (int d : m.characteristicDegrees)
                characteristic.push_back({{"degree", d}, {"name", names[static_cast<size_t>(d - 1)].compact(false, false)}});

            maqamat.push_back({
                {"id", m.id},
                {"name", m.name},
                {"name_ar", m.nameAr},
                {"reviewed", m.reviewed},
                {"octave_repeating", m.octaveRepeating},
                {"tonic", {{"name", spellTonic(tonic).compact(true)},
                           {"traditional_name", m.referenceTonic.traditionalName},
                           {"hz", frequencyHz(tonic, 0.0, a4)}}},
                {"ghammaz", {{"degree", m.ghammazDegree},
                             {"name", names[static_cast<size_t>(m.ghammazDegree - 1)].compact(false, false)}}},
                {"characteristic", characteristic},
                {"has_descending", m.descending.has_value()},
                {"forms", forms},
            });
        }

        const json out = {{"version", lib.version}, {"a4_hz", a4}, {"maqamat", maqamat}};
        if (const fs::path parent = fs::path(outPath).parent_path(); !parent.empty())
            fs::create_directories(parent);
        std::ofstream f(outPath, std::ios::binary);
        f << out.dump(1) << "\n";
        if (!f) {
            std::cerr << "error: cannot write " << outPath << "\n";
            return 1;
        }
        std::cout << "Wrote " << outPath << " (" << maqamat.size() << " maqamat)\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
