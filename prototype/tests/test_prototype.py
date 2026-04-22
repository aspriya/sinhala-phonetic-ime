"""Pytest suite for the Python reference transliterator.

Covers:
- The 10 worked examples from docs/phonetic-mapping-design.md.
- The 15-entry TC matrix from the design doc.
- A 70+ word corpus in word_corpus.txt.
- Edge cases (empty, backspace, reset, conjuncts, mid-composition state).
"""
from __future__ import annotations

import random
from pathlib import Path

import pytest

from prototype.transliterate import (
    Mapping,
    State,
    Transliterator,
    transliterate_string,
    shift_key,
)


HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
MAPPING_PATH = ROOT / "mapping" / "phonetic-default.json"
CORPUS_PATH = HERE / "word_corpus.txt"


@pytest.fixture(scope="module")
def mapping() -> Mapping:
    return Mapping.from_file(MAPPING_PATH)


# -----------------------------------------------------------------------------
# Worked examples from the design doc.
# -----------------------------------------------------------------------------
WORKED_EXAMPLES = [
    ("ammaa",           "අම්මා"),
    ("ashanx",          "අශන්"),
    ("kaawx",           "කාව්"),
    ("siri",            "සිරි"),
    ("gedhara",         "ගෙදර"),
    ("magee",           "මගේ"),
    ("ispirithaalaya",  "ඉස්පිරිතාලය"),
    ("sakkaraa",        "සක්කරා"),
    ("nagaraya",        "නගරය"),
    ("laqkaawa",        "ලංකාව"),
]


@pytest.mark.parametrize("inp,expected", WORKED_EXAMPLES)
def test_worked_examples(mapping: Mapping, inp: str, expected: str):
    assert transliterate_string(inp, mapping) == expected


# -----------------------------------------------------------------------------
# TC-matrix from design doc §5.
# -----------------------------------------------------------------------------
def test_tc01_k_preview(mapping: Mapping):
    t = Transliterator(mapping)
    committed = t.feed("k")
    assert committed == ""
    assert t.composition() == "ක"


def test_tc02_k_then_x(mapping: Mapping):
    t = Transliterator(mapping)
    committed = t.feed("k") + t.feed("x") + t.commit()
    assert committed == "ක්"


def test_tc03_ka(mapping: Mapping):
    assert transliterate_string("ka", mapping) == "ක"


def test_tc04_kaa(mapping: Mapping):
    assert transliterate_string("kaa", mapping) == "කා"


def test_tc05_ammaa(mapping: Mapping):
    assert transliterate_string("ammaa", mapping) == "අම්මා"


def test_tc06_ashanx(mapping: Mapping):
    assert transliterate_string("ashanx", mapping) == "අශන්"


def test_tc07_Shift_k_a(mapping: Mapping):
    t = Transliterator(mapping)
    out = t.feed_shifted("k") + t.feed("a") + t.commit()
    assert out == "ඛ"


def test_tc08_backspace_removes_matra(mapping: Mapping):
    t = Transliterator(mapping)
    t.feed("k"); t.feed("a")
    assert t.state is State.PARTIAL_VOWEL
    t.feed_backspace()
    assert t.state is State.CONSONANT_PENDING
    assert t.composition() == "ක"


def test_tc09_double_backspace_resets(mapping: Mapping):
    t = Transliterator(mapping)
    t.feed("k"); t.feed("a")
    t.feed_backspace(); t.feed_backspace()
    assert t.state is State.IDLE
    assert t.composition() == ""


def test_tc10_tha(mapping: Mapping):
    assert transliterate_string("tha", mapping) == "ත"


def test_tc11_tx(mapping: Mapping):
    assert transliterate_string("tx", mapping) == "ට්"


def test_tc12_laqkaawa(mapping: Mapping):
    assert transliterate_string("laqkaawa", mapping) == "ලංකාව"


def test_tc13_stress_no_crash(mapping: Mapping):
    random.seed(42)
    chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ ^01234567890.,?"
    blob = "".join(random.choices(chars, k=500))
    # Should never raise.
    _ = transliterate_string(blob, mapping)


def test_tc14_empty(mapping: Mapping):
    t = Transliterator(mapping)
    assert t.commit() == ""


def test_tc15_reset(mapping: Mapping):
    t = Transliterator(mapping)
    t.feed("k"); t.feed("a")
    t.reset()
    assert t.state is State.IDLE
    assert t.composition() == ""
    assert t.commit() == ""


# -----------------------------------------------------------------------------
# Additional edge cases.
# -----------------------------------------------------------------------------
def test_unmapped_passthrough(mapping: Mapping):
    # Digits and punctuation pass through verbatim and act as soft terminators.
    assert transliterate_string("123", mapping) == "123"
    assert transliterate_string("ka, ", mapping) == "ක, "


def test_consonant_then_unmapped_soft_commit(mapping: Mapping):
    # 'k' followed by space -> ක (no virama; soft terminator).
    assert transliterate_string("k ", mapping) == "ක "


def test_anusvara_alone(mapping: Mapping):
    assert transliterate_string("q", mapping) == "ං"


def test_visarga_alone(mapping: Mapping):
    assert transliterate_string("Q", mapping) == "ඃ"


def test_conjunct_marker(mapping: Mapping):
    # kXk -> ක + virama + ZWJ + ක (touching conjunct).
    out = transliterate_string("kXk", mapping)
    assert out == "\u0D9A\u0DCA\u200D\u0D9A"


def test_shifted_consonant_prenasalised(mapping: Mapping):
    # Prenasalised g -> ඟ (U+0D9F) via "^g".
    assert transliterate_string("^ga", mapping) == "ඟ"


def test_mid_composition_state_consistency(mapping: Mapping):
    t = Transliterator(mapping)
    assert t.feed("t") == ""
    assert t.state is State.PARTIAL_CONSONANT
    assert t.feed("h") == ""
    assert t.state is State.CONSONANT_PENDING
    assert t.feed("a") == ""
    assert t.state is State.PARTIAL_VOWEL
    # commit() resolves the pending matra (vowel 'a' -> no matra).
    assert t.commit() == "ත"


def test_backspace_on_partial_consonant(mapping: Mapping):
    t = Transliterator(mapping)
    t.feed("t")
    assert t.state is State.PARTIAL_CONSONANT
    t.feed_backspace()
    assert t.state is State.IDLE


def test_all_vowel_word(mapping: Mapping):
    # Standalone vowels only.
    assert transliterate_string("au", mapping) == "ඖ"
    assert transliterate_string("ai", mapping) == "ඓ"


def test_version_present(mapping: Mapping):
    assert mapping.version


# -----------------------------------------------------------------------------
# Corpus.
# -----------------------------------------------------------------------------
def _load_corpus():
    pairs = []
    for line in CORPUS_PATH.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        inp, want = line.split("\t")
        pairs.append((inp, want))
    return pairs


CORPUS = _load_corpus()


@pytest.mark.parametrize("inp,expected", CORPUS, ids=[p[0] for p in CORPUS])
def test_corpus(mapping: Mapping, inp: str, expected: str):
    assert transliterate_string(inp, mapping) == expected


def test_corpus_size():
    # The design doc requires at least 50 corpus words; we target > 50.
    assert len(CORPUS) >= 50, f"corpus too small: {len(CORPUS)}"
