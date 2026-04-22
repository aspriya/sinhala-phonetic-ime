#include "sinhala_ime/composition.h"

#include <functional>
#include <optional>

namespace sinhala_ime {

Composer::Composer(const Mapping& mapping) noexcept : mapping_(mapping) {
    prefix_.reserve(8);
}

void Composer::reset() noexcept {
    state_             = CompositionState::Idle;
    pending_consonant_ = 0;
    prefix_.clear();
}

void Composer::append_utf8(std::string& out, char32_t cp) {
    // Encode cp as UTF-8.
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string Composer::composition() const {
    std::string out;
    switch (state_) {
    case CompositionState::Idle:
    case CompositionState::PartialConsonant:
        return out;
    case CompositionState::ConsonantPending:
        if (pending_consonant_) append_utf8(out, pending_consonant_);
        return out;
    case CompositionState::PartialVowel: {
        auto entry = mapping_.vowels.find(prefix_);
        if (pending_consonant_) append_utf8(out, pending_consonant_);
        if (entry && entry->matra) append_utf8(out, entry->matra);
        return out;
    }
    case CompositionState::PartialVowelStandalone: {
        auto entry = mapping_.vowels.find(prefix_);
        if (entry && entry->standalone) append_utf8(out, entry->standalone);
        return out;
    }
    }
    return out;
}

void Composer::flush_consonant_with_virama(std::string& out) {
    if (pending_consonant_) {
        append_utf8(out, pending_consonant_);
        append_utf8(out, mapping_.virama);
        pending_consonant_ = 0;
    }
}

void Composer::flush_consonant_plain(std::string& out) {
    if (pending_consonant_) {
        append_utf8(out, pending_consonant_);
        pending_consonant_ = 0;
    }
}

void Composer::flush_standalone_vowel(std::string& out) {
    auto entry = mapping_.vowels.find(prefix_);
    if (entry && entry->standalone) append_utf8(out, entry->standalone);
}

void Composer::flush_consonant_matra(std::string& out) {
    auto entry = mapping_.vowels.find(prefix_);
    if (pending_consonant_) append_utf8(out, pending_consonant_);
    if (entry && entry->matra) append_utf8(out, entry->matra);
    pending_consonant_ = 0;
}

// -----------------------------------------------------------------------------
// feed()/commit()
// -----------------------------------------------------------------------------
FeedOutcome Composer::feed(char32_t c, bool is_backspace) {
    FeedOutcome result;

    if (is_backspace) {
        bool was_active = state_ != CompositionState::Idle;
        // State-specific backspace handling.
        switch (state_) {
        case CompositionState::Idle:
            result.consumed = false;
            return result;
        case CompositionState::PartialConsonant:
        case CompositionState::PartialVowelStandalone:
            if (!prefix_.empty()) prefix_.pop_back();
            if (prefix_.empty()) reset();
            break;
        case CompositionState::ConsonantPending:
            pending_consonant_ = 0;
            state_             = CompositionState::Idle;
            break;
        case CompositionState::PartialVowel:
            if (!prefix_.empty()) prefix_.pop_back();
            if (prefix_.empty()) state_ = CompositionState::ConsonantPending;
            break;
        }
        result.composition = composition();
        result.consumed    = was_active;
        return result;
    }

    std::string committed;
    // Lambda to re-feed recursively when the transition table says so.
    std::function<void(char32_t)> dispatch;
    dispatch = [&](char32_t cp) {
        // Special keys first.
        if (cp == mapping_.terminator_key) {
            // Hard terminator (`x`).
            switch (state_) {
            case CompositionState::ConsonantPending:
                flush_consonant_with_virama(committed);
                break;
            case CompositionState::PartialConsonant: {
                std::u32string first(prefix_.begin(), prefix_.begin() + 1);
                auto entry = mapping_.consonants.find(first);
                if (entry) {
                    append_utf8(committed, entry->cp);
                    append_utf8(committed, mapping_.virama);
                }
                break;
            }
            case CompositionState::PartialVowel:
                flush_consonant_matra(committed);
                break;
            case CompositionState::PartialVowelStandalone:
                flush_standalone_vowel(committed);
                break;
            case CompositionState::Idle:
                break;
            }
            pending_consonant_ = 0;
            prefix_.clear();
            state_             = CompositionState::Idle;
            return;
        }
        if (cp == mapping_.conjunct_marker) {
            // `X` — virama + ZWJ on the pending consonant.
            if (state_ == CompositionState::ConsonantPending && pending_consonant_) {
                append_utf8(committed, pending_consonant_);
                append_utf8(committed, mapping_.virama);
                append_utf8(committed, mapping_.zwj);
                pending_consonant_ = 0;
                state_             = CompositionState::Idle;
            } else {
                // Soft-commit current state then reset.
                switch (state_) {
                case CompositionState::ConsonantPending:
                    flush_consonant_plain(committed); break;
                case CompositionState::PartialConsonant: {
                    std::u32string first(prefix_.begin(), prefix_.begin() + 1);
                    auto entry = mapping_.consonants.find(first);
                    if (entry) append_utf8(committed, entry->cp);
                    break;
                }
                case CompositionState::PartialVowel:
                    flush_consonant_matra(committed); break;
                case CompositionState::PartialVowelStandalone:
                    flush_standalone_vowel(committed); break;
                case CompositionState::Idle: break;
                }
                reset();
            }
            return;
        }
        if (cp == mapping_.anusvara_key) {
            // Soft-commit then emit anusvara.
            switch (state_) {
            case CompositionState::ConsonantPending:
                flush_consonant_plain(committed); break;
            case CompositionState::PartialConsonant: {
                std::u32string first(prefix_.begin(), prefix_.begin() + 1);
                auto entry = mapping_.consonants.find(first);
                if (entry) append_utf8(committed, entry->cp);
                break;
            }
            case CompositionState::PartialVowel:
                flush_consonant_matra(committed); break;
            case CompositionState::PartialVowelStandalone:
                flush_standalone_vowel(committed); break;
            case CompositionState::Idle: break;
            }
            append_utf8(committed, mapping_.anusvara);
            reset();
            return;
        }
        if (cp == mapping_.visarga_key) {
            switch (state_) {
            case CompositionState::ConsonantPending:
                flush_consonant_plain(committed); break;
            case CompositionState::PartialConsonant: {
                std::u32string first(prefix_.begin(), prefix_.begin() + 1);
                auto entry = mapping_.consonants.find(first);
                if (entry) append_utf8(committed, entry->cp);
                break;
            }
            case CompositionState::PartialVowel:
                flush_consonant_matra(committed); break;
            case CompositionState::PartialVowelStandalone:
                flush_standalone_vowel(committed); break;
            case CompositionState::Idle: break;
            }
            append_utf8(committed, mapping_.visarga);
            reset();
            return;
        }

        // State dispatch.
        switch (state_) {
        case CompositionState::Idle: {
            std::u32string one{cp};
            bool cons_exact   = mapping_.consonants.find(one).has_value();
            bool cons_isprefx = mapping_.consonants.is_strict_prefix(one);
            if (cons_exact && !cons_isprefx) {
                auto entry          = mapping_.consonants.find(one).value();
                pending_consonant_  = entry.cp;
                state_              = CompositionState::ConsonantPending;
                return;
            }
            if (cons_exact || cons_isprefx) {
                prefix_ = one;
                state_  = CompositionState::PartialConsonant;
                return;
            }
            bool vow_exact   = mapping_.vowels.find(one).has_value();
            bool vow_isprefx = mapping_.vowels.is_strict_prefix(one);
            if (vow_exact && !vow_isprefx) {
                auto entry = mapping_.vowels.find(one).value();
                if (entry.standalone) append_utf8(committed, entry.standalone);
                state_ = CompositionState::Idle;
                return;
            }
            if (vow_exact || vow_isprefx) {
                prefix_ = one;
                state_  = CompositionState::PartialVowelStandalone;
                return;
            }
            // Unmapped passthrough.
            if (mapping_.unmapped_passthrough) {
                append_utf8(committed, cp & ~kShiftBit);
            }
            return;
        }
        case CompositionState::PartialConsonant: {
            std::u32string extended = prefix_;
            extended.push_back(cp);
            bool exact_ext    = mapping_.consonants.find(extended).has_value();
            bool prefix_ext   = mapping_.consonants.is_strict_prefix(extended);
            if (exact_ext || prefix_ext) {
                prefix_ = extended;
                if (exact_ext && !prefix_ext) {
                    pending_consonant_  = mapping_.consonants.find(extended).value().cp;
                    prefix_.clear();
                    state_              = CompositionState::ConsonantPending;
                }
                return;
            }
            // Resolve buffered prefix as single-char consonant, then re-feed.
            std::u32string first(prefix_.begin(), prefix_.begin() + 1);
            auto entry = mapping_.consonants.find(first);
            if (entry) {
                pending_consonant_ = entry->cp;
                prefix_.clear();
                state_             = CompositionState::ConsonantPending;
                // Re-dispatch against ConsonantPending by recursion.
                dispatch(cp);
                return;
            }
            // Defensive: drop and re-feed.
            prefix_.clear();
            state_ = CompositionState::Idle;
            dispatch(cp);
            return;
        }
        case CompositionState::ConsonantPending: {
            std::u32string one{cp};
            bool vow_exact   = mapping_.vowels.find(one).has_value();
            bool vow_isprefx = mapping_.vowels.is_strict_prefix(one);
            if (vow_exact || vow_isprefx) {
                if (vow_isprefx) {
                    prefix_ = one;
                    state_  = CompositionState::PartialVowel;
                    return;
                }
                auto entry = mapping_.vowels.find(one).value();
                if (pending_consonant_) append_utf8(committed, pending_consonant_);
                if (entry.matra)        append_utf8(committed, entry.matra);
                pending_consonant_ = 0;
                state_             = CompositionState::Idle;
                return;
            }
            bool cons_exact   = mapping_.consonants.find(one).has_value();
            bool cons_isprefx = mapping_.consonants.is_strict_prefix(one);
            if (cons_exact && !cons_isprefx) {
                flush_consonant_with_virama(committed);
                pending_consonant_ = mapping_.consonants.find(one).value().cp;
                state_             = CompositionState::ConsonantPending;
                return;
            }
            if (cons_exact || cons_isprefx) {
                flush_consonant_with_virama(committed);
                prefix_ = one;
                state_  = CompositionState::PartialConsonant;
                return;
            }
            // Unmapped.
            flush_consonant_plain(committed);
            if (mapping_.unmapped_passthrough) append_utf8(committed, cp & ~kShiftBit);
            return;
        }
        case CompositionState::PartialVowel: {
            std::u32string extended = prefix_;
            extended.push_back(cp);
            bool exact_ext    = mapping_.vowels.find(extended).has_value();
            bool prefix_ext   = mapping_.vowels.is_strict_prefix(extended);
            if (exact_ext || prefix_ext) {
                prefix_ = extended;
                if (exact_ext && !prefix_ext) {
                    auto entry = mapping_.vowels.find(extended).value();
                    if (pending_consonant_) append_utf8(committed, pending_consonant_);
                    if (entry.matra)        append_utf8(committed, entry.matra);
                    pending_consonant_ = 0;
                    prefix_.clear();
                    state_             = CompositionState::Idle;
                }
                return;
            }
            // Commit current vowel prefix as matra, then re-dispatch.
            auto curr = mapping_.vowels.find(prefix_);
            if (curr) {
                if (pending_consonant_) append_utf8(committed, pending_consonant_);
                if (curr->matra)        append_utf8(committed, curr->matra);
                pending_consonant_ = 0;
            }
            prefix_.clear();
            state_ = CompositionState::Idle;
            dispatch(cp);
            return;
        }
        case CompositionState::PartialVowelStandalone: {
            std::u32string extended = prefix_;
            extended.push_back(cp);
            bool exact_ext    = mapping_.vowels.find(extended).has_value();
            bool prefix_ext   = mapping_.vowels.is_strict_prefix(extended);
            if (exact_ext || prefix_ext) {
                prefix_ = extended;
                if (exact_ext && !prefix_ext) {
                    auto entry = mapping_.vowels.find(extended).value();
                    if (entry.standalone) append_utf8(committed, entry.standalone);
                    prefix_.clear();
                    state_ = CompositionState::Idle;
                }
                return;
            }
            auto curr = mapping_.vowels.find(prefix_);
            if (curr && curr->standalone) {
                append_utf8(committed, curr->standalone);
            }
            prefix_.clear();
            state_ = CompositionState::Idle;
            dispatch(cp);
            return;
        }
        }
    };

    dispatch(c);

    result.committed   = std::move(committed);
    result.composition = composition();
    result.consumed    = true;
    return result;
}

std::string Composer::commit() {
    std::string out;
    switch (state_) {
    case CompositionState::Idle:
        break;
    case CompositionState::ConsonantPending:
        flush_consonant_plain(out);
        break;
    case CompositionState::PartialConsonant: {
        std::u32string first(prefix_.begin(), prefix_.begin() + 1);
        auto entry = mapping_.consonants.find(first);
        if (entry) append_utf8(out, entry->cp);
        break;
    }
    case CompositionState::PartialVowel:
        flush_consonant_matra(out);
        break;
    case CompositionState::PartialVowelStandalone:
        flush_standalone_vowel(out);
        break;
    }
    reset();
    return out;
}

}  // namespace sinhala_ime
