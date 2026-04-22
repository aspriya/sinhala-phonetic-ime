"""Reference Python implementation of the Sinhala phonetic IME state machine.

This module is the *source of truth* for the IME's transliteration semantics.
The C++ engine in ``engine/`` must produce byte-for-byte identical UTF-8
output for every input in ``engine/tests/golden.txt``.

See ``docs/phonetic-mapping-design.md`` for the canonical transition table.
"""

from __future__ import annotations

import json
import sys
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# -----------------------------------------------------------------------------
# Shift-modifier encoding.
#
# We store keystrokes as 32-bit values; bit 31 is set to signal "Shift+ASCII".
# Real Unicode stops at U+10FFFF, so this is safe.
# -----------------------------------------------------------------------------
SHIFT_BIT = 0x8000_0000


def shift_key(ascii_cp: int) -> int:
    return ascii_cp | SHIFT_BIT


def is_shifted(cp: int) -> bool:
    return (cp & SHIFT_BIT) != 0


def base_ascii(cp: int) -> int:
    return cp & ~SHIFT_BIT


# -----------------------------------------------------------------------------
# Mapping: loaded from JSON.
# -----------------------------------------------------------------------------
@dataclass
class VowelEntry:
    standalone: Optional[int]  # codepoint or None
    matra: Optional[int]       # codepoint or None ("a" uses None for matra)


@dataclass
class Mapping:
    version: str
    # Consonant tree indexed by a tuple of codepoints -> consonant codepoint.
    consonants: Dict[Tuple[int, ...], int] = field(default_factory=dict)
    # Vowel tree indexed by ASCII tuple -> VowelEntry.
    vowels: Dict[Tuple[int, ...], VowelEntry] = field(default_factory=dict)

    virama: int = 0x0DCA
    anusvara: int = 0x0D82
    visarga: int = 0x0D83
    zwj: int = 0x200D

    terminator_key: int = ord("x")
    conjunct_marker: int = ord("X")
    anusvara_key: int = ord("q")
    visarga_key: int = ord("Q")
    unmapped_passthrough: bool = True

    # Derived helpers.
    consonant_prefixes: set = field(default_factory=set)  # set of tuples that are strict prefixes
    vowel_prefixes: set = field(default_factory=set)

    # Set of all consonant ASCII keys (for classifying input).
    consonant_first_chars: set = field(default_factory=set)
    vowel_first_chars: set = field(default_factory=set)

    @staticmethod
    def from_json_string(text: str) -> "Mapping":
        data = json.loads(text)
        m = Mapping(version=data.get("version", ""))

        # Consonants: base + shift variants.
        for key, entry in (data.get("consonants") or {}).items():
            if len(key) != 1:
                raise ValueError(f"single-key consonant must be 1 char: {key!r}")
            ascii_cp = ord(key)
            base = entry.get("base")
            if base:
                m.consonants[(ascii_cp,)] = ord(base)
            shift = entry.get("shift")
            if shift:
                m.consonants[(shift_key(ord(key.lower()) if key.islower() else ord(key.lower())),)] = ord(shift)
                # Also: if the base key is lowercase, Shift+that produces the
                # upper-case ASCII char. We represent Shift via the shift bit.
                m.consonants[(shift_key(ascii_cp),)] = ord(shift)

        # Multi-key consonants: e.g. "ch" -> U+0DA0. Each character of the key
        # is represented as its raw ASCII codepoint. A capital first letter
        # means the user physically pressed Shift+letter, so we use shift_key
        # on that first position.
        for key, cp_str in (data.get("multi_key_consonants") or {}).items():
            tup = _multi_key_to_tuple(key)
            m.consonants[tup] = ord(cp_str)

        # Vowels: merge standalone + vowel_signs keyed by ASCII sequence.
        stand = data.get("vowels_standalone") or {}
        signs = data.get("vowel_signs") or {}
        all_keys = set(stand.keys()) | set(signs.keys())
        for key in all_keys:
            tup = _vowel_key_to_tuple(key)
            s = stand.get(key)
            sign = signs.get(key)
            entry = VowelEntry(
                standalone=(ord(s) if s else None),
                matra=(ord(sign) if sign else None),
            )
            m.vowels[tup] = entry

        special = data.get("special") or {}
        if "virama" in special:   m.virama = ord(special["virama"])
        if "anusvara" in special: m.anusvara = ord(special["anusvara"])
        if "visarga" in special:  m.visarga = ord(special["visarga"])
        if "zwj" in special:      m.zwj = ord(special["zwj"])

        rules = data.get("rules") or {}

        def _rule_key(s: str) -> int:
            # Uppercase letter means Shift+<lower> in the keystroke stream, so
            # we encode it with the shift bit to match how transliterate_string
            # feeds events.
            if len(s) != 1:
                raise ValueError(f"rule key must be 1 char: {s!r}")
            if s.isalpha() and s.isupper():
                return shift_key(ord(s.lower()))
            return ord(s)

        if "terminator_key" in rules:  m.terminator_key  = _rule_key(rules["terminator_key"])
        if "conjunct_marker" in rules: m.conjunct_marker = _rule_key(rules["conjunct_marker"])
        if "anusvara_key" in rules:    m.anusvara_key    = _rule_key(rules["anusvara_key"])
        if "visarga_key" in rules:     m.visarga_key     = _rule_key(rules["visarga_key"])
        if "unmapped_passthrough" in rules:
            m.unmapped_passthrough = bool(rules["unmapped_passthrough"])

        # Derive prefix sets.
        for tup in m.consonants:
            for i in range(1, len(tup)):
                m.consonant_prefixes.add(tup[:i])
            if tup:
                m.consonant_first_chars.add(tup[0])
        for tup in m.vowels:
            for i in range(1, len(tup)):
                m.vowel_prefixes.add(tup[:i])
            if tup:
                m.vowel_first_chars.add(tup[0])

        return m

    @staticmethod
    def from_file(path: str | Path) -> "Mapping":
        return Mapping.from_json_string(Path(path).read_text(encoding="utf-8"))

    # ---- lookup helpers -----------------------------------------------------
    def consonant_exact(self, tup: Tuple[int, ...]) -> Optional[int]:
        return self.consonants.get(tup)

    def consonant_is_prefix(self, tup: Tuple[int, ...]) -> bool:
        """True if *some* stored consonant key strictly extends `tup`."""
        return tup in self.consonant_prefixes

    def vowel_exact(self, tup: Tuple[int, ...]) -> Optional[VowelEntry]:
        return self.vowels.get(tup)

    def vowel_is_prefix(self, tup: Tuple[int, ...]) -> bool:
        return tup in self.vowel_prefixes


def _multi_key_to_tuple(key: str) -> Tuple[int, ...]:
    """Convert a multi-key string like 'Th' or '^g' into a tuple of char32_t."""
    out: List[int] = []
    for i, ch in enumerate(key):
        cp = ord(ch)
        if i == 0 and ch.isalpha() and ch.isupper():
            # User physically pressed Shift+<lower>.
            out.append(shift_key(ord(ch.lower())))
        else:
            out.append(cp)
    return tuple(out)


def _vowel_key_to_tuple(key: str) -> Tuple[int, ...]:
    """Vowel keys like 'aa', 'A', 'aee', 'I' -> tuple.

    Capital letters use the shift-bit to match how we receive shifted input.
    """
    out: List[int] = []
    for ch in key:
        if ch.isalpha() and ch.isupper():
            out.append(shift_key(ord(ch.lower())))
        else:
            out.append(ord(ch))
    return tuple(out)


# -----------------------------------------------------------------------------
# State machine.
# -----------------------------------------------------------------------------
class State(Enum):
    IDLE = 0
    PARTIAL_CONSONANT = 1
    CONSONANT_PENDING = 2
    PARTIAL_VOWEL = 3
    PARTIAL_VOWEL_STANDALONE = 4


class Transliterator:
    """Incremental Sinhala-phonetic transliterator.

    Usage::

        t = Transliterator(Mapping.from_file("mapping/phonetic-default.json"))
        for ch in "ammaa":
            committed = t.feed(ch)
        committed += t.commit()
        assert committed == "අම්මා"
    """

    def __init__(self, mapping: Mapping) -> None:
        self.m = mapping
        self._state: State = State.IDLE
        self._pending_consonant: Optional[int] = None  # codepoint
        self._prefix: List[int] = []  # buffered ASCII codepoints (with shift bit)

    # ---- public API ---------------------------------------------------------
    @property
    def state(self) -> State:
        return self._state

    def feed(self, ch: str) -> str:
        """Feed a single character. Returns newly-committed UTF-8."""
        if len(ch) != 1:
            raise ValueError("feed() takes exactly one character")
        return self._feed_cp(ord(ch), is_backspace=False)

    def feed_shifted(self, ch: str) -> str:
        """Feed a Shift+<letter> event where ch is the lowercase letter."""
        if len(ch) != 1 or not ch.isalpha():
            raise ValueError("feed_shifted expects one alphabetic character")
        return self._feed_cp(shift_key(ord(ch.lower())), is_backspace=False)

    def feed_backspace(self) -> str:
        return self._feed_cp(0, is_backspace=True)

    def commit(self) -> str:
        """Soft-commit the pending composition (no virama)."""
        out: List[str] = []
        self._soft_commit(out)
        self.reset()
        return "".join(out)

    def reset(self) -> None:
        self._state = State.IDLE
        self._pending_consonant = None
        self._prefix = []

    def composition(self) -> str:
        """UTF-8 preview of the current composition."""
        s = self._state
        if s is State.IDLE:
            return ""
        if s is State.PARTIAL_CONSONANT:
            return ""  # nothing emittable yet
        if s is State.CONSONANT_PENDING:
            return chr(self._pending_consonant) if self._pending_consonant else ""
        if s is State.PARTIAL_VOWEL:
            # Preview: consonant + matra of current vowel-prefix (if matches)
            entry = self.m.vowel_exact(tuple(self._prefix))
            if entry and entry.matra is not None and self._pending_consonant:
                return chr(self._pending_consonant) + chr(entry.matra)
            return chr(self._pending_consonant) if self._pending_consonant else ""
        if s is State.PARTIAL_VOWEL_STANDALONE:
            entry = self.m.vowel_exact(tuple(self._prefix))
            if entry and entry.standalone is not None:
                return chr(entry.standalone)
            return ""
        return ""

    # ---- state-machine core -------------------------------------------------
    def _feed_cp(self, cp: int, *, is_backspace: bool) -> str:
        out: List[str] = []
        self._feed_impl(cp, is_backspace, out)
        return "".join(out)

    def _feed_impl(self, cp: int, is_backspace: bool, out: List[str]) -> None:
        if is_backspace:
            self._handle_backspace()
            return

        # Classify special keys first (they override trie lookup).
        if cp == self.m.terminator_key:
            self._handle_hard_terminator(out)
            return
        if cp == self.m.conjunct_marker:
            self._handle_conjunct(out)
            return
        if cp == self.m.anusvara_key:
            self._handle_anusvara(out)
            return
        if cp == self.m.visarga_key:
            self._handle_visarga(out)
            return

        # Dispatch by state.
        if self._state is State.IDLE:
            self._from_idle(cp, out)
        elif self._state is State.PARTIAL_CONSONANT:
            self._from_partial_consonant(cp, out)
        elif self._state is State.CONSONANT_PENDING:
            self._from_consonant_pending(cp, out)
        elif self._state is State.PARTIAL_VOWEL:
            self._from_partial_vowel(cp, out)
        elif self._state is State.PARTIAL_VOWEL_STANDALONE:
            self._from_partial_vowel_standalone(cp, out)

    # ---- per-state handlers -------------------------------------------------
    def _from_idle(self, cp: int, out: List[str]) -> None:
        # Consonant exact single-char match?
        if (cp,) in self.m.consonants and not self.m.consonant_is_prefix((cp,)):
            self._pending_consonant = self.m.consonants[(cp,)]
            self._state = State.CONSONANT_PENDING
            return
        # Consonant prefix (could extend to a multi-key)?
        if self.m.consonant_is_prefix((cp,)) or (cp,) in self.m.consonants:
            # Could extend OR could stand on its own later; buffer.
            self._prefix = [cp]
            self._state = State.PARTIAL_CONSONANT
            return
        # Vowel single-char match with no extension?
        if (cp,) in self.m.vowels and not self.m.vowel_is_prefix((cp,)):
            entry = self.m.vowels[(cp,)]
            if entry.standalone is not None:
                out.append(chr(entry.standalone))
            self._state = State.IDLE
            return
        # Vowel prefix?
        if self.m.vowel_is_prefix((cp,)) or (cp,) in self.m.vowels:
            self._prefix = [cp]
            self._state = State.PARTIAL_VOWEL_STANDALONE
            return
        # Unmapped -> passthrough as terminator (already have empty buffer, so
        # just emit verbatim).
        self._emit_unmapped(cp, out)

    def _from_partial_consonant(self, cp: int, out: List[str]) -> None:
        extended = tuple(self._prefix + [cp])
        if self.m.consonant_is_prefix(extended) or extended in self.m.consonants:
            # Keep extending.
            self._prefix.append(cp)
            if extended in self.m.consonants and not self.m.consonant_is_prefix(extended):
                # Complete and cannot extend further -> promote.
                self._pending_consonant = self.m.consonants[extended]
                self._prefix = []
                self._state = State.CONSONANT_PENDING
            return

        # Not an extension. Resolve buffered prefix as single-char consonant.
        first = tuple(self._prefix[:1])
        if first in self.m.consonants:
            self._pending_consonant = self.m.consonants[first]
            self._prefix = []
            self._state = State.CONSONANT_PENDING
            # Re-feed the new input against ConsonantPending.
            self._feed_impl(cp, False, out)
            return

        # Buffered prefix isn't even a valid consonant (shouldn't happen, but
        # be defensive): drop it and re-feed.
        self._prefix = []
        self._state = State.IDLE
        self._feed_impl(cp, False, out)

    def _from_consonant_pending(self, cp: int, out: List[str]) -> None:
        # Vowel prefix / exact?
        if (cp,) in self.m.vowels or self.m.vowel_is_prefix((cp,)):
            if self.m.vowel_is_prefix((cp,)):
                self._prefix = [cp]
                self._state = State.PARTIAL_VOWEL
                return
            # Exact, non-extending vowel (e.g. single-char like 'ai' full form
            # — but 'a' is a prefix of 'aa', so this branch is rare).
            entry = self.m.vowels[(cp,)]
            self._emit_consonant_matra(entry, out)
            self._state = State.IDLE
            return

        # Another consonant -> commit prior with virama, start new.
        if (cp,) in self.m.consonants and not self.m.consonant_is_prefix((cp,)):
            self._commit_pending_with_virama(out)
            self._pending_consonant = self.m.consonants[(cp,)]
            self._state = State.CONSONANT_PENDING
            return
        if self.m.consonant_is_prefix((cp,)) or (cp,) in self.m.consonants:
            self._commit_pending_with_virama(out)
            self._prefix = [cp]
            self._state = State.PARTIAL_CONSONANT
            return

        # Unmapped: soft-commit prior and emit verbatim.
        self._commit_pending_plain(out)
        self._emit_unmapped(cp, out)

    def _from_partial_vowel(self, cp: int, out: List[str]) -> None:
        extended = tuple(self._prefix + [cp])
        if self.m.vowel_is_prefix(extended) or extended in self.m.vowels:
            self._prefix.append(cp)
            if extended in self.m.vowels and not self.m.vowel_is_prefix(extended):
                entry = self.m.vowels[extended]
                self._emit_consonant_matra(entry, out)
                self._prefix = []
                self._state = State.IDLE
            return

        # Commit current vowel prefix as a complete vowel if possible.
        curr = tuple(self._prefix)
        entry = self.m.vowels.get(curr)
        if entry is not None:
            self._emit_consonant_matra(entry, out)
        else:
            # Unusual: prefix isn't itself a complete vowel. Fall back to first-char.
            first_entry = self.m.vowels.get(tuple(curr[:1]))
            if first_entry:
                self._emit_consonant_matra(first_entry, out)
        self._prefix = []
        self._state = State.IDLE
        self._feed_impl(cp, False, out)

    def _from_partial_vowel_standalone(self, cp: int, out: List[str]) -> None:
        extended = tuple(self._prefix + [cp])
        if self.m.vowel_is_prefix(extended) or extended in self.m.vowels:
            self._prefix.append(cp)
            if extended in self.m.vowels and not self.m.vowel_is_prefix(extended):
                entry = self.m.vowels[extended]
                if entry.standalone is not None:
                    out.append(chr(entry.standalone))
                self._prefix = []
                self._state = State.IDLE
            return

        curr = tuple(self._prefix)
        entry = self.m.vowels.get(curr)
        if entry and entry.standalone is not None:
            out.append(chr(entry.standalone))
        self._prefix = []
        self._state = State.IDLE
        self._feed_impl(cp, False, out)

    # ---- special-key handlers ----------------------------------------------
    def _handle_hard_terminator(self, out: List[str]) -> None:
        """`x` — commit the pending consonant with virama; otherwise flush."""
        if self._state is State.CONSONANT_PENDING:
            self._commit_pending_with_virama(out)
        elif self._state is State.PARTIAL_CONSONANT:
            # Resolve buffered prefix to its single-char consonant, commit with virama.
            first = tuple(self._prefix[:1])
            if first in self.m.consonants:
                cp = self.m.consonants[first]
                out.append(chr(cp))
                out.append(chr(self.m.virama))
        elif self._state is State.PARTIAL_VOWEL:
            # Flush vowel as matra, then nothing more (no further virama).
            curr = tuple(self._prefix)
            entry = self.m.vowels.get(curr)
            if entry is not None:
                self._emit_consonant_matra(entry, out)
        elif self._state is State.PARTIAL_VOWEL_STANDALONE:
            curr = tuple(self._prefix)
            entry = self.m.vowels.get(curr)
            if entry and entry.standalone is not None:
                out.append(chr(entry.standalone))
        # Idle: no-op.
        self._pending_consonant = None
        self._prefix = []
        self._state = State.IDLE

    def _handle_conjunct(self, out: List[str]) -> None:
        """`X` — emit virama + ZWJ on the pending consonant for conjunct formation."""
        if self._state is State.CONSONANT_PENDING and self._pending_consonant is not None:
            out.append(chr(self._pending_consonant))
            out.append(chr(self.m.virama))
            out.append(chr(self.m.zwj))
            self._pending_consonant = None
            self._state = State.IDLE
        else:
            # In other states, treat as unmapped (commit + passthrough).
            self._soft_commit(out)
            self.reset()

    def _handle_anusvara(self, out: List[str]) -> None:
        self._soft_commit(out)
        out.append(chr(self.m.anusvara))
        self.reset()

    def _handle_visarga(self, out: List[str]) -> None:
        self._soft_commit(out)
        out.append(chr(self.m.visarga))
        self.reset()

    def _handle_backspace(self) -> None:
        s = self._state
        if s is State.IDLE:
            return  # pass-through is caller's problem
        if s is State.PARTIAL_CONSONANT or s is State.PARTIAL_VOWEL_STANDALONE:
            if self._prefix:
                self._prefix.pop()
            if not self._prefix:
                self.reset()
            return
        if s is State.CONSONANT_PENDING:
            self._pending_consonant = None
            self._state = State.IDLE
            return
        if s is State.PARTIAL_VOWEL:
            if self._prefix:
                self._prefix.pop()
            if not self._prefix:
                self._state = State.CONSONANT_PENDING
            return

    # ---- emit helpers ------------------------------------------------------
    def _emit_consonant_matra(self, entry: VowelEntry, out: List[str]) -> None:
        if self._pending_consonant is not None:
            out.append(chr(self._pending_consonant))
        if entry.matra is not None:
            out.append(chr(entry.matra))
        self._pending_consonant = None

    def _commit_pending_with_virama(self, out: List[str]) -> None:
        if self._pending_consonant is not None:
            out.append(chr(self._pending_consonant))
            out.append(chr(self.m.virama))
            self._pending_consonant = None

    def _commit_pending_plain(self, out: List[str]) -> None:
        if self._pending_consonant is not None:
            out.append(chr(self._pending_consonant))
            self._pending_consonant = None

    def _soft_commit(self, out: List[str]) -> None:
        s = self._state
        if s is State.IDLE:
            return
        if s is State.CONSONANT_PENDING:
            self._commit_pending_plain(out)
            return
        if s is State.PARTIAL_CONSONANT:
            # Resolve buffered prefix to its single-char consonant, emit plain.
            first = tuple(self._prefix[:1])
            if first in self.m.consonants:
                out.append(chr(self.m.consonants[first]))
            return
        if s is State.PARTIAL_VOWEL:
            curr = tuple(self._prefix)
            entry = self.m.vowels.get(curr)
            if entry is not None:
                self._emit_consonant_matra(entry, out)
            return
        if s is State.PARTIAL_VOWEL_STANDALONE:
            curr = tuple(self._prefix)
            entry = self.m.vowels.get(curr)
            if entry and entry.standalone is not None:
                out.append(chr(entry.standalone))
            return

    def _emit_unmapped(self, cp: int, out: List[str]) -> None:
        if self.m.unmapped_passthrough:
            # Strip shift bit for passthrough.
            out.append(chr(base_ascii(cp)))


# -----------------------------------------------------------------------------
# CLI: transliterate stdin → stdout.
# -----------------------------------------------------------------------------
def _default_mapping_path() -> Path:
    here = Path(__file__).resolve().parent
    return here.parent / "mapping" / "phonetic-default.json"


def transliterate_string(text: str, mapping: Optional[Mapping] = None) -> str:
    m = mapping or Mapping.from_file(_default_mapping_path())
    t = Transliterator(m)
    out = []
    for ch in text:
        if ch.isalpha() and ch.isupper():
            out.append(t.feed_shifted(ch))
        else:
            out.append(t.feed(ch))
    out.append(t.commit())
    return "".join(out)


def _main(argv: List[str]) -> int:
    m = Mapping.from_file(_default_mapping_path())
    if len(argv) > 1:
        # Treat each arg as a word to transliterate (space-separated output).
        print(" ".join(transliterate_string(w, m) for w in argv[1:]))
        return 0
    data = sys.stdin.read()
    sys.stdout.write(transliterate_string(data, m))
    return 0


if __name__ == "__main__":
    raise SystemExit(_main(sys.argv))
