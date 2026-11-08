#include <catch2/catch_test_macros.hpp>

#include "core/MaqamData.h"
#include "core/NoteNames.h"
#include "core/Tuning.h"

using namespace maqam;

namespace {

const MaqamLibrary& library() {
    static const MaqamLibrary lib = loadLibraryFromFile(MAQAM_DATA_FILE);
    return lib;
}

ResolvedScale scaleOf(const char* id, Direction dir = Direction::Ascending) {
    const Maqam* m = library().findMaqam(id);
    REQUIRE(m != nullptr);
    auto s = resolveScale(*m, dir);
    REQUIRE(s.has_value());
    return *s;
}

std::vector<std::string> words(const std::vector<NoteName>& names) {
    std::vector<std::string> out;
    for (const auto& n : names)
        out.push_back(n.words());
    return out;
}

using Names = std::vector<std::string>;

} // namespace

TEST_CASE("tonic spelling") {
    CHECK(spellTonic(Tonic{}).words(true) == "F 4");
    CHECK(spellTonic(Tonic{60, 0}).words(true) == "C 4");
    CHECK(spellTonic(Tonic{70, 0}).words() == "B flat");
    CHECK(spellTonic(Tonic{66, 0}).words() == "G flat");
    CHECK(spellTonic(Tonic::fromCentsAboveC(350)).words(true) == "E half-flat 4");
    CHECK(spellTonic(Tonic::fromCentsAboveC(150)).words() == "D half-flat");
    CHECK(spellTonic(Tonic::fromCentsAboveC(1050)).words() == "B half-flat");
    CHECK(spellTonic(Tonic::fromCentsAboveC(450)).words() == "E half-sharp");
    CHECK(spellTonic(Tonic::fromCentsAboveC(1150)).words() == "B half-sharp");
    CHECK(spellTonic(Tonic::fromCentsAboveC(550)).words() == "F half-sharp");
    CHECK(spellTonic(Tonic::fromCentsAboveC(50)).words() == "C half-sharp");
}

TEST_CASE("every quarter-tone pitch class has a spelling that sounds at that pitch") {
    for (int q = 0; q < 24; ++q) {
        const Tonic t = Tonic::fromCentsAboveC(q * 50.0);
        const NoteName n = spellTonic(t);
        // Rebuild the pitch from the spelling.
        const int naturals[7] = {0, 200, 400, 500, 700, 900, 1100};
        const char* letters = "CDEFGAB";
        int idx = 0;
        while (letters[idx] != n.letter)
            ++idx;
        const double rebuilt = (n.octave + 1) * 1200.0 + naturals[idx] + n.quarterTones * 50.0;
        INFO("q=" << q << " " << n.words(true));
        CHECK(rebuilt == t.absoluteCents());
        CHECK(std::abs(n.quarterTones) <= 2);
    }
}

TEST_CASE("Rast spelled on F and on C") {
    CHECK(words(spellScale(scaleOf("rast"), Tonic{})) ==
          Names{"F", "G", "A half-flat", "B flat", "C", "D", "E half-flat", "F"});
    CHECK(words(spellScale(scaleOf("rast"), Tonic{60, 0})) ==
          Names{"C", "D", "E half-flat", "F", "G", "A", "B half-flat", "C"});
    CHECK(words(spellScale(scaleOf("rast", Direction::Descending), Tonic{60, 0}))[6] == "B flat");
}

TEST_CASE("octaves follow the letters") {
    const auto names = spellScale(scaleOf("rast"), Tonic{});
    CHECK(names[0].octave == 4);   // F4
    CHECK(names[3].octave == 4);   // B-flat 4
    CHECK(names[4].octave == 5);   // C5
    CHECK(names[7].octave == 5);   // F5
}

TEST_CASE("maqamat on their reference tonics") {
    const auto& lib = library();
    auto onReference = [&](const char* id) {
        const Maqam* m = lib.findMaqam(id);
        return words(spellScale(scaleOf(id), Tonic::fromCentsAboveC(m->referenceTonic.centsAboveC)));
    };
    CHECK(onReference("bayati") == Names{"D", "E half-flat", "F", "G", "A", "B flat", "C", "D"});
    CHECK(onReference("hijaz") == Names{"D", "E flat", "F sharp", "G", "A", "B flat", "C", "D"});
    CHECK(onReference("saba") == Names{"D", "E half-flat", "F", "G flat", "A", "B flat", "C", "D flat"});
    CHECK(onReference("sikah") ==
          Names{"E half-flat", "F", "G", "A", "B half-flat", "C", "D", "E half-flat"});
    CHECK(onReference("ajam") == Names{"B flat", "C", "D", "E flat", "F", "G", "A", "B flat"});
    CHECK(onReference("kurd") == Names{"D", "E flat", "F", "G", "A", "B flat", "C", "D"});
}

TEST_CASE("transposed spellings stay readable") {
    CHECK(words(spellScale(scaleOf("hijaz"), Tonic{})) ==
          Names{"F", "G flat", "A", "B flat", "C", "D flat", "E flat", "F"});
    CHECK(words(spellScale(scaleOf("sikah"), Tonic{})) ==
          Names{"F", "G half-flat", "A half-flat", "B half-flat", "C", "D half-flat", "E half-flat", "F"});
    CHECK(words(spellScale(scaleOf("bayati"), Tonic{67, 0})) ==
          Names{"G", "A half-flat", "B flat", "C", "D", "E flat", "F", "G"});
}

TEST_CASE("symbols and preset remainders") {
    const auto names = spellScale(scaleOf("rast"), Tonic{});
    CHECK(names[2].symbols() == "A\xF0\x9D\x84\xB3");
    CHECK(names[3].symbols(true) == "B\xE2\x99\xAD" "4");

    ResolvedScale lowered = scaleOf("rast");
    lowered.cents[2] -= 10.0;
    CHECK(spellScale(lowered, Tonic{})[2].words() == "A half-flat (-10 cents)");
}

TEST_CASE("12-TET key spelling prefers flats") {
    CHECK(spellKey(61).words(true) == "D flat 4");
    CHECK(spellKey(69).words(true) == "A 4");
    CHECK(spellKey(0).words(true) == "C -1");
}

TEST_CASE("compact names") {
    const auto names = spellScale(scaleOf("rast"), Tonic{});
    CHECK(names[2].compact() == "A\xC2\xBD\xE2\x99\xAD");          // A½♭
    CHECK(names[3].compact(true) == "B\xE2\x99\xAD" "4");          // B♭4
    CHECK(names[0].compact() == "F");
    CHECK(spellTonic(Tonic::fromCentsAboveC(450)).compact() == "E\xC2\xBD\xE2\x99\xAF");  // E½♯

    ResolvedScale lowered = scaleOf("rast");
    lowered.cents[2] -= 10.0;
    const NoteName n = spellScale(lowered, Tonic{})[2];
    CHECK(n.compact() == "A\xC2\xBD\xE2\x99\xAD (-10 cents)");
    CHECK(n.compact(false, false) == "A\xC2\xBD\xE2\x99\xAD");
}

TEST_CASE("double accidentals are respelled") {
    // Saba on F: degree 4 is 400 cents up, which by letter would be B double-flat.
    CHECK(words(spellScale(scaleOf("saba"), Tonic{})) ==
          Names{"F", "G half-flat", "A flat", "A", "C", "D flat", "E flat", "F flat"});
    for (const auto& m : library().maqamat) {
        for (int midi = 55; midi < 67; ++midi) {
            for (const auto& n : spellScale(*resolveScale(m, Direction::Ascending), Tonic{midi, 0})) {
                INFO(m.id << " on MIDI " << midi << ": " << n.words());
                CHECK(std::abs(n.quarterTones) <= 2);
            }
        }
    }
}
