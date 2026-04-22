#include <gtest/gtest.h>

#include <fstream>
#include <stdexcept>
#include <string>

#include "sinhala_ime/mapping.h"

using namespace sinhala_ime;

static std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    EXPECT_TRUE(in.good()) << "cannot open " << path;
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

TEST(Mapping, LoadsFromFile) {
    auto m = Mapping::from_file(SINHALA_IME_MAPPING_PATH);
    EXPECT_FALSE(m.version.empty());
    EXPECT_EQ(m.virama,   U'\u0DCA');
    EXPECT_EQ(m.anusvara, U'\u0D82');
    EXPECT_EQ(m.visarga,  U'\u0D83');
    EXPECT_EQ(m.zwj,      U'\u200D');
}

TEST(Mapping, ConsonantBaseAndShift) {
    auto m = Mapping::from_file(SINHALA_IME_MAPPING_PATH);
    auto k  = m.consonants.find(std::u32string{U'k'});
    ASSERT_TRUE(k.has_value());
    EXPECT_EQ(k->cp, U'\u0D9A');
    auto K  = m.consonants.find(std::u32string{shift_key(U'k')});
    ASSERT_TRUE(K.has_value());
    EXPECT_EQ(K->cp, U'\u0D9B');
}

TEST(Mapping, MultiKeyConsonants) {
    auto m = Mapping::from_file(SINHALA_IME_MAPPING_PATH);
    auto th = m.consonants.find(std::u32string{U't', U'h'});
    ASSERT_TRUE(th.has_value());
    EXPECT_EQ(th->cp, U'\u0DAD');
    auto Th = m.consonants.find(std::u32string{shift_key(U't'), U'h'});
    ASSERT_TRUE(Th.has_value());
    EXPECT_EQ(Th->cp, U'\u0DAE');
}

TEST(Mapping, VowelsHaveBothForms) {
    auto m = Mapping::from_file(SINHALA_IME_MAPPING_PATH);
    auto aa = m.vowels.find(std::u32string{U'a', U'a'});
    ASSERT_TRUE(aa.has_value());
    EXPECT_EQ(aa->standalone, U'\u0D86');
    EXPECT_EQ(aa->matra,      U'\u0DCF');
}

TEST(Mapping, RuleKeysUseShiftForUppercase) {
    auto m = Mapping::from_file(SINHALA_IME_MAPPING_PATH);
    EXPECT_EQ(m.terminator_key,  static_cast<char32_t>(U'x'));
    EXPECT_EQ(m.conjunct_marker, shift_key(U'x'));
    EXPECT_EQ(m.anusvara_key,    static_cast<char32_t>(U'q'));
    EXPECT_EQ(m.visarga_key,     shift_key(U'q'));
}

TEST(Mapping, RejectsBadJson) {
    EXPECT_THROW(Mapping::from_json_string("{ not json }"), std::runtime_error);
}

TEST(Mapping, RejectsMissingFile) {
    EXPECT_THROW(Mapping::from_file("/nonexistent/path/to/mapping.json"),
                 std::runtime_error);
}

TEST(Mapping, RejectsBadConsonantKeyLength) {
    const char* bad = R"({"version":"1.0","consonants":{"kk":{"base":"\u0D9A"}}})";
    EXPECT_THROW(Mapping::from_json_string(bad), std::runtime_error);
}

TEST(Mapping, AcceptsMinimalJson) {
    const char* tiny = R"({
        "version":"0.1",
        "special":{"virama":"\u0DCA","anusvara":"\u0D82","visarga":"\u0D83","zwj":"\u200D"},
        "rules":{"terminator_key":"x","conjunct_marker":"X","anusvara_key":"q","visarga_key":"Q"}
    })";
    auto m = Mapping::from_json_string(tiny);
    EXPECT_EQ(m.version, "0.1");
    EXPECT_FALSE(m.consonants.find(std::u32string{U'k'}).has_value());
}

TEST(Mapping, RealFileMatchesSlurp) {
    auto from_file  = Mapping::from_file(SINHALA_IME_MAPPING_PATH);
    auto from_json  = Mapping::from_json_string(slurp(SINHALA_IME_MAPPING_PATH));
    EXPECT_EQ(from_file.version, from_json.version);
}
