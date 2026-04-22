# Phase 3 — Windows TSF Integration Handoff

> This document assumes you are a Windows developer picking up where
> Phases 1 & 2 left off. Phases 1–2 delivered a platform-neutral C++20
> engine library (`sinhala_ime_engine`) and a Python reference prototype.
> This document tells you exactly what to build next.

## Scope of Phase 3

Implement a **Windows TSF (Text Services Framework) text-input processor DLL**
that:

1. Runs on Windows 10 and Windows 11.
2. Registers as a system-wide text service.
3. Receives key events from any TSF-aware host (UWP, Win32 edit controls,
   Edge, Word, Notepad++, VS Code, …).
4. Delegates all mapping logic to `sinhala_ime::Engine` (do not re-implement
   the state machine).
5. Displays a composition preview and commits UTF-8 text into the host
   document.

Out of scope for Phase 3 (those are Phases 4–5): installer, code signing,
auto-update, candidate windows, settings UI.

## Deliverables

```
tsf_dll/                         # new subdirectory (parallel to engine/)
├── CMakeLists.txt               # builds a .dll on Windows only
├── include/
│   └── sinhala_tsf/
│       └── text_service.h       # concrete class (implements the stub)
├── src/
│   ├── text_service.cpp
│   ├── key_event_sink.cpp
│   ├── composition_sink.cpp
│   ├── display_attribute_provider.cpp
│   ├── class_factory.cpp
│   └── registration.cpp         # DllRegisterServer / DllUnregisterServer
└── tsf_dll.def                  # DLL exports
```

Each COM class should delegate all semantics to `sinhala_ime::Engine`:

```cpp
#include <sinhala_ime/engine.h>
...
class CSinhalaTextService : public ITfTextInputProcessor, ... {
  std::unique_ptr<sinhala_ime::Engine> m_engine =
      sinhala_ime::Engine::from_mapping_file(kMappingPath);
  ...
};
```

## TSF interfaces you need to implement

At minimum:

| Interface | Purpose |
|---|---|
| `ITfTextInputProcessor` / `ITfTextInputProcessorEx` | service entry/exit |
| `ITfThreadMgrEventSink` | focus + document tracking |
| `ITfKeyEventSink` | the actual keystroke handling |
| `ITfCompositionSink` | track the composition lifecycle |
| `ITfDisplayAttributeProvider` | style the composition preview (underline) |
| `ITfClassFactory` | COM plumbing |

MS official sample: <https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/Win7Samples/winui/input/tsf/textservice>

Also useful:

- [Text Services Framework Reference](https://learn.microsoft.com/en-us/windows/win32/api/_tsf/)
- [MSKLC has been deprecated; use TSF, not KLID](https://learn.microsoft.com/en-us/globalization/keyboards/)
- [CLSID/GUID registration pattern](https://learn.microsoft.com/en-us/windows/win32/tsf/implementing-the-text-service)

## Wiring a key event into the engine

Pseudocode for the core of `OnTestKeyDown`/`OnKeyDown`:

```cpp
STDMETHODIMP CKeyEventSink::OnKeyDown(ITfContext* ctx, WPARAM wParam,
                                      LPARAM lParam, BOOL* pfEaten) {
    // 1. Translate WPARAM + modifier state into a sinhala_ime::KeyEvent.
    sinhala_ime::KeyEvent ev{};
    ev.codepoint    = MapVirtualKeyToCodepoint(wParam, lParam);
    ev.mods         = IsShiftDown() ? sinhala_ime::KeyModifier::Shift
                                    : sinhala_ime::KeyModifier::None;
    ev.is_backspace = (wParam == VK_BACK);

    // 2. Feed the engine.
    auto r = m_svc->engine().feed(ev);

    // 3. Write output back to the host document via a TSF edit session.
    if (!r.committed.empty() || !r.composition.empty()) {
        StartEditSession(ctx, r.committed, r.composition);
    }

    *pfEaten = r.consumed ? TRUE : FALSE;
    return S_OK;
}
```

### Modifier detection

`GetKeyState(VK_SHIFT) & 0x8000` for each key event. Don't rely on Caps
Lock (TSF handles casing for you in most hosts, but you receive a virtual
keycode and have to do the translation).

### Focus loss / activation

On `OnKillFocus` (or `ITfThreadMgrEventSink::OnSetFocus` with a null
context), call `m_engine->commit()` and insert the returned UTF-8 into the
document before releasing the edit session. This prevents "stuck"
composition buffers.

### Escape key

TSF will usually pass Escape through. Treat it as `m_engine->reset()` and
discard the composition.

## Mapping file location

Ship `phonetic-default.json` next to the DLL and load via a path derived
from `GetModuleFileName`:

```cpp
wchar_t buf[MAX_PATH];
GetModuleFileNameW(g_hInst, buf, MAX_PATH);
std::filesystem::path p = buf;
p = p.parent_path() / L"phonetic-default.json";
m_engine = sinhala_ime::Engine::from_mapping_file(p.u8string());
```

## Registration

Standard TSF registration via `DllRegisterServer`. Key registry entries:

- `HKCR\CLSID\{your-guid}` — CLSID for the text service.
- `HKCU\SOFTWARE\Microsoft\CTF\TIP\{your-guid}` — language profile.
- Tag as LANG ID `0x45` (Sinhala).

Install script should call `regsvr32 /s tsf_dll.dll`.

Ship the CLSID/GUID committed in a header; do **not** regenerate per build.

## Testing on Windows

1. Build with MSVC x64 (and x86 for compatibility with 32-bit hosts).
2. Install via `regsvr32` and add via
   Settings → Time & Language → Language → Sinhala → Keyboards.
3. Smoke: open Notepad, switch to Sinhala, type `ammaa` → should produce
   `අම්මා`. Hit a space and another word should start cleanly.
4. Regression: run the same 15 TC-matrix inputs the C++ unit tests use;
   each should produce the expected text in Notepad.
5. Test in a 32-bit host (legacy `explorer.exe` address bar, older Win32
   apps).

## Things to explicitly NOT do in Phase 3

- Do not modify the state machine — all changes land in the engine library
  and cascade to TSF automatically.
- Do not call into the engine from multiple threads. Create one engine
  per input context; TSF is single-threaded per document.
- Do not swallow the `consumed=false` result — pass the keystroke through.

## Handing off to Phase 4 (installer)

- CLSID/GUID should be stable across releases.
- `phonetic-default.json` path must be resolvable from both 32-bit and
  64-bit hosts (same path, shared data).
- Uninstaller must call `DllUnregisterServer` and then remove the DLLs and
  mapping file.

## Open items the Phase 3 dev should decide

- Whether to bundle an embedded copy of the mapping JSON (linked as a
  resource) for installer robustness, vs. shipping the loose file.
- Composition display attribute (underline style, color).
- Whether to surface the "Helakuru conventions vs. our conventions"
  deviation to the user (e.g. show a small info toast on first use
  explaining `dh`/`th` and the anusvara key).
