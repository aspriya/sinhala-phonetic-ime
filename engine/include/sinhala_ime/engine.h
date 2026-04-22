// sinhala_ime/engine.h — public API for the Sinhala phonetic IME core.
//
// Thread safety: a single Engine instance is NOT thread-safe. Construct one
// per input context. Engines are cheap to construct from a cached mapping.
//
// Exception safety: factory functions may throw std::runtime_error on bad
// input; feed()/commit()/reset() never throw (noexcept not declared because
// nlohmann::json callbacks aren't noexcept; implementations catch internally).
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace sinhala_ime {

enum class KeyModifier : std::uint8_t {
    None  = 0,
    Shift = 1u << 0,
    // Reserved for future: Ctrl = 1u<<1, Alt = 1u<<2.
};

constexpr KeyModifier operator|(KeyModifier a, KeyModifier b) noexcept {
    return static_cast<KeyModifier>(
        static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
constexpr KeyModifier operator&(KeyModifier a, KeyModifier b) noexcept {
    return static_cast<KeyModifier>(
        static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}
constexpr bool has_shift(KeyModifier m) noexcept {
    return (m & KeyModifier::Shift) == KeyModifier::Shift;
}

struct KeyEvent {
    char32_t    codepoint      = 0;      // Unicode codepoint of the pressed key (typically ASCII).
    KeyModifier mods           = KeyModifier::None;
    bool        is_backspace   = false;
    bool        is_terminator  = false;  // Reserved; v1 derives terminator status from codepoint.
};

struct FeedResult {
    std::string committed;    // UTF-8 text newly committed by this event.
    std::string composition;  // UTF-8 in-progress composition preview.
    bool        consumed = false;  // true if the IME consumed the key.
};

class Engine {
public:
    // Load a mapping from a JSON file path on disk.
    // Throws std::runtime_error on missing file, invalid JSON, or schema mismatch.
    static std::unique_ptr<Engine> from_mapping_file(const std::string& path);

    // Load a mapping from an in-memory JSON string (useful for tests).
    static std::unique_ptr<Engine> from_mapping_json(std::string_view json);

    virtual ~Engine() = default;

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&)                 = delete;
    Engine& operator=(Engine&&)      = delete;

    // Feed a single key event. Returns any newly-committed text and the
    // current composition preview.
    virtual FeedResult feed(const KeyEvent& ev) = 0;

    // Force-commit the pending composition. Returns the committed UTF-8.
    virtual std::string commit() = 0;

    // Discard the pending composition without committing.
    virtual void reset() = 0;

    // Current in-progress composition buffer as UTF-8 (preview only; never finalized).
    virtual std::string composition() const = 0;

protected:
    Engine() = default;
};

}  // namespace sinhala_ime
