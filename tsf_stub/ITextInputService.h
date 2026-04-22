// ITextInputService.h — Phase-3 boundary interface.
//
// This header is INTENTIONALLY Windows-free so it compiles on Ubuntu. It
// declares an abstract interface that a future TSF DLL will implement by
// adapting to Windows COM TSF interfaces (ITfTextInputProcessor et al).
//
// Nothing in Phases 1 or 2 includes this header; it is documentation-as-code
// for the next developer.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace sinhala_ime::tsf {

// Result of handling a physical key event. Mirrors TSF's convention where
// "eaten" keys are not propagated to the host app.
struct KeyOutcome {
    bool        consumed = false;
    std::string committed_utf8;   // text to insert into the host document
    std::string composition_utf8; // text to display as composition preview
};

// Opaque session token — the real implementation will carry a TfEditCookie.
using EditCookie = std::uint32_t;

// Abstract service a TSF-side DLL implements. The Phase-2 engine does NOT
// implement this interface; Phase 3 writes a concrete class that:
//   1. Wraps a `sinhala_ime::Engine`.
//   2. Implements TSF COM interfaces and delegates to this abstraction.
class ITextInputService {
public:
    virtual ~ITextInputService() = default;

    // Activate the service for an input context (e.g. focus event).
    virtual bool activate(EditCookie ec) = 0;

    // Deactivate (e.g. focus loss). Must force-commit any pending composition.
    virtual void deactivate(EditCookie ec) = 0;

    // Handle a single physical key event. `codepoint` is the Unicode value of
    // the pressed key (for ASCII keys, just the ASCII value). `is_shift` is
    // true if Shift is held. `is_backspace` is true for backspace.
    virtual KeyOutcome on_key(EditCookie ec,
                              char32_t   codepoint,
                              bool       is_shift,
                              bool       is_backspace) = 0;

    // Read-only access to the current composition preview.
    virtual std::string composition_preview() const = 0;

    // Force commit (used on focus loss, candidate-window close, etc).
    virtual std::string force_commit(EditCookie ec) = 0;

    // Discard composition (user pressed Escape).
    virtual void discard(EditCookie ec) = 0;
};

}  // namespace sinhala_ime::tsf
