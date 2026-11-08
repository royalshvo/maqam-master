#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "core/KeyMapping.h"
#include "core/MaqamData.h"
#include "core/Tuning.h"

using namespace maqam;
using Catch::Approx;

namespace {

const MaqamLibrary& library() {
    static const MaqamLibrary lib = loadLibraryFromFile(MAQAM_DATA_FILE);
    return lib;
}

ResolvedScale scaleOf(const char* id, Direction dir = Direction::Ascending, const std::string& variant = {}) {
    const Maqam* m = library().findMaqam(id);
    REQUIRE(m != nullptr);
    auto s = resolveScale(*m, dir, variant);
    REQUIRE(s.has_value());
    return *s;
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

} // namespace

TEST_CASE("12-key layout: Rast on F") {
    const TuningTable t = makeTwelveKeyTable(scaleOf("rast"), Tonic{});
    REQUIRE(t.stepsPerPeriod() == 12);

    CHECK(keyFrequencyHz(t, 65) == Approx(349.2282));                       // F4 tonic
    CHECK(keyFrequencyHz(t, 69) == Approx(440.0 * centsToRatio(-50)));      // A key = A half-flat
    CHECK(keyFrequencyHz(t, 76) == Approx(midiNoteToHz(76) * centsToRatio(-50)));  // E key = E half-flat
    CHECK(keyFrequencyHz(t, 70) == Approx(midiNoteToHz(70)));               // B-flat, in the maqam
    CHECK(keyFrequencyHz(t, 66) == Approx(midiNoteToHz(66)));               // G-flat, outside: 12-TET
    CHECK(keyFrequencyHz(t, 77) == Approx(698.4565));                       // F5
    CHECK(keyFrequencyHz(t, 53) == Approx(174.6141));                       // F3
    CHECK(keyFrequencyHz(t, 57) == Approx(220.0 * centsToRatio(-50)));      // lower octave too

    CHECK(keyDegree(t, 65) == 1);
    CHECK(keyDegree(t, 69) == 3);
    CHECK(keyDegree(t, 72) == 5);
    CHECK(keyDegree(t, 76) == 7);
    CHECK(keyDegree(t, 66) == 0);
    CHECK(keyDegree(t, 77) == 1);
}

TEST_CASE("12-key layout: Rast descending puts B-flat on the E-flat key") {
    const TuningTable t = makeTwelveKeyTable(scaleOf("rast", Direction::Descending), Tonic{});
    CHECK(keyFrequencyHz(t, 75) == Approx(midiNoteToHz(75)));   // E-flat 5 = degree 7 (1000 cents)
    CHECK(keyDegree(t, 75) == 7);
    CHECK(keyDegree(t, 76) == 0);
}

TEST_CASE("12-key layout: Sikah on its reference tonic E half-flat") {
    const Tonic eHalfFlat = Tonic::fromCentsAboveC(350);
    const TuningTable t = makeTwelveKeyTable(scaleOf("sikah"), eHalfFlat);
    CHECK(t.tonicMidiNote == 64);
    CHECK(keyFrequencyHz(t, 64) == Approx(midiNoteToHz(63.5)));   // E key plays E half-flat
    CHECK(keyFrequencyHz(t, 65) == Approx(midiNoteToHz(65)));     // F
    CHECK(keyFrequencyHz(t, 67) == Approx(midiNoteToHz(67)));     // G
    CHECK(keyFrequencyHz(t, 71) == Approx(midiNoteToHz(70.5)));   // B key plays B half-flat
    CHECK(keyFrequencyHz(t, 63) == Approx(midiNoteToHz(63)));     // E-flat key, outside: 12-TET
    CHECK(keyDegree(t, 71) == 5);
}

TEST_CASE("12-key layout: Saba keeps D-flat as its own note") {
    // Saba on D: D E-half-flat F G-flat A B-flat C D-flat
    const Tonic d4{62, 0.0};
    const TuningTable t = makeTwelveKeyTable(scaleOf("saba"), d4);
    CHECK(keyDegree(t, 66) == 4);   // G-flat on the F-sharp key
    CHECK(keyDegree(t, 73) == 8);   // D-flat on the C-sharp key
    CHECK(keyDegree(t, 74) == 1);   // D an octave up is the tonic key
    CHECK(keyFrequencyHz(t, 64) == Approx(midiNoteToHz(63.5)));
}

TEST_CASE("12-key layout: a collision moves to the free neighbouring key") {
    ResolvedScale s;
    s.cents = {0, 350, 400, 700, 1200};
    s.octaveRepeating = true;
    const TuningTable t = makeTwelveKeyTable(s, Tonic{60, 0.0});
    CHECK(keyDegree(t, 64) == 2);   // 350 takes the upper key first
    CHECK(keyDegree(t, 65) == 3);   // 400 (exactly on E) then falls back to the next key up
    CHECK(keyFrequencyHz(t, 65) == Approx(midiNoteToHz(64)));

    ResolvedScale crowded;
    crowded.cents = {0, 350, 375, 400, 425, 1200};
    crowded.octaveRepeating = true;
    CHECK_THROWS(makeTwelveKeyTable(crowded, Tonic{60, 0.0}));
}

TEST_CASE("degrees layout: one degree per key, tonic on its own key") {
    const TuningTable t = makeDegreesTable(scaleOf("rast"), Tonic{});
    REQUIRE(t.stepsPerPeriod() == 7);
    CHECK(keyFrequencyHz(t, 65) == Approx(349.2282));
    CHECK(keyFrequencyHz(t, 66) == Approx(midiNoteToHz(67)));               // G
    CHECK(keyFrequencyHz(t, 67) == Approx(440.0 * centsToRatio(-50)));      // A half-flat
    CHECK(keyFrequencyHz(t, 72) == Approx(698.4565));                       // 7 keys up = octave
    CHECK(keyFrequencyHz(t, 58) == Approx(174.6141));                       // 7 keys down
    CHECK(keyDegree(t, 71) == 7);
    CHECK(keyDegree(t, 72) == 1);
}

TEST_CASE("degrees layout: Saba has 8 steps") {
    const TuningTable t = makeDegreesTable(scaleOf("saba"), Tonic{});
    CHECK(t.stepsPerPeriod() == 8);
    CHECK(keyDegree(t, 72) == 8);
}

TEST_CASE("Scala export") {
    const TuningTable t = makeTwelveKeyTable(scaleOf("rast"), Tonic{});
    const std::string scl = toScl(t, "Rast test");
    CHECK(contains(scl, "Rast test\n 12\n"));
    CHECK(contains(scl, " 350.000\n"));
    CHECK(contains(scl, " 1050.000\n"));
    CHECK(contains(scl, " 1200.000\n"));
    CHECK_FALSE(contains(scl, " 0.000\n"));   // the unison is implied

    const std::string kbm = toKbm(t);
    CHECK(contains(kbm, "(scale degree 0)\n65\n"));
    CHECK(contains(kbm, "note and frequency\n65\n349.228231\n"));
    CHECK(contains(kbm, "formal octave\n12\n"));
}

TEST_CASE("Ableton .ascl export pins the tonic on its key") {
    const TuningTable twelve = makeTwelveKeyTable(scaleOf("rast"), Tonic{});
    const std::string a = toAscl(twelve, "Rast", "Maqam Master");
    // MIDI 65 is F3 in Live's naming; MIDI 0 is 65 keys below = 6 periods down, index 7.
    CHECK(contains(a, "! @ABL REFERENCE_PITCH 3 0 349.228231\n"));
    CHECK(contains(a, "! @ABL NOTE_RANGE_BY_INDEX -3 7\n"));
    CHECK(contains(a, "! @ABL SOURCE Maqam Master\n"));
    // REFERENCE_PITCH must come before NOTE_RANGE_BY_INDEX.
    CHECK(a.find("REFERENCE_PITCH") < a.find("NOTE_RANGE_BY_INDEX"));

    const TuningTable degrees = makeDegreesTable(scaleOf("rast"), Tonic{});
    const std::string d = toAscl(degrees, "Rast", "Maqam Master");
    // 65 = 9 * 7 + 2: MIDI 0 is 10 periods below the tonic, index 5.
    CHECK(contains(d, "\n 7\n"));
    CHECK(contains(d, "! @ABL NOTE_RANGE_BY_INDEX -7 5\n"));
}

TEST_CASE("every maqam form exports in both layouts") {
    for (const auto& m : library().maqamat) {
        std::vector<ResolvedScale> forms{*resolveScale(m, Direction::Ascending)};
        if (auto d = resolveScale(m, Direction::Descending))
            forms.push_back(*d);
        for (const auto& v : m.ascendingVariants)
            forms.push_back(*resolveScale(m, Direction::Ascending, v.id));
        for (const auto& s : forms) {
            for (const Tonic& tonic : {Tonic{}, Tonic::fromCentsAboveC(m.referenceTonic.centsAboveC)}) {
                INFO(m.id << " tonic " << tonic.midiNote);
                const TuningTable twelve = makeTwelveKeyTable(s, tonic);
                // Every maqam degree is on some key, each exactly once.
                std::vector<int> seen(static_cast<size_t>(s.numDegrees()) + 1, 0);
                for (int d : twelve.degreeOfStep)
                    if (d > 0)
                        ++seen[static_cast<size_t>(d)];
                for (int d = 1; d <= s.numDegrees(); ++d) {
                    const bool isOctave = s.octaveRepeating && d == s.numDegrees();
                    CHECK(seen[static_cast<size_t>(d)] == (isOctave ? 0 : 1));
                }
                // Each key plays its maqam note at the right pitch.
                for (int key = tonic.midiNote; key < tonic.midiNote + 12; ++key) {
                    const int d = keyDegree(twelve, key);
                    if (d > 0)
                        CHECK(keyFrequencyHz(twelve, key) ==
                              Approx(degreeFrequencyHz(s, tonic, d)).epsilon(1e-9));
                }
                CHECK_NOTHROW(makeDegreesTable(s, tonic));
            }
        }
    }
}

TEST_CASE("combined 12-key table: Rast has both E half-flat and E-flat on F") {
    const Maqam* rast = library().findMaqam("rast");
    REQUIRE(rast != nullptr);
    const TuningTable t = makeTwelveKeyTable(*rast, Tonic{});

    // Going up: E key plays E half-flat (degree 7). Coming down: E-flat key plays E-flat.
    CHECK(keyDegree(t, 76) == 7);
    CHECK_FALSE(t.alternateOfStep[11]);
    CHECK(keyFrequencyHz(t, 76) == Approx(midiNoteToHz(75.5)));
    CHECK(keyDegree(t, 75) == 7);
    CHECK(t.alternateOfStep[10]);
    CHECK(keyFrequencyHz(t, 75) == Approx(midiNoteToHz(75)));
    CHECK(t.unplacedAlternateDegrees.empty());

    // Everything else is unchanged from the plain ascending table.
    const TuningTable plain = makeTwelveKeyTable(*resolveScale(*rast, Direction::Ascending), Tonic{});
    for (int k = 0; k < 12; ++k)
        if (k != 10)
            CHECK(t.stepCents[static_cast<size_t>(k)] == plain.stepCents[static_cast<size_t>(k)]);
}

TEST_CASE("combined 12-key table: maqamat without a descending form match ascending") {
    const Maqam* bayati = library().findMaqam("bayati");
    REQUIRE(bayati != nullptr);
    const TuningTable t = makeTwelveKeyTable(*bayati, Tonic{});
    const TuningTable plain = makeTwelveKeyTable(*resolveScale(*bayati, Direction::Ascending), Tonic{});
    CHECK(t.stepCents == plain.stepCents);
    CHECK(t.degreeOfStep == plain.degreeOfStep);
    for (bool alt : t.alternateOfStep)
        CHECK_FALSE(alt);
}

TEST_CASE("combined 12-key table works for every maqam and variant") {
    for (const auto& m : library().maqamat) {
        std::vector<std::string> variants{""};
        for (const auto& v : m.ascendingVariants)
            variants.push_back(v.id);
        for (const auto& v : variants) {
            for (const Tonic& tonic : {Tonic{}, Tonic::fromCentsAboveC(m.referenceTonic.centsAboveC)}) {
                INFO(m.id << "/" << v << " tonic " << tonic.midiNote);
                TuningTable t;
                REQUIRE_NOTHROW(t = makeTwelveKeyTable(m, tonic, kDefaultA4Hz, v));
                CHECK(t.unplacedAlternateDegrees.empty());
                CHECK(keyDegree(t, tonic.midiNote) == 1);
            }
        }
    }
}

TEST_CASE("the tonic stays on its key even when tuned +50 cents") {
    ResolvedScale s;
    s.cents = {0, 200, 400, 500, 700, 900, 1100, 1200};
    const Tonic raised{64, 50.0};
    const TuningTable t = makeTwelveKeyTable(s, raised);
    CHECK(keyDegree(t, 64) == 1);
    CHECK(keyFrequencyHz(t, 64) == Approx(midiNoteToHz(64.5)));
}

TEST_CASE(".ascl note names for the combined Rast table") {
    const Maqam* rast = library().findMaqam("rast");
    REQUIRE(rast != nullptr);
    const Tonic f4;
    const TuningTable t = makeTwelveKeyTable(*rast, f4);
    const auto up = resolveScale(*rast, Direction::Ascending);
    const auto down = resolveScale(*rast, Direction::Descending);
    const auto names = stepNoteNames(t, *up, &*down, f4);
    REQUIRE(names.size() == 12);
    // F, G-flat (outside), G, A-flat (outside), A half-flat, B-flat, B (outside), C,
    // D-flat (outside), D, E-flat (descending), E half-flat.
    const std::string hf = "\xC2\xBD\xE2\x99\xAD";   // ½♭
    const std::string fl = "\xE2\x99\xAD";           // ♭
    CHECK(names == std::vector<std::string>{"F", "G" + fl, "G", "A" + fl, "A" + hf, "B" + fl, "B",
                                            "C", "D" + fl, "D", "E" + fl, "E" + hf});

    const std::string a = toAscl(t, "Rast", "Maqam Master", names);
    CHECK(contains(a, "! @ABL NOTE_NAMES \"F\" \"G" + fl + "\""));
    // A wrong count is ignored rather than written.
    CHECK_FALSE(contains(toAscl(t, "Rast", "Maqam Master", {"F"}), "NOTE_NAMES"));
}
