# Sinhala Phonetic Mapping — Design Specification

**Status:** Authoritative spec for Phase 1 (Python prototype) and Phase 2 (C++ engine). Both implementations code to this document; the golden file is the executable contract.

**Unicode range:** U+0D80–U+0DFF (Sinhala block) plus U+200D (ZWJ).

---

## 1. Guiding Principles

1. **Longest-match wins.** Every key that is a *proper prefix* of a longer mapped key causes the engine to *buffer* that input until the ambiguity is resolved.
2. **Shift-modified keys are distinct trie inputs,** not post-processed. `K` and `k` are different lookup keys.
3. **Multi-key digraphs are first-class trie entries** keyed on the raw ASCII sequence: `ch`, `th`, `dh`, `sh`, and their Shift-initial aspirated variants `Ch`, `Th`, `Dh`, `Sh`.
4. **Vowel required, terminator ⇒ virama.** A consonant is committed with virama `්` (U+0DCA) unless followed by an explicit vowel key. There is *no* automatic inherent `a`. (This replaces the original "implicit a" rule to reconcile TC-02/TC-06/TC-11 with TC-03.)
5. **UTF-8 for committed output; UTF-32 (`char32_t`) for trie keys.**
6. **Byte-for-byte Python↔C++ parity.** Both implementations produce identical UTF-8 bytes for every input in the golden file.

---

## 2. Key Assignments

### 2.1 Consonants (single key, with optional Shift variant)

| Key | Sinhala | U+ | Shift | Sinhala | U+ |
|---|---|---|---|---|---|
| k | ක | 0D9A | K | ඛ | 0D9B |
| g | ග | 0D9C | G | ඝ | 0D9D |
| j | ජ | 0DA2 | J | ඣ | 0DA3 |
| t | ට | 0DA7 | T | ඨ | 0DA8 |
| d | ඩ | 0DA9 | D | ඪ | 0DAA |
| n | න | 0DB1 | N | ණ | 0DAB |
| p | ප | 0DB4 | P | ඵ | 0DB5 |
| b | බ | 0DB6 | B | භ | 0DB7 |
| m | ම | 0DB8 | — | — | — |
| y | ය | 0DBA | — | — | — |
| r | ර | 0DBB | — | — | — |
| l | ල | 0DBD | L | ළ | 0DC5 |
| v | ව | 0DC0 | — | — | — |
| w | ව | 0DC0 | — | — | — |
| s | ස | 0DC3 | S | ෂ | 0DC2 |
| h | හ | 0DC4 | — | — | — |
| f | ෆ | 0DC6 | — | — | — |

### 2.2 Multi-key consonants (digraphs)

| Keys | Sinhala | U+ | Notes |
|---|---|---|---|
| ch | ච | 0DA0 | |
| Ch | ඡ | 0DA1 | aspirated |
| th | ත | 0DAD | |
| Th | ථ | 0DAE | aspirated |
| dh | ද | 0DAF | |
| Dh | ධ | 0DB0 | aspirated |
| sh | ශ | 0DC1 | |
| Sh | ෂ | 0DC2 | same as Shift+s (intentional alias) |

### 2.3 Prenasalised consonants (added v1)

| Keys | Sinhala | U+ | Notes |
|---|---|---|---|
| ^k | ඟ | 0D9F | prenasalised g (ඟ, "nga") — key chosen as `^g` below; `^k` is **not** defined |
| ^g | ඟ | 0D9F | prenasalised g |
| ^d | ඳ | 0DB3 | prenasalised d (dental) |
| ^D | ඬ | 0DAC | prenasalised D (retroflex) |
| ^b | ඹ | 0DB9 | prenasalised b |

> **Note on `^k`:** `^k` is listed in `multi_key_consonants` as an alias for U+0D9F for convenience. `^g` is the preferred form.

### 2.4 Vowels — standalone (start of word / after terminator)

| Keys | Sinhala | U+ |
|---|---|---|
| a | අ | 0D85 |
| aa / A | ආ | 0D86 |
| ae | ඇ | 0D87 |
| aee / Ae | ඈ | 0D88 |
| i | ඉ | 0D89 |
| ii / I | ඊ | 0D8A |
| u | උ | 0D8B |
| uu / U | ඌ | 0D8C |
| e | එ | 0D91 |
| ee / E | ඒ | 0D92 |
| ai | ඓ | 0D93 |
| o | ඔ | 0D94 |
| oo / O | ඕ | 0D95 |
| au | ඖ | 0D96 |

### 2.5 Vowel signs (matras — applied to preceding consonant)

| Keys | Matra | U+ |
|---|---|---|
| a | (no matra; consonant commits as-is) | — |
| aa / A | ා | 0DCF |
| ae | ැ | 0DD0 |
| aee / Ae | ෑ | 0DD1 |
| i | ි | 0DD2 |
| ii / I | ී | 0DD3 |
| u | ු | 0DD4 |
| uu / U | ූ | 0DD6 |
| e | ෙ | 0DD9 |
| ee / E | ේ | 0DDA |
| ai | ෛ | 0DDB |
| o | ො | 0DDC |
| oo / O | ෝ | 0DDD |
| au | ෞ | 0DDE |

### 2.6 Special characters

| Name | Codepoint | Key | Notes |
|---|---|---|---|
| virama (hal kirima) | U+0DCA ් | (auto; or `x`) | Inserted automatically when a consonant commits without a following vowel. `x` forces commit of the pending consonant as a pure consonant (virama) and returns to Idle. |
| anusvara | U+0D82 ං | `q` | Strict, explicitly-typed. Auto-nasalization (e.g. `nk` ⇒ `ංක`) is **not** implemented in v1; deferred. |
| visarga | U+0D83 ඃ | `Q` | |
| ZWJ | U+200D | `X` | Conjunct marker: inserts virama+ZWJ between two consonants instead of the default virama. |

### 2.7 Unmapped ASCII keys

Any ASCII character that is not a mapped key (e.g. `c`, `z`, digits, punctuation, space, newline) is treated as a **terminator**:

1. The current composition buffer is committed (consonants flushed with virama as appropriate).
2. The unmapped character itself is appended verbatim to the committed output.
3. The state machine returns to `Idle`.

This provides clean passthrough for English loanwords, punctuation, whitespace, and numbers. It also means **space, Enter, `.`, `,`, etc. are all terminators** by virtue of being unmapped.

---

## 3. Composition State Machine

### 3.1 States

| State | Meaning | Buffer contents |
|---|---|---|
| `Idle` | No pending input. | empty |
| `PartialConsonant` | An ASCII key was typed that is a *prefix* of one or more multi-key consonants. E.g. just typed `t`: could become `th`. | the raw ASCII prefix |
| `ConsonantPending` | A consonant has been fully matched and its codepoint is buffered, awaiting a vowel, another consonant, anusvara/visarga, or a terminator. | consonant codepoint |
| `PartialVowel` | A vowel ASCII prefix has been typed *after* a `ConsonantPending` (matra case) or after `Idle` (standalone case). E.g. `ka` — could become `kaa`. | (previous consonant, if any) + vowel ASCII prefix |
| `PartialVowelStandalone` | Same as `PartialVowel` but buffered from `Idle` (no preceding consonant). | vowel ASCII prefix |

> **Why explicit `Partial*` states?** Longest-match over ASCII prefixes must be resolved before any Sinhala output is emitted. Encoding the ambiguity in state keeps the transition table total and allocation-free.

### 3.2 Input classes

| Class | Examples |
|---|---|
| `C_complete` | A fully-matched single-key consonant (`k`, `K`, `g`, ...) that is *not* a prefix of any multi-key consonant. Also `m`, `n`, `r`, `y`, `l`, `v`, `w`, `p`, `b`, `f`, etc. |
| `C_prefix` | A single ASCII key that is a proper prefix of a multi-key consonant (e.g. `t` is a prefix of `th`; `s` is a prefix of `sh`; `d` is a prefix of `dh`; `c` only exists as a prefix of `ch`/`Ch`; `^` as prefix of `^g`/`^d`/`^D`/`^b`). |
| `V_complete` | A vowel key that is *not* a prefix of a longer vowel (e.g. `ai`, `au` when typed as two-character completions; single-char vowels become this only once disambiguated). |
| `V_prefix` | A vowel key that is a prefix of a longer vowel (`a` → `aa`/`ae`/`ai`/`au`; `i` → `ii`; `u` → `uu`; `e` → `ee`; `o` → `oo`). |
| `Anusvara` | `q` |
| `Visarga` | `Q` |
| `Virama` | `x` (forced terminator for the pending consonant) |
| `Conjunct` | `X` (forces virama+ZWJ between two consonants) |
| `Backspace` | backspace key |
| `Unmapped` | any ASCII char not in the mapping — acts as terminator and passes through |

### 3.3 Canonical transition table

Rows are current state; columns are input class. Each cell lists the action (Emit = append UTF-8 to *committed*; Buffer = update internal buffer; Commit = flush buffer to committed) and the next state.

| from \\ input | C_complete | C_prefix | V_complete | V_prefix | Anusvara | Visarga | Virama (`x`) | Conjunct (`X`) | Backspace | Unmapped |
|---|---|---|---|---|---|---|---|---|---|---|
| **Idle** | Buffer consonant → `ConsonantPending` | Buffer ASCII → `PartialConsonant` | Emit standalone vowel → `Idle` | Buffer ASCII → `PartialVowelStandalone` | Emit anusvara → `Idle` | Emit visarga → `Idle` | no-op → `Idle` | no-op → `Idle` | no-op → `Idle` | Emit key verbatim → `Idle` |
| **PartialConsonant** | (a) If buffer+input extends to a longer match (still prefix), remain `PartialConsonant`. (b) If buffer+input is a complete multi-key consonant, Buffer that codepoint → `ConsonantPending`. (c) Else: Commit previous buffer as if followed by terminator (consonant+virama), then re-feed this C_complete. | Same disambiguation as above; typically remain `PartialConsonant`. | Commit buffered prefix (as consonant+virama), then re-feed V_complete to `Idle`. | Commit buffered prefix (consonant+virama), then re-feed V_prefix to `Idle` → `PartialVowelStandalone`. | Commit buffered prefix (consonant+virama) + anusvara → `Idle`. | Commit buffered prefix (consonant+virama) + visarga → `Idle`. | Commit buffered prefix (consonant+virama) → `Idle`. | Commit buffered prefix + virama+ZWJ → `Idle`. | Drop one ASCII char from prefix; if empty, → `Idle`. | Commit buffered prefix (consonant+virama); emit unmapped char → `Idle`. |
| **ConsonantPending** | Commit prior consonant + virama; Buffer new consonant → `ConsonantPending`. | Commit prior + virama; Buffer ASCII → `PartialConsonant`. | Emit prior consonant + matra(V_complete) → `Idle`. | Buffer vowel ASCII → `PartialVowel`. | Commit prior + anusvara → `Idle`. | Commit prior + visarga → `Idle`. | Commit prior + virama → `Idle`. | (held for next consonant) — treated as Unmapped for now; Commit prior + virama → `Idle`. | Drop buffered consonant → `Idle`. | Commit prior + virama; emit unmapped char → `Idle`. |
| **PartialVowel** | (a) If prefix can extend, remain `PartialVowel`. (b) If extension to complete vowel possible, emit consonant+matra(complete) → `Idle`. (c) Else: emit consonant+matra(current prefix as complete vowel), re-feed C_complete. | same disambiguation | as (b) above | as (a) above; keep extending | emit consonant+matra(current) + anusvara → `Idle` | emit consonant+matra(current) + visarga → `Idle` | emit consonant+matra(current), then virama case — *but* this leaves a consonant+matra already, so virama `x` on a matra'd consonant is a no-op on the preceding and appends a separate virama? In practice treat as: commit matra; emit new virama on *next* cycle when no consonant is pending → `Idle`. | commit matra; commit virama+ZWJ as no-op → `Idle`. | drop one char from vowel prefix; if empty, → `ConsonantPending`. | commit consonant+matra(current), emit unmapped → `Idle`. |
| **PartialVowelStandalone** | as above but standalone vowel commit, then re-feed C_complete | same | commit current vowel standalone, then emit V_complete standalone → `Idle` | extend vowel prefix | commit standalone + anusvara → `Idle` | commit standalone + visarga → `Idle` | commit standalone → `Idle` | commit standalone → `Idle` | drop one char; if empty, → `Idle`. | commit standalone; emit unmapped → `Idle`. |

> **Disambiguation on C_prefix → complete vowel boundary:** when in `PartialConsonant` with a pure ASCII prefix that could either (a) extend into a multi-key consonant or (b) be a single-letter consonant followed by a vowel, the *next key* decides. E.g. buffer = `t`, next key = `h` ⇒ extend to `th`. Next key = `a` ⇒ commit `t` as `ට` with implicit handling (actually per rule #4, `t` followed by `a` means the consonant `ට` takes matra `a` = no matra → `ට`). See worked examples.

### 3.4 Commit semantics

- `commit()` (force-flush): whatever is in the buffer is committed as if a terminator were pressed. Returns the flushed committed text.
- `reset()`: discards the buffer silently, returns to `Idle`.
- `composition()`: returns the in-progress UTF-8 preview (buffered Sinhala codepoints so far, without the implicit virama).

### 3.5 Backspace semantics

Backspace **undoes one keystroke**, not one codepoint. Concretely:

- `PartialConsonant` with `t` + Backspace → `Idle` (empty).
- `ConsonantPending` (buffer = `ක`) + Backspace → `Idle` (drops the consonant).
- `PartialVowel` (buffered consonant + `a`) + Backspace → `ConsonantPending` (drops the vowel prefix).
- If the composition has already committed text and the user backspaces, the engine does **not** re-claim committed bytes — backspace is passed through to the host application (via `consumed=false`). This matches IME norms.

---

## 4. Worked Examples (10)

Each example shows the input sequence, state transitions, and the final committed UTF-8 bytes. Assume `commit()` is called at end-of-input unless otherwise noted.

### 4.1 `ammaa` → `අම්මා` (U+0D85 U+0DB8 U+0DCA U+0DB8 U+0DCF)

| Step | Key | State before | Action | State after | Committed |
|---|---|---|---|---|---|
| 1 | `a` | Idle | V_prefix → buffer `a` | PartialVowelStandalone | — |
| 2 | `m` | PartialVowelStandalone | C_complete ⇒ commit standalone `අ`, re-feed `m` | ConsonantPending (ම) | `අ` |
| 3 | `m` | ConsonantPending | C_complete ⇒ commit prior ම+virama, buffer new ම | ConsonantPending (ම) | `අම්` |
| 4 | `a` | ConsonantPending | V_prefix ⇒ buffer `a` | PartialVowel (ම, `a`) | `අම්` |
| 5 | `a` | PartialVowel | extends to `aa` complete vowel ⇒ emit ම+matra ා | Idle | `අම්මා` |
| 6 | commit | Idle | no-op | Idle | `අම්මා` ✓ |

### 4.2 `ashan` → `අශන්`

| Step | Key | State | Action | Committed |
|---|---|---|---|---|
| 1 | `a` | Idle → PartialVowelStandalone | buffer `a` | — |
| 2 | `s` | C_prefix (s→sh) ⇒ commit `අ`, re-feed `s` ⇒ buffer `s` | PartialConsonant | `අ` |
| 3 | `h` | PartialConsonant | `sh` complete ⇒ buffer ශ | ConsonantPending | `අ` |
| 4 | `a` | ConsonantPending | V_prefix ⇒ buffer `a` | PartialVowel (ශ, `a`) | `අ` |
| 5 | `n` | PartialVowel | C_complete ⇒ emit ශ + no matra (vowel `a` resolves to "no matra"), re-feed `n` ⇒ buffer න | ConsonantPending (න) | `අශ` |
| 6 | commit | ConsonantPending | flush with virama | Idle | `අශන්` ✓ |

### 4.3 `kaaw` + commit → `කාව්`

| Step | Key | State / Action | Committed |
|---|---|---|---|
| 1 | `k` | → ConsonantPending (ක) | — |
| 2 | `a` | → PartialVowel (ක, `a`) | — |
| 3 | `a` | extends to `aa` ⇒ emit ක+ා | `කා` |
| 4 | `w` | → ConsonantPending (ව) | `කා` |
| 5 | commit | flush virama | `කාව්` ✓ |

### 4.4 `siri` → `සිරි`

`s` (prefix) → `i` (C_prefix? no, `i` is vowel) ⇒ commit buffered `s`? Actually `s` is a prefix of `sh`, so we buffer `s`. Then `i` is V_prefix (prefix of `ii`). Per the table, in PartialConsonant with V_prefix input: commit buffered prefix as consonant+virama, re-feed V_prefix to Idle. That would yield `ස්ඉ...` which is wrong.

**Correction:** in PartialConsonant, a V_prefix input means the buffered ASCII is a single-letter consonant (not extending to a multi-key), so it should be resolved as the single-letter consonant going into `ConsonantPending`, *then* the vowel is applied as a matra. Updated rule:

> In `PartialConsonant`, any input that is **not** a continuation into a longer multi-key consonant causes the buffered ASCII to be resolved as the corresponding single-key consonant (emit into `ConsonantPending`), then the current input is re-fed against `ConsonantPending`.

Re-run:

| Step | Key | State | Action | Committed / Buffer |
|---|---|---|---|---|
| 1 | `s` | Idle → PartialConsonant | buffer `s` | buf=`s` |
| 2 | `i` | PartialConsonant → resolve `s` as ස to ConsonantPending, re-feed `i` ⇒ PartialVowel (ස, `i`) | — | — |
| 3 | `r` | PartialVowel | C_complete, `i` prefix doesn't extend with `r` ⇒ emit ස+matra ි (treating `i` as complete since `ii` isn't reachable from this input), re-feed `r` | `සි` |
| 4 | → ConsonantPending (ර) | | | `සි` |
| 5 | `i` | ConsonantPending | V_prefix ⇒ PartialVowel (ර, `i`) | `සි` |
| 6 | commit | PartialVowel | emit ර+matra ි (implicit completion of `i`) | `සිරි` ✓ |

### 4.5 `gedara` → `ගෙදර`

| Step | Key | Action | Committed |
|---|---|---|---|
| `g` | buffer ග | ConsonantPending | — |
| `e` | V_prefix → PartialVowel (ග, `e`) | — |
| `d` | non-extending, `e` ⇒ emit ග+ෙ, re-feed `d` | `ගෙ` |
| `d` | C_prefix → PartialConsonant (but prior commit already done). Actually `d` here is after re-feed from PartialVowel; now in Idle re-fed as first input ⇒ wait, state is now ConsonantPending with prior ග emitted. Let's redo: after emitting ග+ෙ, state → Idle. Re-feed `d` ⇒ PartialConsonant (d is prefix of `dh`). | `ගෙ` |
| `a` | PartialConsonant → resolve `d` as ඩ, re-feed `a` ⇒ PartialVowel (ඩ, `a`) | `ගෙ` |
| `r` | non-extending ⇒ emit ඩ+no-matra = ඩ, re-feed `r` ⇒ ConsonantPending (ර) | `ගෙද` |

*Wait:* `d` is ඩ (retroflex), but `දර` in `ගෙදර` uses ද (dental, = `dh` in this mapping). **Input needs to be `gedhara`, not `gedara`, under this mapping.** Let me note this: under Helakuru, `d` and `dh` mean retroflex/dental respectively. The phonetic spelling `gedara` in common Singlish conflates the two. Documenting this as a **known convention difference** — we keep the strict mapping (user must type `gedhara` for `ගෙදර`). Our test uses the correct strict input.

Re-worked: **`gedhara` → `ගෙදර`**:

| `g` `e` `d` `h` `a` `r` `a` commit | ⇒ `ගෙ`, then `dh` matches as ද, then `a` matra (no matra), then `r` ⇒ ර, then `a` matra (no matra), then commit+virama → `ගෙදර` | ✓ |

### 4.6 `mage` → `මගේ`

`m` → ම; `a` → PartialVowel; `g` → non-extending, emit `මග` (m+no-matra, re-feed g → ConsonantPending ග)... wait that emits `ම` as `ම` (no matra, which equals U+0DB8). Hmm. But U+0DB8 alone is "ම" which visually looks like ම with the default inherent vowel — that's correct for "ma" in isolation. So `මග` so far. Then `e` → PartialVowel (ග, `e`); `commit` ⇒ PartialVowel with `e` complete ⇒ emit ග+ෙ. Wait but we want `ගේ` which is ග + ේ (U+0DDA), long e. So the user must type `ee` or `E`.

**Correct input:** `mageE` or `magee` → `මගේ`:
- `m` → ConsonantPending (ම)
- `a` → PartialVowel (ම,`a`)
- `g` → non-extending, emit ම+no-matra = ම, re-feed `g` → ConsonantPending (ග)
- `e` → PartialVowel (ග,`e`)
- `e` → extends to `ee` ⇒ emit ග+ේ → Idle
- commit → Idle
- Result: `මගේ` ✓

### 4.7 `ispirithaaleya` → `ඉස්පිරිතාලය`

All keys are single-letter or dh/th multi-key. Walk-through (abbreviated):
`i`→ඉ, `s`→ස්, `p`→ප, `i`→ි, `r`→ර, `i`→ි, `th`→ත, `aa`→ා, `l`→ල, `e`→ෙ, `y`→ය, `a`→no matra. Result: `ඉස්පිරිතාලය` ✓

### 4.8 `sakkaraa` → `සක්කරා`

`s`→ස, `a`→ස+no-matra=ස, `k`→ක, `k`→ක්ක, `a`→ක+no-matra=ක, `r`→ර, `aa`→ා. Result: `සක්කරා` ✓

### 4.9 `nagaraya` → `නගරය`

`n`→න (+no-matra), `a`→-, `g`→ග, `a`→-, `r`→ර, `a`→-, `y`→ය, `a`→-. Result: `නගරය` ✓

### 4.10 `laqkaawa` → `ලංකාව`

(Updated from `lankaawa` per the strict anusvara rule.)
`l`→ල, `a`→ල (no matra), `q`→ anusvara ං (committed after ල), `k`→ක, `aa`→ා, `w`→ව, commit→virama `්`. Result: `ලංකාව්` — but target is `ලංකාව` (no trailing virama). Issue: final `w` with no following vowel triggers commit-with-virama. In real usage the user types punctuation/space after a word, and `ලංකාව` is followed by space ⇒ commit+virama gives `ලංකාව් `. If the goal is `ලංකාව` the user must type `laqkaawaa` (so final `w` gets matra ා) — which is the correct phonetic spelling (`laqkaawa` ends in short `a`, `laqkaawa` + space ⇒ `ලංකාව් `).

**Resolution:** the expected output for `laqkaawa` is `ලංකාව්` (with virama); for `ලංකාව` input must be `laqkaawa` ending the word with terminator space = `laqkaawa ` → `ලංකාව් `. Or, if the user wants the inherent vowel rendered, type explicit `a` on the last consonant — which is what the Helakuru convention treats as "no matra, no virama either" *visually*. Since U+0DC0 on its own *is* ව with inherent vowel visually, committing without a virama is reasonable for a final consonant.

**Final rule clarification:** `commit()` / terminator on a `ConsonantPending` state inserts virama **only if** the preceding state included a vowel or if there is an explicit virama key pressed. When a consonant is the *last* input with no vowel ever attached, commit emits the consonant **without virama** (inherent vowel visual).

This refinement reconciles `ලංකාව` (ending in plain ව, no virama, no matra) with `අශන්` (ending in න්, virama, because… hmm).

Actually re-examining: `අශන්` has a visible virama because Sinhala words ending in a *closed syllable* need it. `ලංකාව` ends in an *open* syllable (inherent short `a`). The distinction is orthographic — users must signal it.

**Final v1 rule:** we adopt two terminator behaviors:
- **Soft terminator** (space, punctuation, end-of-input via `commit()`): consonant commits **without virama**. If the user wants virama, they type `x`.
- **Hard terminator** (`x` key): consonant commits **with virama**.

This fixes both `ලංකාව` (soft) and `අශන්` (must be typed `ashanx`, or our corpus changes to `ashanx`).

**Updated corpus input for TC-06:** `ashanx` → `අශන්`.
**Updated TC-02:** `k`, `x` → committed=`ක්`. (space alone yields `ක`.)
**Updated TC-11:** `t`, `x` → `ට්`.

Worked examples updated below.

### 4.10 (final) `laqkaawa` + space → `ලංකාව ` ✓

---

## 5. Updated Test Matrix (post-refinement)

| ID | Input | Expected committed | Notes |
|---|---|---|---|
| TC-01 | `k` | composition=`ක`, committed=`` | consonant pending, preview shows ක without virama |
| TC-02 | `k`, `x` | `ක්` | hard terminator ⇒ virama |
| TC-03 | `k`, `a` | `ක` (inherent; no matra emitted) | — |
| TC-04 | `k`, `a`, `a` | `කා` | long-a matra |
| TC-05 | `a`, `m`, `m`, `a`, `a` | `අම්මා` | doubled consonant ⇒ virama on first |
| TC-06 | `a`, `s`, `h`, `a`, `n`, `x` | `අශන්` | sh multi-key + hard terminator |
| TC-07 | Shift+`k`, `a` | `ඛ` (inherent) | aspirated consonant |
| TC-08 | `k`, `a`, Backspace | composition=`ක` | backspace removes the vowel prefix |
| TC-09 | `k`, `a`, Backspace, Backspace | composition=`` | fully reset |
| TC-10 | `t`, `h`, `a` | `ත` | th→ත, inherent a |
| TC-11 | `t`, `x` | `ට්` | single t + hard terminator |
| TC-12 | `l`, `a`, `q`, `k`, `a`, `a`, `w`, `a` | `ලංකාව` | explicit anusvara; final `a` → no virama |
| TC-13 | 500 random ASCII chars | no crash, no leaks | stress |
| TC-14 | empty feed + commit | `` | empty state |
| TC-15 | reset mid-composition | composition cleared | reset |

Plus 10 worked examples from §4 (some updated) and the 50-word corpus.

---

## 6. Known Deviations From "Vanilla Helakuru"

1. Dental `d` (ද) requires `dh`, not `d`. `d` alone is retroflex ඩ.
2. Dental `t` (ත) requires `th`, not `t`. `t` alone is retroflex ට.
3. Anusvara is explicit via `q` — no auto-nasalization in v1.
4. Virama is explicit via `x` at end of syllable; bare commit leaves the inherent vowel.
5. Prenasalised consonants use a `^` prefix (e.g. `^g` → ඟ).
6. Unmapped ASCII keys pass through verbatim.

Users migrating from Helakuru will find (1)–(4) the biggest adjustments. Documented prominently in the `README.md`.

---

## 7. Open Enhancements (not v1)

- Auto-nasalization (`nk`, `ng`, `mp`, `mb` ⇒ anusvara + consonant).
- Smart inherent-vowel detection based on syllable-final position.
- User-customizable keymap (loading alternative `*.json` mapping files).
- Candidate window / predictive suggestions.
