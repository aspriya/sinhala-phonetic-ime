# Contributing to sinhala-phonetic-ime

Thanks for your interest in making Sinhala typing better on Windows!

## Ground rules

- The **Python prototype (`prototype/transliterate.py`) is the reference implementation.** The C++ engine must produce byte-for-byte identical UTF-8 output for every input in `engine/tests/golden.txt`.
- Any change to mapping semantics requires:
  1. Updating `docs/phonetic-mapping-design.md` (state table + examples).
  2. Updating `mapping/phonetic-default.json`.
  3. Regenerating `engine/tests/golden.txt` via `python tools/generate_golden.py`.
  4. Passing `pytest` (Python) **and** `ctest` (C++).
- Keep the C++ engine portable: no Windows-specific code outside `tsf_stub/` (and even that is header-only on Ubuntu).
- Use conventional commits: `feat:`, `fix:`, `test:`, `docs:`, `ci:`, `refactor:`, `chore:`.

## Local setup

### Python prototype

```bash
cd prototype
python -m pip install -r requirements.txt
pytest -v
```

### C++ engine (Ubuntu)

```bash
cmake -B build -S . -DSINHALA_IME_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Requires CMake 3.20+, a C++20 compiler (GCC 11+, Clang 14+, or MSVC 19.3+), and an internet connection for `FetchContent` to pull `nlohmann/json` and `GoogleTest`.

### Cross-validation

```bash
python tools/generate_golden.py          # writes engine/tests/golden.txt
cmake --build build --target test_engine # C++ suite includes golden-file test
```

## Pull request checklist

- [ ] Design-doc changes (if semantics changed).
- [ ] Mapping JSON updated and schema-valid.
- [ ] Python tests pass.
- [ ] C++ tests pass on at least GCC or Clang.
- [ ] Golden file regenerated if corpus or rules changed.
- [ ] No new compiler warnings at `-Wall -Wextra -Wpedantic`.

## Phase 3 (Windows TSF) contributors

Windows-specific work is intentionally **not** in Phases 1–2. See `docs/phase-3-tsf-handoff.md` for the roadmap.
