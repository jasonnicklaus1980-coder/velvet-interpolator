"""Checks melody_pads.mid against melody.mid: same number of notes, same timing, and every pad note inside the
16-pad window. usage: check_pads.py melody.mid melody_pads.mid window_low"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools"))
from smf import read_smf_notes

mel_path, pads_path, low = sys.argv[1], sys.argv[2], int(sys.argv[3])
_, mel = read_smf_notes(mel_path)
_, pads = read_smf_notes(pads_path)
ok = len(mel) == len(pads) and len(pads) > 0
ok = ok and all(low <= p <= low + 15 for _, _, p in pads)
ok = ok and all(abs(a[0] - b[0]) < 0.01 and abs(a[1] - b[1]) < 0.02 for a, b in zip(mel, pads))
print(("ok:   " if ok else "FAIL: ") + f"melody_pads has {len(pads)} notes, all within pads {low}-{low + 15}, timing kept")
sys.exit(0 if ok else 1)
