#include "sinhala_ime/mapping.h"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace sinhala_ime {
namespace {

using json = nlohmann::json;

// Decode the first codepoint of a UTF-8 string. Returns 0 on empty input.
// Throws std::runtime_error on malformed UTF-8.
char32_t first_codepoint(std::string_view s) {
    if (s.empty()) return 0;
    unsigned char c0 = static_cast<unsigned char>(s[0]);
    if (c0 < 0x80) return c0;
    auto need = [&](std::size_t want) {
        if (s.size() < want) {
            throw std::runtime_error("malformed UTF-8 in mapping JSON");
        }
    };
    if ((c0 & 0xE0) == 0xC0) {
        need(2);
        return ((c0 & 0x1Fu) << 6) | (static_cast<unsigned char>(s[1]) & 0x3Fu);
    }
    if ((c0 & 0xF0) == 0xE0) {
        need(3);
        return ((c0 & 0x0Fu) << 12)
             | ((static_cast<unsigned char>(s[1]) & 0x3Fu) << 6)
             | (static_cast<unsigned char>(s[2]) & 0x3Fu);
    }
    if ((c0 & 0xF8) == 0xF0) {
        need(4);
        return ((c0 & 0x07u) << 18)
             | ((static_cast<unsigned char>(s[1]) & 0x3Fu) << 12)
             | ((static_cast<unsigned char>(s[2]) & 0x3Fu) << 6)
             | (static_cast<unsigned char>(s[3]) & 0x3Fu);
    }
    throw std::runtime_error("invalid UTF-8 leading byte in mapping JSON");
}

// Convert a multi-key string like "Th" or "^g" into a sequence of char32_t,
// applying the Shift-bit convention to an uppercase first letter.
std::u32string multi_key_to_seq(const std::string& key) {
    std::u32string out;
    bool first = true;
    for (char raw : key) {
        unsigned char ch = static_cast<unsigned char>(raw);
        if (first && std::isalpha(ch) && std::isupper(ch)) {
            out.push_back(shift_key(static_cast<char32_t>(std::tolower(ch))));
        } else {
            out.push_back(static_cast<char32_t>(ch));
        }
        first = false;
    }
    return out;
}

// Convert a vowel ASCII key like "aa", "A", "aee", "I" into a sequence,
// mapping uppercase letters to shifted keys.
std::u32string vowel_key_to_seq(const std::string& key) {
    std::u32string out;
    for (char raw : key) {
        unsigned char ch = static_cast<unsigned char>(raw);
        if (std::isalpha(ch) && std::isupper(ch)) {
            out.push_back(shift_key(static_cast<char32_t>(std::tolower(ch))));
        } else {
            out.push_back(static_cast<char32_t>(ch));
        }
    }
    return out;
}

char32_t rule_key_to_cp(const std::string& s) {
    if (s.size() != 1) {
        throw std::runtime_error("rule key must be 1 char: " + s);
    }
    unsigned char ch = static_cast<unsigned char>(s[0]);
    if (std::isalpha(ch) && std::isupper(ch)) {
        return shift_key(static_cast<char32_t>(std::tolower(ch)));
    }
    return static_cast<char32_t>(ch);
}

}  // namespace

Mapping Mapping::from_json_string(std::string_view text) {
    json data;
    try {
        data = json::parse(text);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("mapping JSON parse error: ") + e.what());
    }

    Mapping m;
    if (data.contains("version")) m.version = data["version"].get<std::string>();

    // Single-key consonants with optional Shift variants.
    if (data.contains("consonants")) {
        for (auto it = data["consonants"].begin(); it != data["consonants"].end(); ++it) {
            const std::string& key = it.key();
            if (key.size() != 1) {
                throw std::runtime_error("single-key consonant must be 1 char: " + key);
            }
            char32_t ascii_cp = static_cast<unsigned char>(key[0]);
            if (it.value().contains("base") && !it.value()["base"].is_null()) {
                auto base = it.value()["base"].get<std::string>();
                std::u32string seq{ascii_cp};
                m.consonants.insert(seq, ConsonantEntry{first_codepoint(base)});
            }
            if (it.value().contains("shift") && !it.value()["shift"].is_null()) {
                auto shift = it.value()["shift"].get<std::string>();
                std::u32string seq{shift_key(ascii_cp)};
                m.consonants.insert(seq, ConsonantEntry{first_codepoint(shift)});
            }
        }
    }

    // Multi-key consonants.
    if (data.contains("multi_key_consonants")) {
        for (auto it = data["multi_key_consonants"].begin();
             it != data["multi_key_consonants"].end(); ++it) {
            auto seq = multi_key_to_seq(it.key());
            auto cp  = first_codepoint(it.value().get<std::string>());
            m.consonants.insert(seq, ConsonantEntry{cp});
        }
    }

    // Vowels: merge standalone + vowel_signs keyed by ASCII sequence.
    auto upsert_vowel = [&](const std::u32string& seq, char32_t standalone, char32_t matra) {
        auto existing = m.vowels.find(seq);
        VowelEntry e = existing.value_or(VowelEntry{});
        if (standalone) e.standalone = standalone;
        if (matra)      e.matra      = matra;
        m.vowels.insert(seq, e);
    };

    if (data.contains("vowels_standalone")) {
        for (auto it = data["vowels_standalone"].begin();
             it != data["vowels_standalone"].end(); ++it) {
            auto seq = vowel_key_to_seq(it.key());
            char32_t cp = 0;
            if (!it.value().is_null()) cp = first_codepoint(it.value().get<std::string>());
            upsert_vowel(seq, cp, 0);
        }
    }
    if (data.contains("vowel_signs")) {
        for (auto it = data["vowel_signs"].begin(); it != data["vowel_signs"].end(); ++it) {
            auto seq = vowel_key_to_seq(it.key());
            char32_t cp = 0;
            if (!it.value().is_null()) cp = first_codepoint(it.value().get<std::string>());
            // We need to preserve whether a standalone was set previously; fetch-and-merge.
            auto existing = m.vowels.find(seq);
            VowelEntry e = existing.value_or(VowelEntry{});
            e.matra = cp;  // may be 0 (e.g. for "a" -> no matra)
            m.vowels.insert(seq, e);
        }
    }

    if (data.contains("special")) {
        auto& sp = data["special"];
        if (sp.contains("virama"))   m.virama   = first_codepoint(sp["virama"].get<std::string>());
        if (sp.contains("anusvara")) m.anusvara = first_codepoint(sp["anusvara"].get<std::string>());
        if (sp.contains("visarga"))  m.visarga  = first_codepoint(sp["visarga"].get<std::string>());
        if (sp.contains("zwj"))      m.zwj      = first_codepoint(sp["zwj"].get<std::string>());
    }

    if (data.contains("rules")) {
        auto& r = data["rules"];
        if (r.contains("terminator_key"))
            m.terminator_key = rule_key_to_cp(r["terminator_key"].get<std::string>());
        if (r.contains("conjunct_marker"))
            m.conjunct_marker = rule_key_to_cp(r["conjunct_marker"].get<std::string>());
        if (r.contains("anusvara_key"))
            m.anusvara_key = rule_key_to_cp(r["anusvara_key"].get<std::string>());
        if (r.contains("visarga_key"))
            m.visarga_key = rule_key_to_cp(r["visarga_key"].get<std::string>());
        if (r.contains("unmapped_passthrough"))
            m.unmapped_passthrough = r["unmapped_passthrough"].get<bool>();
    }

    return m;
}

Mapping Mapping::from_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open mapping file: " + path);
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return Mapping::from_json_string(buf.str());
}

}  // namespace sinhala_ime
