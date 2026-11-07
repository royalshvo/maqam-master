#include "core/MaqamData.h"

#include <cmath>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace maqam {

using nlohmann::json;

namespace {

[[noreturn]] void fail(const std::string& where, const std::string& what) {
    throw DataError("maqams.json: " + where + ": " + what);
}

const json& require(const json& obj, const char* key, const std::string& where) {
    auto it = obj.find(key);
    if (it == obj.end())
        fail(where, std::string("missing field '") + key + "'");
    return *it;
}

std::string requireString(const json& obj, const char* key, const std::string& where) {
    const json& v = require(obj, key, where);
    if (!v.is_string())
        fail(where, std::string("'") + key + "' must be a string");
    return v.get<std::string>();
}

std::string optionalString(const json& obj, const char* key) {
    auto it = obj.find(key);
    return (it != obj.end() && it->is_string()) ? it->get<std::string>() : std::string();
}

std::vector<double> requireCents(const json& obj, const char* key, const std::string& where) {
    const json& v = require(obj, key, where);
    if (!v.is_array() || v.empty())
        fail(where, std::string("'") + key + "' must be a non-empty array of numbers");
    std::vector<double> out;
    for (const auto& c : v) {
        if (!c.is_number())
            fail(where, std::string("'") + key + "' must contain only numbers");
        out.push_back(c.get<double>());
    }
    for (size_t i = 1; i < out.size(); ++i)
        if (out[i] <= out[i - 1])
            fail(where, std::string("'") + key + "' must be strictly increasing");
    return out;
}

bool isVerified(const json& obj) {
    return optionalString(obj, "review") == "verified";
}

int parseDegreeKey(const std::string& key, int numDegrees, const std::string& where) {
    size_t used = 0;
    int degree = 0;
    try {
        degree = std::stoi(key, &used);
    } catch (const std::exception&) {
        fail(where, "degree key '" + key + "' is not a number");
    }
    if (used != key.size())
        fail(where, "degree key '" + key + "' is not an integer");
    if (degree < 1 || degree > numDegrees)
        fail(where, "degree " + key + " is out of range 1.." + std::to_string(numDegrees));
    return degree;
}

std::map<int, double> parseChanges(const json& obj, int numDegrees, const std::string& where) {
    std::map<int, double> out;
    auto it = obj.find("changes");
    if (it == obj.end())
        return out;
    if (!it->is_object())
        fail(where, "'changes' must be an object");
    for (auto& [key, value] : it->items()) {
        if (!value.is_number())
            fail(where, "change for degree " + key + " must be a number");
        out[parseDegreeKey(key, numDegrees, where)] = value.get<double>();
    }
    return out;
}

std::vector<JinsPlacement> parseAjnas(const json& arr, const MaqamLibrary& lib, int numDegrees,
                                      const std::string& where) {
    if (!arr.is_array() || arr.empty())
        fail(where, "'ajnas' must be a non-empty array");
    std::vector<JinsPlacement> out;
    for (const auto& a : arr) {
        JinsPlacement p;
        p.jinsId = requireString(a, "jins", where);
        const json& root = require(a, "root_degree", where);
        if (!root.is_number_integer())
            fail(where, "'root_degree' must be an integer");
        p.rootDegree = root.get<int>();
        const JinsDef* def = lib.findJins(p.jinsId);
        if (def == nullptr)
            fail(where, "unknown jins '" + p.jinsId + "'");
        const int lastDegree = p.rootDegree + static_cast<int>(def->cents.size()) - 1;
        if (p.rootDegree < 1 || lastDegree > numDegrees)
            fail(where, "jins '" + p.jinsId + "' on degree " + std::to_string(p.rootDegree) +
                            " does not fit in " + std::to_string(numDegrees) + " degrees");
        out.push_back(p);
    }
    return out;
}

ScaleChange parseScaleChange(const json& obj, const MaqamLibrary& lib, int numDegrees,
                             const std::string& where) {
    if (!obj.is_object())
        fail(where, "must be an object");
    ScaleChange sc;
    sc.id = optionalString(obj, "id");
    sc.name = optionalString(obj, "name");
    sc.changes = parseChanges(obj, numDegrees, where);
    auto it = obj.find("ajnas");
    if (it != obj.end())
        sc.ajnas = parseAjnas(*it, lib, numDegrees, where);
    return sc;
}

Maqam parseMaqam(const json& m, const MaqamLibrary& lib) {
    Maqam out;
    out.id = requireString(m, "id", "maqam");
    const std::string where = "maqam '" + out.id + "'";
    out.name = requireString(m, "name", where);
    out.nameAr = optionalString(m, "name_ar");
    out.reviewed = isVerified(m);

    const json& rt = require(m, "reference_tonic", where);
    out.referenceTonic.note = requireString(rt, "note", where);
    out.referenceTonic.traditionalName = optionalString(rt, "traditional_name");
    const json& rtCents = require(rt, "cents_above_c", where);
    if (!rtCents.is_number())
        fail(where, "'cents_above_c' must be a number");
    out.referenceTonic.centsAboveC = rtCents.get<double>();
    if (out.referenceTonic.centsAboveC < 0.0 || out.referenceTonic.centsAboveC >= 1200.0)
        fail(where, "'cents_above_c' must be in 0..1199");

    const json& octave = require(m, "octave_repeating", where);
    if (!octave.is_boolean())
        fail(where, "'octave_repeating' must be true or false");
    out.octaveRepeating = octave.get<bool>();

    out.ascending = requireCents(m, "ascending", where);
    if (out.ascending.front() != 0.0)
        fail(where, "ascending scale must start at 0 cents");
    if (out.octaveRepeating && out.ascending.back() != 1200.0)
        fail(where, "an octave-repeating scale must end at 1200 cents");

    const int n = out.numDegrees();
    out.ajnas = parseAjnas(require(m, "ajnas", where), lib, n, where);

    if (auto it = m.find("ascending_variants"); it != m.end()) {
        if (!it->is_array())
            fail(where, "'ascending_variants' must be an array");
        for (const auto& v : *it) {
            ScaleChange sc = parseScaleChange(v, lib, n, where + " variant");
            if (sc.id.empty())
                fail(where, "every ascending variant needs an 'id'");
            if (out.findVariant(sc.id) != nullptr)
                fail(where, "duplicate variant id '" + sc.id + "'");
            out.ascendingVariants.push_back(std::move(sc));
        }
    }

    // "descending" is required so that "not yet specified" is always an explicit null.
    const json& desc = require(m, "descending", where);
    if (!desc.is_null()) {
        ScaleChange sc = parseScaleChange(desc, lib, n, where + " descending");
        sc.id = "descending";
        out.descending = std::move(sc);
    }

    const json& gh = require(m, "ghammaz_degree", where);
    if (!gh.is_number_integer())
        fail(where, "'ghammaz_degree' must be an integer");
    out.ghammazDegree = gh.get<int>();
    if (out.ghammazDegree < 1 || out.ghammazDegree > n)
        fail(where, "'ghammaz_degree' is out of range");

    if (auto it = m.find("characteristic_degrees"); it != m.end()) {
        if (!it->is_array())
            fail(where, "'characteristic_degrees' must be an array");
        for (const auto& d : *it) {
            if (!d.is_number_integer() || d.get<int>() < 1 || d.get<int>() > n)
                fail(where, "characteristic degree out of range");
            out.characteristicDegrees.push_back(d.get<int>());
        }
    }
    return out;
}

} // namespace

const ScaleChange* Maqam::findVariant(const std::string& variantId) const {
    for (const auto& v : ascendingVariants)
        if (v.id == variantId)
            return &v;
    return nullptr;
}

const Maqam* MaqamLibrary::findMaqam(const std::string& id) const {
    for (const auto& m : maqamat)
        if (m.id == id)
            return &m;
    return nullptr;
}

const JinsDef* MaqamLibrary::findJins(const std::string& id) const {
    auto it = ajnas.find(id);
    return it == ajnas.end() ? nullptr : &it->second;
}

const TuningPreset* MaqamLibrary::findPreset(const std::string& id) const {
    for (const auto& p : tuningPresets)
        if (p.id == id)
            return &p;
    return nullptr;
}

MaqamLibrary parseLibrary(const std::string& jsonText) {
    json root;
    try {
        root = json::parse(jsonText);
    } catch (const json::parse_error& e) {
        throw DataError(std::string("maqams.json: invalid JSON: ") + e.what());
    }
    if (!root.is_object())
        fail("root", "must be an object");

    MaqamLibrary lib;
    const json& version = require(root, "version", "root");
    if (!version.is_number_integer())
        fail("root", "'version' must be an integer");
    lib.version = version.get<int>();
    if (lib.version != 2)
        fail("root", "unsupported version " + std::to_string(lib.version) + " (expected 2)");

    if (auto it = root.find("note_roles"); it != root.end() && it->is_array())
        for (const auto& r : *it)
            lib.noteRoles.push_back(r.get<std::string>());

    const json& ajnas = require(root, "ajnas", "root");
    if (!ajnas.is_object() || ajnas.empty())
        fail("root", "'ajnas' must be a non-empty object");
    for (auto& [id, j] : ajnas.items()) {
        const std::string where = "jins '" + id + "'";
        JinsDef def;
        def.id = id;
        def.name = requireString(j, "name", where);
        def.nameAr = optionalString(j, "name_ar");
        def.cents = requireCents(j, "cents", where);
        if (def.cents.front() != 0.0)
            fail(where, "jins cents must start at 0");
        def.reviewed = isVerified(j);
        lib.ajnas.emplace(id, std::move(def));
    }

    const json& maqamat = require(root, "maqamat", "root");
    if (!maqamat.is_array() || maqamat.empty())
        fail("root", "'maqamat' must be a non-empty array");
    for (const auto& m : maqamat) {
        Maqam parsed = parseMaqam(m, lib);
        if (lib.findMaqam(parsed.id) != nullptr)
            fail("maqam '" + parsed.id + "'", "duplicate id");
        lib.maqamat.push_back(std::move(parsed));
    }

    if (auto it = root.find("tuning_presets"); it != root.end()) {
        if (!it->is_array())
            fail("root", "'tuning_presets' must be an array");
        for (const auto& p : *it) {
            TuningPreset preset;
            preset.id = requireString(p, "id", "tuning preset");
            const std::string where = "tuning preset '" + preset.id + "'";
            preset.name = optionalString(p, "name");
            preset.description = optionalString(p, "description");
            if (auto off = p.find("maqam_offsets"); off != p.end()) {
                if (!off->is_object())
                    fail(where, "'maqam_offsets' must be an object");
                for (auto& [maqamId, degrees] : off->items()) {
                    const Maqam* m = lib.findMaqam(maqamId);
                    if (m == nullptr)
                        fail(where, "offsets for unknown maqam '" + maqamId + "'");
                    if (!degrees.is_object())
                        fail(where, "offsets for '" + maqamId + "' must be an object");
                    for (auto& [key, value] : degrees.items()) {
                        if (!value.is_number())
                            fail(where, "offset must be a number");
                        preset.maqamOffsets[maqamId][parseDegreeKey(key, m->numDegrees(), where)] =
                            value.get<double>();
                    }
                }
            }
            lib.tuningPresets.push_back(std::move(preset));
        }
    }
    return lib;
}

MaqamLibrary loadLibraryFromFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw DataError("cannot open " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return parseLibrary(ss.str());
}

namespace {

void checkPlacements(const MaqamLibrary& lib, const std::string& label,
                     const std::vector<double>& scale, const std::vector<JinsPlacement>& ajnas,
                     double tol, std::vector<std::string>& problems) {
    for (const auto& p : ajnas) {
        const JinsDef* def = lib.findJins(p.jinsId);
        if (def == nullptr) {
            problems.push_back(label + ": unknown jins '" + p.jinsId + "'");
            continue;
        }
        const size_t root = static_cast<size_t>(p.rootDegree - 1);
        if (root + def->cents.size() > scale.size()) {
            problems.push_back(label + ": jins '" + p.jinsId + "' does not fit");
            continue;
        }
        for (size_t i = 0; i < def->cents.size(); ++i) {
            const double got = scale[root + i] - scale[root];
            if (std::abs(got - def->cents[i]) > tol) {
                std::ostringstream msg;
                msg << label << ": jins '" << p.jinsId << "' on degree " << p.rootDegree
                    << " expects " << def->cents[i] << " cents at step " << i << ", scale has "
                    << got;
                problems.push_back(msg.str());
                break;
            }
        }
    }
}

std::vector<double> applyChanges(std::vector<double> scale, const std::map<int, double>& changes) {
    for (const auto& [degree, cents] : changes)
        scale[static_cast<size_t>(degree - 1)] = cents;
    return scale;
}

} // namespace

std::vector<std::string> checkConsistency(const MaqamLibrary& lib, double tol) {
    std::vector<std::string> problems;
    for (const auto& m : lib.maqamat) {
        checkPlacements(lib, m.id, m.ascending, m.ajnas, tol, problems);

        bool ghammazIsRoot = false;
        for (const auto& p : m.ajnas)
            ghammazIsRoot = ghammazIsRoot || p.rootDegree == m.ghammazDegree;
        if (!ghammazIsRoot)
            problems.push_back(m.id + ": ghammaz degree " + std::to_string(m.ghammazDegree) +
                               " is not the root of any jins");

        for (const auto& v : m.ascendingVariants)
            checkPlacements(lib, m.id + "/" + v.id, applyChanges(m.ascending, v.changes),
                            v.ajnas ? *v.ajnas : m.ajnas, tol, problems);

        if (m.descending)
            checkPlacements(lib, m.id + "/descending",
                            applyChanges(m.ascending, m.descending->changes),
                            m.descending->ajnas ? *m.descending->ajnas : m.ajnas, tol, problems);
    }
    return problems;
}

} // namespace maqam
