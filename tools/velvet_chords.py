"""Command-line entry point: analyze a WAV file, print the chord segments, write a MIDI chord track."""
import argparse
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from chords import load_wav, analyze, NOTE_NAMES, QUALITIES  # noqa: E402
from smf import write_smf  # noqa: E402

ROOT_OCTAVE_MIDI = 48  # C3 for the lowest chord root


def chord_to_midi(label):
    if label == "N":
        return []
    # parse root (C or C#) and quality
    if len(label) > 1 and label[1] == "#":
        root_name, quality = label[:2], label[2:]
    else:
        root_name, quality = label[:1], label[1:]
    root = NOTE_NAMES.index(root_name)
    base = ROOT_OCTAVE_MIDI + root
    return [base + i for i in QUALITIES[quality]]


def main():
    ap = argparse.ArgumentParser(description="Velvet Interpolator companion: chord analysis to MIDI")
    ap.add_argument("wav")
    ap.add_argument("--midi", help="write a standard MIDI file with the chord track")
    ap.add_argument("--bpm", type=float, default=120.0)
    ap.add_argument("--pads", metavar="PREFIX", help="write PREFIX_pads.txt (pad sheet) and PREFIX_progression.mid")
    ap.add_argument("--switch-penalty", type=float, default=6.0)
    args = ap.parse_args()
    sr, x = load_wav(args.wav)
    _, _, segments = analyze(x, sr, switch_penalty=args.switch_penalty)
    print(f"{'start':>7} {'end':>7}  chord")
    for start, end, label in segments:
        print(f"{start:7.2f} {end:7.2f}  {label}")
    if args.pads:
        from pads import build_pads, write_pad_sheet, write_all_progressions
        table, bank_segs, overflow = build_pads(segments)
        write_pad_sheet(args.pads + "_pads.txt", table, overflow)
        files = write_all_progressions(args.pads, bank_segs, bpm=args.bpm)
        print(f"wrote {args.pads}_pads.txt, {len(table)} pads used, progression files: {', '.join(files) or 'none'}")
    if args.midi:
        notes = []
        for start, end, label in segments:
            for p in chord_to_midi(label):
                notes.append((start, end, p, 90))
        write_smf(args.midi, notes, bpm=args.bpm)
        print(f"wrote {args.midi} ({len(notes)} notes)")


if __name__ == "__main__":
    main()
