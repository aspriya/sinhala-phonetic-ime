#include <gtest/gtest.h>

#include <string>

#include "sinhala_ime/composition.h"
#include "sinhala_ime/mapping.h"

using namespace sinhala_ime;

namespace {

struct Harness {
    Mapping mapping;
    Composer composer;

    Harness()
        : mapping(Mapping::from_file(SINHALA_IME_MAPPING_PATH)),
          composer(mapping) {}

    std::string feed(char32_t c) {
        return composer.feed(c, /*is_backspace=*/false).committed;
    }
    std::string feed_shift(char32_t letter) {
        return composer.feed(shift_key(letter), /*is_backspace=*/false).committed;
    }
    std::string backspace() {
        return composer.feed(0, /*is_backspace=*/true).committed;
    }
    std::string commit() { return composer.commit(); }
};

std::string run_string(Harness& h, std::string_view s) {
    std::string out;
    for (char c : s) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalpha(uc) && std::isupper(uc)) {
            out += h.feed_shift(std::tolower(uc));
        } else {
            out += h.feed(uc);
        }
    }
    out += h.commit();
    return out;
}

}  // namespace

TEST(Composition, IdleStartsIdle) {
    Harness h;
    EXPECT_EQ(h.composer.state(), CompositionState::Idle);
}

TEST(Composition, PartialConsonantThenExtend) {
    Harness h;
    h.feed('t');
    EXPECT_EQ(h.composer.state(), CompositionState::PartialConsonant);
    h.feed('h');
    EXPECT_EQ(h.composer.state(), CompositionState::ConsonantPending);
}

TEST(Composition, ConsonantPendingToPartialVowelToIdle) {
    Harness h;
    h.feed('k');
    EXPECT_EQ(h.composer.state(), CompositionState::ConsonantPending);
    h.feed('a');
    EXPECT_EQ(h.composer.state(), CompositionState::PartialVowel);
    // feeding a second 'a' should complete the vowel as 'aa' and commit matra.
    std::string committed = h.feed('a');
    EXPECT_EQ(h.composer.state(), CompositionState::Idle);
    EXPECT_EQ(committed, "කා");
}

TEST(Composition, HardTerminatorInsertsVirama) {
    Harness h;
    auto s = run_string(h, "kx");
    EXPECT_EQ(s, "ක්");
}

TEST(Composition, SoftTerminatorOmitsVirama) {
    Harness h;
    auto s = run_string(h, "k ");
    EXPECT_EQ(s, "ක ");
}

TEST(Composition, BackspaceFromPartialVowelReturnsToConsonantPending) {
    Harness h;
    h.feed('k');
    h.feed('a');
    h.backspace();
    EXPECT_EQ(h.composer.state(), CompositionState::ConsonantPending);
    EXPECT_EQ(h.composer.composition(), "ක");
}

TEST(Composition, BackspaceFromConsonantPendingReturnsToIdle) {
    Harness h;
    h.feed('k');
    h.backspace();
    EXPECT_EQ(h.composer.state(), CompositionState::Idle);
    EXPECT_EQ(h.composer.composition(), "");
}

TEST(Composition, ResetDiscards) {
    Harness h;
    h.feed('k'); h.feed('a');
    h.composer.reset();
    EXPECT_EQ(h.composer.state(), CompositionState::Idle);
    EXPECT_EQ(h.composer.composition(), "");
    EXPECT_EQ(h.commit(), "");
}

TEST(Composition, AnusvaraKeyCommitsThenEmits) {
    Harness h;
    auto s = run_string(h, "laqkaawa");
    EXPECT_EQ(s, "ලංකාව");
}

TEST(Composition, VisargaKeyEmits) {
    Harness h;
    auto s = run_string(h, "Q");
    EXPECT_EQ(s, "ඃ");
}

TEST(Composition, ConjunctMarkerEmitsViramaZWJ) {
    Harness h;
    auto s = run_string(h, "kXk");
    EXPECT_EQ(s, "ක්\u200dක");
}

TEST(Composition, UnmappedPassthrough) {
    Harness h;
    EXPECT_EQ(run_string(h, "123"), "123");
}
