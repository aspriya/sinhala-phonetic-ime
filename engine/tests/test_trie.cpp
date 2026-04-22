#include <gtest/gtest.h>

#include "sinhala_ime/trie.h"

using namespace sinhala_ime;

TEST(Trie, InsertAndFindExact) {
    Trie<int> t;
    t.insert(U"abc", 42);
    t.insert(U"ab", 7);
    EXPECT_EQ(t.find(U"ab").value_or(-1), 7);
    EXPECT_EQ(t.find(U"abc").value_or(-1), 42);
    EXPECT_FALSE(t.find(U"a").has_value());
    EXPECT_FALSE(t.find(U"abcd").has_value());
}

TEST(Trie, LongestPrefixValue) {
    Trie<int> t;
    t.insert(U"a", 1);
    t.insert(U"ab", 2);
    t.insert(U"abc", 3);

    auto m = t.longest_prefix_value(U"abcxyz");
    ASSERT_TRUE(m.value.has_value());
    EXPECT_EQ(*m.value, 3);
    EXPECT_EQ(m.length, 3u);

    m = t.longest_prefix_value(U"abzzz");
    ASSERT_TRUE(m.value.has_value());
    EXPECT_EQ(*m.value, 2);
    EXPECT_EQ(m.length, 2u);

    m = t.longest_prefix_value(U"zzz");
    EXPECT_FALSE(m.value.has_value());
    EXPECT_EQ(m.length, 0u);
}

TEST(Trie, StrictPrefixAndPrefix) {
    Trie<int> t;
    t.insert(U"th", 100);
    t.insert(U"the", 101);
    EXPECT_TRUE(t.is_strict_prefix(U"t"));   // 't' extends to 'th'/'the'
    EXPECT_TRUE(t.is_strict_prefix(U"th"));  // 'th' extends to 'the'
    EXPECT_FALSE(t.is_strict_prefix(U"the")); // nothing extends 'the'
    EXPECT_TRUE(t.is_prefix(U"th"));
    EXPECT_FALSE(t.is_prefix(U"zz"));
}

TEST(Trie, ShiftBitDistinct) {
    Trie<int> t;
    t.insert(std::u32string{U'k'}, 1);
    t.insert(std::u32string{shift_key(U'k')}, 2);
    EXPECT_EQ(t.find(std::u32string{U'k'}).value_or(-1), 1);
    EXPECT_EQ(t.find(std::u32string{shift_key(U'k')}).value_or(-1), 2);
}

TEST(Trie, OverwriteValue) {
    Trie<int> t;
    t.insert(U"x", 1);
    t.insert(U"x", 42);
    EXPECT_EQ(t.find(U"x").value_or(-1), 42);
}

TEST(Trie, EmptyStringMissing) {
    Trie<int> t;
    EXPECT_FALSE(t.find(U"").has_value());
}
