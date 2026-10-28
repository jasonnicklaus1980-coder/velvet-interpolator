"""Analyses a real song or recording file (any format ffmpeg reads: MP3, M4A, OGG, FLAC, WAV ...).

Usage:
    python3 tools/analyze_song.py song.mp3 OUT_DIR [--answer answer.txt]

Writes MIDI files into OUT_DIR:
    chords.mid       the chord progression
    notes.mid        every polyphonic note found
    melody.mid       the melody, found after removing drums
If --answer is given (a file in the eval_real.py format), the chord score is printed too.
Real songs are much harder than the synthetic tests: expect lower accuracy, and check the output by ear.
"""
import os, subprocess, sys, shutil, argparse
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
sys.path.insert(0, HERE)
from chords import analyze, NOTE_NAMES  # noqa: E402
from pads import build_pads, write_pad_sheet  # noqa: E402
from smf import write_smf  # noqa: E402
from velvet_chords import chord_to_midi  # noqa: E402

SR = 44100
POLY = os.path.join(ROOT, "build", "poly_driver")


def decode(path):
    """Any ffmpeg-readable file -> mono float32 at SR."""
    if shutil.which("ffmpeg") is None:
        raise SystemExit("ffmpeg is needed to read this file (apt install ffmpeg)")
    cmd = ["ffmpeg", "-v", "error", "-i", path, "-ac", "1", "-ar", str(SR), "-f", "f32le", "-"]
    raw = subprocess.run(cmd, capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.float32).astype(np.float64)


def clean_segments(segments, min_chord_s=0.6):
    """Drops 'no chord' gaps, and merges chords shorter than min_chord_s into the chord before them.
    Stops the frame-level flicker from turning into dozens of one-frame chords."""
    out = []
    for a, b, name in segments:
        if name == "N":
            continue
        if out and (b - a) < min_chord_s and out[-1][2] != name:
            out[-1] = (out[-1][0], b, out[-1][2])          # absorb the short chord into the previous one
        elif out and out[-1][2] == name:
            out[-1] = (out[-1][0], b, name)                  # join repeats of the same chord
        else:
            out.append((a, b, name))
    return out


LINE = os.path.join(ROOT, "build", "line_driver")


def melody_notes(x):
    """Melody through drums: harmonic-percussive separation, then monophonic (YIN) note detection."""
    from melody import hpss
    if not os.path.exists(LINE):
        raise SystemExit("build the line driver first: gcc -O2 -Isrc tests/line_driver.c src/notes.c -lm -o build/line_driver")
    tmp = "/tmp/_song_melody.f32"
    hpss(x, SR).astype(np.float32).tofile(tmp)
    out = subprocess.run([LINE, tmp, str(SR)], capture_output=True, text=True, check=True).stdout
    return [(float(a), float(b), int(c)) for a, b, c in (l.split() for l in out.splitlines() if l.strip())]


def clean_notes(pn, min_s=0.2):
    """Drops very short notes, which are almost always analysis noise, and sorts by start time."""
    return sorted([(a, b, p) for a, b, p in pn if (b - a) >= min_s], key=lambda t: (t[0], t[2]))


def poly_notes(x):
    tmp = "/tmp/_song_poly.f32"
    x.astype(np.float32).tofile(tmp)
    if not os.path.exists(POLY):
        raise SystemExit("build the poly driver first: gcc -O2 -Isrc tests/poly_driver.c src/poly.c -lm -o build/poly_driver")
    out = subprocess.run([POLY, tmp, str(SR)], capture_output=True, text=True, check=True).stdout
    rows = [line.split() for line in out.splitlines() if line.strip()]
    return [(float(a), float(b), int(p)) for a, b, p in rows]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("song")
    ap.add_argument("outdir")
    ap.add_argument("--answer")
    args = ap.parse_args()
    os.makedirs(args.outdir, exist_ok=True)
    x = decode(args.song)
    print(f"decoded {len(x)/SR:.1f} s of audio")

    times, names, raw_segments = analyze(x, SR)
    segments = clean_segments(raw_segments)
    _, bank_segs = None, None
    table, bank_segs, overflow = build_pads(segments)

    notes = []
    for a, b, name in segments:
        for p in chord_to_midi(name):
            notes.append((a, b, p, 90))
    write_smf(os.path.join(args.outdir, "chords.mid"), notes, bpm=120.0)
    print(f"chords: {len(segments)} segments, {len(table)} distinct chords")

    pn = clean_notes(poly_notes(x))
    write_smf(os.path.join(args.outdir, "notes.mid"), [(a, b, p, 90) for a, b, p in pn], bpm=120.0)
    print(f"notes: {len(pn)} notes found")

    mel = melody_notes(x)
    write_smf(os.path.join(args.outdir, "melody.mid"), [(a, b, p, 100) for a, b, p in mel], bpm=120.0)
    print(f"melody: {len(mel)} notes found (drums suppressed)")

    if args.answer:
        import eval_real
        answer = eval_real.load_answer(args.answer)
        strict = equiv = scored = 0
        for t, name in zip(times, names):          # scored on the raw frames, before cleaning
            centre = t + 4096 / SR
            ref = eval_real.truth_at(answer, centre)
            if ref is None or eval_real.near_boundary(answer, centre):
                continue
            scored += 1
            strict += name == ref
            equiv += eval_real.pitch_classes(name) == eval_real.pitch_classes(ref)
        if scored:
            print(f"chord score vs answer: strict {strict/scored:.1%}, pitch-class {equiv/scored:.1%} ({scored} frames)")
    print(f"written to {args.outdir}")


if __name__ == "__main__":
    main()
