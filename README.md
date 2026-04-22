# sinhala-phonetic-ime

[![CI — Ubuntu](https://github.com/aspriya/sinhala-phonetic-ime/actions/workflows/ci-ubuntu.yml/badge.svg)](https://github.com/aspriya/sinhala-phonetic-ime/actions/workflows/ci-ubuntu.yml)
[![CI — Windows](https://github.com/aspriya/sinhala-phonetic-ime/actions/workflows/ci-windows.yml/badge.svg)](https://github.com/aspriya/sinhala-phonetic-ime/actions/workflows/ci-windows.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

> A lightweight, open-source Sinhala phonetic input method for Windows. Ad-free, telemetry-free, minimal-footprint alternative to Helakuru.

## Status

| Phase | Scope | Status |
|---|---|---|
| 1 | Phonetic mapping design + Python reference prototype | in progress |
| 2 | C++20 core engine + GTest suite + CMake/CI | in progress |
| 3 | Windows TSF integration (COM, `ITfTextInputProcessor`, …) | **not started** — see [handoff doc](docs/phase-3-tsf-handoff.md) |
| 4 | WiX installer | not started |
| 5 | Public beta + code signing | not started |

## Quickstart

### Try the Python reference prototype

```bash
cd prototype
python -m pip install -r requirements.txt
echo "ammaa" | python transliterate.py      # prints: අම්මා
pytest -v
```

### Build & test the C++ engine (Ubuntu / macOS / Windows)

```bash
cmake -B build -S . -DSINHALA_IME_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requires CMake 3.20+, a C++20 compiler, and internet access (FetchContent pulls `nlohmann/json` v3.11.3 and `GoogleTest` v1.14.0).

## What's in the box

```
mapping/phonetic-default.json    # the phonetic keymap (authoritative spec)
docs/phonetic-mapping-design.md  # state machine + worked examples
prototype/                       # pure-Python reference implementation
engine/                          # C++20 core library (sinhala_ime_engine)
engine/tests/                    # GTest suite + golden-file cross-validation
tsf_stub/                        # header-only Phase-3 interface (Ubuntu-compilable)
tools/generate_golden.py         # regenerate the cross-validation golden file
```

## Design highlights

- **Longest-match-wins** prefix trie over `char32_t` keys (handles Shift-modified + multi-key inputs uniformly).
- **Explicit composition state machine** (`Idle`, `PartialConsonant`, `ConsonantPending`, `PartialVowel`, `PartialVowelStandalone`) — no heuristics.
- **Python prototype is the source of truth**; C++ must match byte-for-byte against `engine/tests/golden.txt`.
- **UTF-8 throughout** on the API boundary; internal trie uses UTF-32.
- **No exceptions cross the API boundary** — clean for future COM interop.

See [`docs/architecture.md`](docs/architecture.md) for the full picture and [`docs/phonetic-mapping-design.md`](docs/phonetic-mapping-design.md) for every key-to-Sinhala mapping and the authoritative state-transition table.

## Typing conventions (summary)

| To type | Keystrokes |
|---|---|
| `ක` (ka, inherent) | `k` |
| `කා` (kā) | `kaa` |
| `ක්` (k, virama) | `kx` |
| `ත` (dental ta) | `th` |
| `ට` (retroflex ta) | `t` |
| `ද` (dental da) | `dh` |
| `ඩ` (retroflex da) | `d` |
| `ශ` (sha) | `sh` |
| `ෂ` (sha, sibilant) | `Sh` or `S` |
| `ං` (anusvara) | `q` |
| `ඃ` (visarga) | `Q` |
| `ඟ` (prenasalised ga) | `^g` |
| Conjunct (virama + ZWJ) | `X` between two consonants |

Full table: [`docs/phonetic-mapping-design.md`](docs/phonetic-mapping-design.md).

## License

MIT — see [`LICENSE`](LICENSE).

## Contributing

See [`CONTRIBUTING.md`](CONTRIBUTING.md). Phase-3 Windows-TSF contributors should start with [`docs/phase-3-tsf-handoff.md`](docs/phase-3-tsf-handoff.md).
