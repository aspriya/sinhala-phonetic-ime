#include <gtest/gtest.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "sinhala_ime/engine.h"

using namespace sinhala_ime;

namespace {

// Run a string through the engine as if typed: uppercase letters become
// Shift+<lower>, backspace/terminator are positional ASCII. Returns the
// concatenation of all committed output including a final commit().
std::string type(Engine& e, std::string_view s) {
    std::string out;
    for (char c : s) {
        KeyEvent ev;
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalpha(uc) && std::isupper(uc)) {
            ev.codepoint = static_cast<char32_t>(std::tolower(uc));
            ev.mods      = KeyModifier::Shift;
        } else {
            ev.codepoint = uc;
            ev.mods      = KeyModifier::None;
        }
        auto r = e.feed(ev);
        out += r.committed;
    }
    out += e.commit();
    return out;
}

std::unique_ptr<Engine> make_engine() {
    return Engine::from_mapping_file(SINHALA_IME_MAPPING_PATH);
}

}  // namespace

// -----------------------------------------------------------------------------
// Worked examples from the design doc.
// -----------------------------------------------------------------------------
struct WorkedCase {
    std::string_view input;
    std::string_view expected;
};

static constexpr WorkedCase kWorked[] = {
    {"ammaa",           "අම්මා"},
    {"ashanx",          "අශන්"},
    {"kaawx",           "කාව්"},
    {"siri",            "සිරි"},
    {"gedhara",         "ගෙදර"},
    {"magee",           "මගේ"},
    {"ispirithaalaya",  "ඉස්පිරිතාලය"},
    {"sakkaraa",        "සක්කරා"},
    {"nagaraya",        "නගරය"},
    {"laqkaawa",        "ලංකාව"},
};

class WorkedTest : public testing::TestWithParam<WorkedCase> {};

TEST_P(WorkedTest, MatchesExpected) {
    auto e = make_engine();
    EXPECT_EQ(type(*e, GetParam().input), GetParam().expected);
}

INSTANTIATE_TEST_SUITE_P(WorkedExamples, WorkedTest, testing::ValuesIn(kWorked));

// -----------------------------------------------------------------------------
// TC matrix.
// -----------------------------------------------------------------------------
TEST(Engine, TC01_K_Preview) {
    auto e = make_engine();
    KeyEvent ev{'k', KeyModifier::None, false, false};
    auto r = e->feed(ev);
    EXPECT_EQ(r.committed, "");
    EXPECT_EQ(r.composition, "ක");
}

TEST(Engine, TC02_Kx_Virama) {
    auto e = make_engine();
    EXPECT_EQ(type(*e, "kx"), "ක්");
}

TEST(Engine, TC03_Ka) {
    auto e = make_engine();
    EXPECT_EQ(type(*e, "ka"), "ක");
}

TEST(Engine, TC04_Kaa) {
    auto e = make_engine();
    EXPECT_EQ(type(*e, "kaa"), "කා");
}

TEST(Engine, TC05_Ammaa) {
    auto e = make_engine();
    EXPECT_EQ(type(*e, "ammaa"), "අම්මා");
}

TEST(Engine, TC06_Ashanx) {
    auto e = make_engine();
    EXPECT_EQ(type(*e, "ashanx"), "අශන්");
}

TEST(Engine, TC07_ShiftKa) {
    auto e = make_engine();
    // Shift+k then a.
    KeyEvent K{'k', KeyModifier::Shift, false, false};
    auto r1 = e->feed(K);
    EXPECT_EQ(r1.committed, "");
    KeyEvent a{'a', KeyModifier::None, false, false};
    auto r2 = e->feed(a);
    EXPECT_EQ(r2.composition, "ඛ");  // inherent a (no matra)
    EXPECT_EQ(e->commit(), "ඛ");
}

TEST(Engine, TC08_BackspaceUndoesMatraPrefix) {
    auto e = make_engine();
    e->feed({'k', KeyModifier::None, false, false});
    e->feed({'a', KeyModifier::None, false, false});
    KeyEvent bs{0, KeyModifier::None, true, false};
    e->feed(bs);
    EXPECT_EQ(e->composition(), "ක");
}

TEST(Engine, TC09_DoubleBackspaceResets) {
    auto e = make_engine();
    e->feed({'k', KeyModifier::None, false, false});
    e->feed({'a', KeyModifier::None, false, false});
    e->feed({0,   KeyModifier::None, true,  false});
    e->feed({0,   KeyModifier::None, true,  false});
    EXPECT_EQ(e->composition(), "");
}

TEST(Engine, TC10_Tha) {
    auto e = make_engine();
    EXPECT_EQ(type(*e, "tha"), "ත");
}

TEST(Engine, TC11_Tx) {
    auto e = make_engine();
    EXPECT_EQ(type(*e, "tx"), "ට්");
}

TEST(Engine, TC12_Laqkaawa) {
    auto e = make_engine();
    EXPECT_EQ(type(*e, "laqkaawa"), "ලංකාව");
}

TEST(Engine, TC13_StressNoCrash) {
    auto e = make_engine();
    std::mt19937 rng(42);
    std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ ^0123456789.,?";
    std::string blob;
    blob.reserve(500);
    std::uniform_int_distribution<std::size_t> dist(0, charset.size() - 1);
    for (int i = 0; i < 500; ++i) blob.push_back(charset[dist(rng)]);
    EXPECT_NO_THROW({ (void)type(*e, blob); });
}

TEST(Engine, TC14_EmptyCommit) {
    auto e = make_engine();
    EXPECT_EQ(e->commit(), "");
}

TEST(Engine, TC15_Reset) {
    auto e = make_engine();
    e->feed({'k', KeyModifier::None, false, false});
    e->feed({'a', KeyModifier::None, false, false});
    e->reset();
    EXPECT_EQ(e->composition(), "");
    EXPECT_EQ(e->commit(), "");
}

TEST(Engine, FromMappingJsonSmoke) {
    const char* tiny = R"({
        "version":"0.1",
        "consonants":{"k":{"base":"\u0D9A","shift":"\u0D9B"}},
        "vowels_standalone":{"a":"\u0D85"},
        "vowel_signs":{"a":null,"aa":"\u0DCF"},
        "special":{"virama":"\u0DCA","anusvara":"\u0D82","visarga":"\u0D83","zwj":"\u200D"},
        "rules":{"terminator_key":"x","conjunct_marker":"X","anusvara_key":"q","visarga_key":"Q","unmapped_passthrough":true}
    })";
    auto e = Engine::from_mapping_json(tiny);
    EXPECT_EQ(type(*e, "ka"), "ක");
}

// -----------------------------------------------------------------------------
// Cross-validation against the Python-generated golden file.
// -----------------------------------------------------------------------------
namespace {

struct GoldenRow { std::string input; std::string expected; };

std::vector<GoldenRow> load_golden(const std::string& path) {
    std::vector<GoldenRow> rows;
    std::ifstream in(path);
    if (!in) return rows;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        rows.push_back({line.substr(0, tab), line.substr(tab + 1)});
    }
    return rows;
}

}  // namespace

TEST(Engine, GoldenFileCrossValidation) {
    auto rows = load_golden(SINHALA_IME_GOLDEN_PATH);
    ASSERT_FALSE(rows.empty()) << "golden.txt not found at " SINHALA_IME_GOLDEN_PATH;

    for (const auto& row : rows) {
        auto e   = make_engine();
        auto got = type(*e, row.input);
        EXPECT_EQ(got, row.expected)
            << "input=" << row.input
            << " got_hex=" << [&]{
                std::ostringstream os;
                for (unsigned char c : got) os << std::hex << (int)c << ' ';
                return os.str();
            }();
    }
}
