#pragma once

// Maqam data model and loader for data/maqams.json.
// UI-free: no JUCE dependency. See the "conventions" block in the JSON file.

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace maqam {

/** Thrown when maqams.json is malformed or internally inconsistent in structure. */
class DataError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/** A jins defined once in the top-level "ajnas" library. Cents are from the jins root. */
struct JinsDef {
    std::string id;
    std::string name;
    std::string nameAr;
    std::vector<double> cents;
    bool reviewed = false;   // true only when "review" is "verified"
};

/** A jins placed on a maqam degree (1-based). */
struct JinsPlacement {
    std::string jinsId;
    int rootDegree = 1;
};

/** Degree changes (1-based degree -> cents) plus an optional replacement ajnas list.
    Used for ascending variants and for the descending form. */
struct ScaleChange {
    std::string id;
    std::string name;
    std::map<int, double> changes;
    std::optional<std::vector<JinsPlacement>> ajnas;
};

struct ReferenceTonic {
    std::string note;
    std::string traditionalName;
    double centsAboveC = 0.0;   // 0..1199
};

struct Maqam {
    std::string id;
    std::string name;
    std::string nameAr;
    ReferenceTonic referenceTonic;
    bool octaveRepeating = true;
    std::vector<double> ascending;              // cents from the tonic, degree 1 first
    std::vector<JinsPlacement> ajnas;
    std::vector<ScaleChange> ascendingVariants;
    std::optional<ScaleChange> descending;      // nullopt: not yet specified, don't show
    int ghammazDegree = 5;
    std::vector<int> characteristicDegrees;
    bool reviewed = false;                      // true only when "review" is "verified"

    int numDegrees() const { return static_cast<int>(ascending.size()); }
    const ScaleChange* findVariant(const std::string& variantId) const;
};

/** Per-maqam, per-degree cent offsets added on top of the data file values. */
struct TuningPreset {
    std::string id;
    std::string name;
    std::string description;
    std::map<std::string, std::map<int, double>> maqamOffsets;
};

struct MaqamLibrary {
    int version = 0;
    std::vector<std::string> noteRoles;
    std::map<std::string, JinsDef> ajnas;
    std::vector<Maqam> maqamat;
    std::vector<TuningPreset> tuningPresets;

    const Maqam* findMaqam(const std::string& id) const;
    const JinsDef* findJins(const std::string& id) const;
    const TuningPreset* findPreset(const std::string& id) const;
};

/** Parses the JSON text of maqams.json. Throws DataError on malformed data. */
MaqamLibrary parseLibrary(const std::string& jsonText);

/** Reads and parses a maqams.json file. Throws DataError. */
MaqamLibrary loadLibraryFromFile(const std::string& path);

/** Musical consistency checks: every jins placement (base, variants, descending) must match
    the scale it sits on, and the ghammaz must be the root of a jins. Returns one message per
    problem; empty means consistent. */
std::vector<std::string> checkConsistency(const MaqamLibrary& library, double toleranceCents = 0.5);

} // namespace maqam
