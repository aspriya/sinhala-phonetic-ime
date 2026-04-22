# `tsf_stub/` — Phase-3 placeholder

This directory is **not built** as part of Phases 1 & 2. It exists so that a
Windows developer continuing with Phase 3 (Microsoft TSF COM integration) has
a defined interface boundary to implement against.

`ITextInputService.h` is a header-only, Ubuntu-compilable abstraction that
models the minimal surface a TSF DLL will need. The real Phase-3 code will
live in a separate `tsf_dll/` subtree (or similar) that depends on
`sinhala_ime_engine` and adapts its calls to `ITfTextInputProcessor`,
`ITfKeyEventSink`, `ITfCompositionSink`, and friends.

See `docs/phase-3-tsf-handoff.md` for the full roadmap.
