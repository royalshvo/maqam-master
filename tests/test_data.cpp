#include <catch2/catch_test_macros.hpp>

#include "core/MaqamData.h"
#include "core/Tuning.h"

using namespace maqam;

namespace {

const MaqamLibrary& library() {
    static const MaqamLibrary lib = loadLibraryFromFile(MAQAM_DATA_FILE);
    return lib;
}

// A minimal valid file; tests patch it to provoke specific errors.
std::string minimalJson(const std::string& maqamExtra = "", const std::string& rootExtra = "") {
    return R"({
      "version": 2,
      "ajnas": { "ajam": { "name": "Ajam", "cents": [0, 200, 400, 500] } },
      "maqamat": [{
        "id": "ajam", "name": "Ajam",
        "reference_tonic": { "note": "B-flat", "cents_above_c": 1000 },
        "octave_repeating": true,
        "ascending": [0, 200, 400, 500, 700, 900, 1100, 1200],
        "ajnas": [ { "jins": "ajam", "root_degree": 1 }, { "jins": "ajam", "root_degree": 5 } ],
        "descending": null,
        "ghammaz_degree": 5)" + maqamExtra + R"(
      }])" + rootExtra + "}";
}

std::string replaced(std::string s, const std::string& from, const std::string& to) {
    const auto pos = s.find(from);
    REQUIRE(pos != std::string::npos);
    return s.replace(pos, from.size(), to);
}

} // namespace

TEST_CASE("maqams.json loads") {
    const auto& lib = library();
    CHECK(lib.version == 2);
    CHECK(lib.maqamat.size() == 8);
    CHECK(lib.ajnas.size() == 8);
    for (const char* id : {"rast", "bayati", "hijaz", "saba", "nahawand", "kurd", "sikah", "ajam"})
        CHECK(lib.findMaqam(id) != nullptr);
    CHECK(lib.findPreset("24-tet") != nullptr);
}

TEST_CASE("every maqam form matches its ajnas") {
    const auto problems = checkConsistency(library());
    for (const auto& p : problems)
        UNSCOPED_INFO(p);
    CHECK(problems.empty());
}

TEST_CASE("draft data is not marked as reviewed") {
    // Guards CLAUDE.md: data marked "review": "needed" must not be treated as verified.
    for (const auto& m : library().maqamat)
        CHECK_FALSE(m.reviewed);
}

TEST_CASE("note roles in the data file match the core enum") {
    std::vector<std::string> expected;
    for (auto r : allNoteRoles())
        expected.emplace_back(toString(r));
    CHECK(library().noteRoles == expected);
}

TEST_CASE("structure of specific maqamat") {
    const auto& lib = library();

    const Maqam* saba = lib.findMaqam("saba");
    REQUIRE(saba != nullptr);
    CHECK_FALSE(saba->octaveRepeating);
    CHECK(saba->ascending.back() == 1100.0);
    CHECK(saba->ghammazDegree == 3);

    const Maqam* rast = lib.findMaqam("rast");
    REQUIRE(rast != nullptr);
    REQUIRE(rast->descending.has_value());
    CHECK(rast->descending->changes.at(7) == 1000.0);

    const Maqam* bayati = lib.findMaqam("bayati");
    REQUIRE(bayati != nullptr);
    CHECK(bayati->ghammazDegree == 4);
    CHECK_FALSE(bayati->descending.has_value());

    const Maqam* hijaz = lib.findMaqam("hijaz");
    REQUIRE(hijaz != nullptr);
    CHECK(hijaz->findVariant("rast_upper") != nullptr);
}

TEST_CASE("minimal file parses") {
    const auto lib = parseLibrary(minimalJson());
    REQUIRE(lib.maqamat.size() == 1);
    CHECK(checkConsistency(lib).empty());
}

TEST_CASE("loader rejects malformed data") {
    CHECK_THROWS_AS(parseLibrary("{ not json"), DataError);
    CHECK_THROWS_AS(parseLibrary(R"({"version": 1})"), DataError);
    CHECK_THROWS_AS(loadLibraryFromFile("/nonexistent/maqams.json"), DataError);

    // Unknown jins.
    std::string s = replaced(minimalJson(), R"("jins": "ajam", "root_degree": 5)", R"("jins": "zzz", "root_degree": 5)");
    CHECK_THROWS_AS(parseLibrary(s), DataError);

    // Jins that runs past the end of the scale.
    s = replaced(minimalJson(), R"("root_degree": 5)", R"("root_degree": 6)");
    CHECK_THROWS_AS(parseLibrary(s), DataError);

    // Descending field missing entirely (must be explicit null).
    s = replaced(minimalJson(), R"("descending": null,)", "");
    CHECK_THROWS_AS(parseLibrary(s), DataError);

    // Degree key out of range in a variant.
    CHECK_THROWS_AS(parseLibrary(minimalJson(R"(, "ascending_variants": [{ "id": "x", "changes": { "9": 1000 } }])")),
                    DataError);

    // Non-integer degree key.
    CHECK_THROWS_AS(parseLibrary(minimalJson(R"(, "ascending_variants": [{ "id": "x", "changes": { "7b": 1000 } }])")),
                    DataError);

    // Offsets for an unknown maqam.
    CHECK_THROWS_AS(parseLibrary(minimalJson("", R"(, "tuning_presets": [{ "id": "p", "maqam_offsets": { "nope": { "2": 5 } } }])")),
                    DataError);
}

TEST_CASE("consistency check catches a mislabelled jins") {
    // Upper tetrachord 700-800-1000-1200 is Kurd, not Ajam.
    const std::string s = replaced(minimalJson(), "[0, 200, 400, 500, 700, 900, 1100, 1200]", "[0, 200, 400, 500, 700, 800, 1000, 1200]");
    const auto problems = checkConsistency(parseLibrary(s));
    CHECK(problems.size() == 1);
}

TEST_CASE("consistency check catches a ghammaz that is not a jins root") {
    std::string s = replaced(minimalJson(), R"("ghammaz_degree": 5)", R"("ghammaz_degree": 4)");
    CHECK(checkConsistency(parseLibrary(s)).size() == 1);
}
