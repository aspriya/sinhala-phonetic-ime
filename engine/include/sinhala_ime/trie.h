// sinhala_ime/trie.h — generic prefix trie keyed on sequences of char32_t.
//
// Stores a payload of type T at leaves/internal-nodes that have an explicit
// value. Supports:
//   - insert(sequence, value)
//   - find(sequence) -> optional value
//   - longest_prefix_value(sequence) -> { value, length } where length is the
//     number of symbols consumed (<= sequence.size())
//   - is_prefix(sequence) -> true if any inserted key extends this sequence
//     (strict prefix check)
//
// Design choices:
//   - char32_t keys so ASCII + Shift-modified symbols live in the same tree.
//     Shift-modified ASCII uses the high code-point space starting at
//     0x80000000 to avoid collisions with real Unicode.
//   - Payload T must be default-constructible, copyable, and moveable.
//   - All APIs are const after construction where possible. No allocations
//     on hot-path lookups.
#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace sinhala_ime {

// Bit stashed in a char32_t to mark "Shift-modified ASCII key". Real Unicode
// stops at U+10FFFF (21 bits); we use bit 31.
inline constexpr char32_t kShiftBit = 0x80000000u;

inline constexpr char32_t shift_key(char32_t ascii) noexcept {
    return ascii | kShiftBit;
}

template <class T>
class Trie {
public:
    Trie() = default;

    void insert(std::u32string_view seq, T value) {
        Node* node = &root_;
        for (char32_t c : seq) {
            auto& child = node->children[c];
            if (!child) child = std::make_unique<Node>();
            node = child.get();
        }
        node->value = std::move(value);
        node->has_value = true;
    }

    // Exact-match lookup.
    std::optional<T> find(std::u32string_view seq) const {
        const Node* node = &root_;
        for (char32_t c : seq) {
            auto it = node->children.find(c);
            if (it == node->children.end()) return std::nullopt;
            node = it->second.get();
        }
        if (!node->has_value) return std::nullopt;
        return node->value;
    }

    struct LongestMatch {
        std::optional<T> value;
        std::size_t      length = 0;  // Symbols consumed from seq.
    };

    // Returns the longest prefix of `seq` that has a stored value.
    LongestMatch longest_prefix_value(std::u32string_view seq) const {
        LongestMatch best;
        const Node* node = &root_;
        for (std::size_t i = 0; i < seq.size(); ++i) {
            auto it = node->children.find(seq[i]);
            if (it == node->children.end()) break;
            node = it->second.get();
            if (node->has_value) {
                best.value  = node->value;
                best.length = i + 1;
            }
        }
        return best;
    }

    // True if some inserted key strictly extends `seq` (i.e. `seq` is a proper
    // prefix of a stored key). If `seq` itself matches a stored key but no
    // extension exists, returns false.
    bool is_strict_prefix(std::u32string_view seq) const {
        const Node* node = descend(seq);
        if (!node) return false;
        return !node->children.empty();
    }

    // True if at least one stored key starts with `seq` (including equal).
    bool is_prefix(std::u32string_view seq) const {
        return descend(seq) != nullptr;
    }

private:
    struct Node {
        std::unordered_map<char32_t, std::unique_ptr<Node>> children;
        T    value{};
        bool has_value = false;
    };

    const Node* descend(std::u32string_view seq) const {
        const Node* node = &root_;
        for (char32_t c : seq) {
            auto it = node->children.find(c);
            if (it == node->children.end()) return nullptr;
            node = it->second.get();
        }
        return node;
    }

    Node root_{};
};

}  // namespace sinhala_ime
