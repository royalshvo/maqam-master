#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/MaqamData.h"
#include "core/Tuning.h"

using namespace maqam;
using Catch::Approx;

namespace {

const MaqamLibrary& library() {
    static const MaqamLibrary lib = loadLibraryFromFile(MAQAM_DATA_FILE);
    return lib;
}

const Maqam& get(const char* id) {
    const Maqam* m = library().findMaqam(id);
    REQUIRE(m != nullptr);
    return *m;
}

ResolvedScale ascending(const Maqam& m, const std::string& variant = {}) {
    auto s = resolveScale(m, Direction::Ascending, variant);
    REQUIRE(s.has_value());
    return *s;
}

} // namespace

TEST_CASE("basic conversions") {
    CHECK(centsToRatio(0) == Approx(1.0));
    CHECK(centsToRatio(1200) == Approx(2.0));
    CHECK(centsToRatio(700) == Approx(1.4983070768766815));
    CHECK(midiNoteToHz(69) == Approx(440.0));
    CHECK(midiNoteToHz(60) == Approx(261.6255653005986));
    CHECK(midiNoteToHz(69, 442.0) == Approx(442.0));
}

TEST_CASE("default tonic is F4") {
    const Tonic t;
    CHECK(t.midiNote == 65);
    CHECK(frequencyHz(t, 0) == Approx(349.2282314330039));
}

TEST_CASE("tonic from a pitch class in cents above C") {
    const Tonic sikah = Tonic::fromCentsAboveC(350);   // E half-flat 4
    CHECK(sikah.midiNote == 64);
    CHECK(sikah.centsOffset == Approx(-50.0));
    const Tonic ajam = Tonic::fromCentsAboveC(1000);   // B-flat 4
    CHECK(ajam.midiNote == 70);
    CHECK(ajam.centsOffset == Approx(0.0).margin(1e-9));
    CHECK(Tonic::fromCentsAboveC(500).midiNote == 65); // F4
    CHECK(Tonic::fromCentsAboveC(0, 3).midiNote == 48);
}

TEST_CASE("M1: Hijaz on F has the right frequencies") {
    // F Gb A Bb C Db Eb F, all on the 12-TET grid.
    const ResolvedScale s = ascending(get("hijaz"));
    const Tonic f4;
    const double expected[] = {349.2282, 369.9944, 440.0000, 466.1638,
                               523.2511, 554.3653, 622.2540, 698.4565};
    for (int d = 1; d <= 8; ++d)
        CHECK(degreeFrequencyHz(s, f4, d) == Approx(expected[d - 1]).epsilon(1e-6));
    CHECK(degreeFrequencyHz(s, f4, 3, -1) == Approx(220.0));
    CHECK_THROWS(degreeFrequencyHz(s, f4, 9));
}

TEST_CASE("Rast on F: the half-flat third and seventh") {
    const ResolvedScale s = ascending(get("rast"));
    const Tonic f4;
    // Degree 3 = A half-flat, a quarter tone (50 cents) below A4.
    CHECK(degreeFrequencyHz(s, f4, 3) == Approx(440.0 * centsToRatio(-50)));
    CHECK(isOffTwelveTetGrid(f4, s.cents[2]));
    CHECK(isOffTwelveTetGrid(f4, s.cents[6]));
    CHECK_FALSE(isOffTwelveTetGrid(f4, s.cents[4]));
}

TEST_CASE("A4 reference is configurable") {
    const ResolvedScale s = ascending(get("hijaz"));
    CHECK(degreeFrequencyHz(s, Tonic{}, 3, 0, 432.0) == Approx(432.0));
}

TEST_CASE("quarter-tone status depends on the tonic") {
    const ResolvedScale s = ascending(get("sikah"));
    // On its reference tonic E half-flat: degrees 1 and 5 are half-flat, degree 2 (F) is not.
    const Tonic eHalfFlat = Tonic::fromCentsAboveC(350);
    CHECK(isOffTwelveTetGrid(eHalfFlat, s.cents[0]));
    CHECK_FALSE(isOffTwelveTetGrid(eHalfFlat, s.cents[1]));
    CHECK(isOffTwelveTetGrid(eHalfFlat, s.cents[4]));
    // On F the tonic is on the grid, and degree 2 (150 cents) is not.
    const Tonic f4;
    CHECK_FALSE(isOffTwelveTetGrid(f4, s.cents[0]));
    CHECK(isOffTwelveTetGrid(f4, s.cents[1]));
}

TEST_CASE("descending forms") {
    auto rastDown = resolveScale(get("rast"), Direction::Descending);
    REQUIRE(rastDown.has_value());
    CHECK(rastDown->cents[6] == 1000.0);
    CHECK(rastDown->ajnas[1].jinsId == "nahawand");

    // Not specified in the data: no descending form to show.
    CHECK_FALSE(resolveScale(get("bayati"), Direction::Descending).has_value());

    CHECK_THROWS_AS(resolveScale(get("hijaz"), Direction::Descending, "rast_upper"), std::invalid_argument);
}

TEST_CASE("ascending variants") {
    const ResolvedScale harmonic = ascending(get("nahawand"), "harmonic");
    CHECK(harmonic.cents[6] == 1100.0);
    CHECK(harmonic.ajnas[1].jinsId == "hijaz");

    const ResolvedScale rastUpper = ascending(get("hijaz"), "rast_upper");
    CHECK(rastUpper.cents[5] == 850.0);

    CHECK_THROWS_AS(resolveScale(get("hijaz"), Direction::Ascending, "nope"), std::invalid_argument);
}

TEST_CASE("tuning preset offsets are applied") {
    const auto lib = parseLibrary(R"({
      "version": 2,
      "ajnas": { "rast": { "name": "Rast", "cents": [0, 200, 350, 500] } },
      "maqamat": [{
        "id": "rast", "name": "Rast",
        "reference_tonic": { "note": "C", "cents_above_c": 0 },
        "octave_repeating": true,
        "ascending": [0, 200, 350, 500, 700, 900, 1050, 1200],
        "ajnas": [ { "jins": "rast", "root_degree": 1 }, { "jins": "rast", "root_degree": 5 } ],
        "descending": null,
        "ghammaz_degree": 5
      }],
      "tuning_presets": [{ "id": "arab", "maqam_offsets": { "rast": { "3": -10 } } }]
    })");
    const auto s = resolveScale(lib.maqamat[0], Direction::Ascending, {}, lib.findPreset("arab"));
    REQUIRE(s.has_value());
    CHECK(s->cents[2] == Approx(340.0));
    CHECK(s->cents[3] == Approx(500.0));
}

TEST_CASE("note roles") {
    const Maqam& rast = get("rast");
    const ResolvedScale s = ascending(rast);
    const Tonic f4;
    CHECK(primaryRole(rast, s, f4, 1) == NoteRole::Tonic);
    CHECK(primaryRole(rast, s, f4, 8) == NoteRole::Tonic);
    CHECK(primaryRole(rast, s, f4, 5) == NoteRole::Ghammaz);
    CHECK(primaryRole(rast, s, f4, 2) == NoteRole::Passing);

    // Degree 3 of Rast is both characteristic and (on F) a quarter tone.
    const auto roles3 = rolesForDegree(rast, s, f4, 3);
    CHECK(roles3 == std::vector<NoteRole>{NoteRole::Characteristic, NoteRole::QuarterTone});

    // Saba does not repeat at the octave: degree 8 (D-flat) is not the tonic.
    const Maqam& saba = get("saba");
    const ResolvedScale sabaScale = ascending(saba);
    CHECK(primaryRole(saba, sabaScale, f4, 8) == NoteRole::Passing);
    CHECK(primaryRole(saba, sabaScale, f4, 3) == NoteRole::Ghammaz);

    CHECK_THROWS(rolesForDegree(rast, s, f4, 0));
}
