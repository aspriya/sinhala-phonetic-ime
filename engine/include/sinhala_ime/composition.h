// sinhala_ime/composition.h — state machine over a loaded Mapping.
//
// Mirrors, byte-for-byte, the Python reference implementation in
// prototype/transliterate.py. See docs/phonetic-mapping-design.md for the
// canonical transition table.
#pragma once

#include <string>

#include "sinhala_ime/mapping.h"

namespace sinhala_ime {

enum class CompositionState : std::uint8_t {
    Idle = 0,
    PartialConsonant,
    ConsonantPending,
    PartialVowel,
    PartialVowelStandalone,
};

struct FeedOutcome {
    std::string committed;    // UTF-8 text newly committed by this step.
    std::string composition;  // UTF-8 preview of the current composition.
    bool        consumed = false;
};

class Composer {
public:
    explicit Composer(const Mapping& mapping) noexcept;

    // Feed one symbol. `c` is either a real ASCII char32_t or a Shift-modified
    // key (via shift_key()). Backspace is signaled with `is_backspace=true`.
    FeedOutcome feed(char32_t c, bool is_backspace);

    // Force-commit the buffered composition. Returns the committed UTF-8.
    std::string commit();

    // Drop the buffered composition silently.
    void reset() noexcept;

    // UTF-8 preview of the current composition (without implicit virama).
    std::string composition() const;

    CompositionState state() const noexcept { return state_; }

private:
    // Commit the currently-buffered consonant (if any) with virama, unless
    // `with_virama` is false.
    void flush_consonant_with_virama(std::string& out);
    // Emit the currently-buffered consonant without virama (soft terminator).
    void flush_consonant_plain(std::string& out);
    // Emit the standalone form of the currently-buffered vowel prefix.
    void flush_standalone_vowel(std::string& out);
    // Emit consonant + matra for the currently-buffered consonant + vowel prefix.
    void flush_consonant_matra(std::string& out);

    // Append a codepoint as UTF-8 to `out`.
    static void append_utf8(std::string& out, char32_t cp);

    const Mapping&   mapping_;
    CompositionState state_ = CompositionState::Idle;

    // Buffered consonant codepoint (valid when state ∈ {ConsonantPending, PartialVowel}).
    char32_t pending_consonant_ = 0;

    // Buffered ASCII prefix for in-progress consonant or vowel lookup.
    std::u32string prefix_;
};

}  // namespace sinhala_ime
