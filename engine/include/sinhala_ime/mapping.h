// sinhala_ime/mapping.h — parse mapping JSON into tries + special codepoints.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "sinhala_ime/trie.h"

namespace sinhala_ime {

// Payload for the consonant trie. `cp` is the Sinhala codepoint to emit.
struct ConsonantEntry {
    char32_t cp = 0;
};

// Payload for the vowel trie. A vowel may have a standalone form (emitted at
// word start) and/or a matra form (emitted after a consonant). `a` is the
// special "inherent" vowel — `matra` is 0 for it.
struct VowelEntry {
    char32_t standalone = 0;  // codepoint, or 0 if no standalone form
    char32_t matra      = 0;  // codepoint, or 0 if no matra
};

struct Mapping {
    std::string    version;

    // Consonant trie: keys are sequences of char32_t. Single-key consonants
    // with Shift variants are stored under `c` and `shift_key(c)`. Multi-key
    // consonants are stored under their raw ASCII sequence.
    Trie<ConsonantEntry> consonants;

    // Vowel trie: keys are sequences of char32_t (raw ASCII).
    Trie<VowelEntry>     vowels;

    // Special codepoints.
    char32_t virama   = 0;
    char32_t anusvara = 0;
    char32_t visarga  = 0;
    char32_t zwj      = 0;

    // Rule keys. All single ASCII chars.
    char32_t terminator_key  = U'x';
    char32_t conjunct_marker = U'X';
    char32_t anusvara_key    = U'q';
    char32_t visarga_key     = U'Q';
    bool     unmapped_passthrough = true;

    // Load & parse. Throws std::runtime_error on any error.
    static Mapping from_json_string(std::string_view json);
    static Mapping from_file(const std::string& path);
};

}  // namespace sinhala_ime
