"""Scores the chord analyser on a real recording against a hand-written answer file.

Usage:
    python3 tests/eval_real.py recording.wav answer.txt

answer.txt has one line per chord: START_SECONDS END_SECONDS CHORD, for example:
    0.0 2.0 C
    2.0 4.0 G
    4.0 6.0 Am
    6.0 8.0 F
Chord names use the same spelling as the analyser: C, C#, Dm, G7, Fmaj7, Dsus4, Bdim, Eaug, Am7 ...

The score ignores frames within 0.2 s of a chord change, since the exact boundary is ambiguous.
Two scores are reported: strict (exact name) and pitch-class (same notes, so C+ = E+ = G#+ and C sus2 = G sus4).
"""
import os
import sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "companion"))
from chords import load_wav, analyze, NOTE_NAMES, QUALITIES  # noqa: E402

BOUNDARY_MARGIN = 0.2


def parse_name(name):
    if len(name) > 1 and name[1] == "#":
        return NOTE_NAMES.index(name[:2]), name[2:]
    return NOTE_NAMES.index(name[:1]), name[1:]


def pitch_classes(name):
    r, q = parse_name(name)
    return frozenset((r + i) % 12 for i in QUALITIES[q])


def load_answer(path):
    out = []
    for line in open(path):
        line = line.split("#")[0].strip()
        if not line:
            continue
        a, b, name = line.split(None, 2)
        out.append((float(a), float(b), name.strip()))
    return out


def truth_at(answer, t):
    for a, b, name in answer:
        if a <= t < b:
            return name
    return None


def near_boundary(answer, t):
    return any(abs(t - a) < BOUNDARY_MARGIN or abs(t - b) < BOUNDARY_MARGIN for a, b, _ in answer)


def main():
    if len(sys.argv) != 3:
        print(__doc__); return 2
    sr, x = load_wav(sys.argv[1])
    answer = load_answer(sys.argv[2])
    for _, _, name in answer:
        parse_name(name)   # fails early on a misspelt chord name
    times, names, _ = analyze(x, sr)
    strict = equiv = scored = 0
    for t, name in zip(times, names):
        centre = t + 4096 / sr
        ref = truth_at(answer, centre)
        if ref is None or near_boundary(answer, centre):
            continue
        scored += 1
        strict += name == ref
        equiv += pitch_classes(name) == pitch_classes(ref)
    if scored == 0:
        print("no frames scored: check the answer file covers the recording"); return 1
    print(f"frames scored: {scored}")
    print(f"strict (exact chord name):  {strict / scored:6.1%}")
    print(f"pitch-class (same notes):   {equiv / scored:6.1%}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
