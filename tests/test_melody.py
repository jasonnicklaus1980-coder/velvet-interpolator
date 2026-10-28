"""Melody through drums: a synthetic melody with synthetic drums at several levels. Compares note F1 on the
raw mix with F1 after harmonic-percussive separation. Synthetic only."""
import os, sys, subprocess
import numpy as np
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
sys.path.insert(0, os.path.join(ROOT, "tools"))
from melody import hpss

SR = 44100
POLY = os.path.join(ROOT, "build", "poly_driver")
rng = np.random.default_rng(5)

def melody(seconds=8.0):
    """A lead line: notes of 0.4 to 0.7 s, pitched 60..79, with a soft attack and harmonic partials."""
    t_list, x = [], np.zeros(int(seconds * SR))
    t = 0.2
    notes = []
    while t < seconds - 0.8:
        d = rng.uniform(0.4, 0.7)
        p = int(rng.integers(60, 80))
        notes.append((t, min(t + d, seconds - 0.1), p))
        t += d + rng.uniform(0.05, 0.2)
    for a, b, p in notes:
        f0 = 440 * 2 ** ((p - 69) / 12)
        i0, i1 = int(a * SR), int(b * SR)
        tt = np.arange(i1 - i0) / SR
        env = np.minimum(1, np.minimum(tt / 0.02, (b - a - tt) / 0.05))
        v = sum(np.sin(2 * np.pi * f0 * h * tt) / h for h in range(1, 5))
        x[i0:i1] += 0.2 * env * v
    return x, notes

def drums(n_samples, level=1.0):
    """Kick (pitch-dropping sine), snare (noise burst) and hats (short high noise), every beat at 110 bpm."""
    y = np.zeros(n_samples)
    beat = 60.0 / 110.0
    k = 0
    while k * beat < n_samples / SR:
        i = int(k * beat * SR)
        if k % 2 == 0:                                   # kick
            L = int(0.15 * SR); tt = np.arange(L) / SR
            f = 120 * np.exp(-tt * 25) + 45
            kick = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-tt * 18)
            y[i:i + L] += kick[:len(y[i:i + L])]
        else:                                            # snare
            L = int(0.12 * SR); tt = np.arange(L) / SR
            y[i:i + L] += 0.7 * rng.standard_normal(L) * np.exp(-tt * 30)
        for h in range(2):                               # hats
            j = i + int(h * beat / 2 * SR)
            L = int(0.04 * SR); tt = np.arange(L) / SR
            if j + L < len(y):
                y[j:j + L] += 0.3 * rng.standard_normal(L) * np.exp(-tt * 90)
        k += 1
    return level * y

def notes_of(x):
    p = os.path.join("/tmp", "_mel.f32")
    x.astype(np.float32).tofile(p)
    out = subprocess.run([POLY, p, str(SR)], capture_output=True, text=True, check=True).stdout
    return [(float(a), float(b), int(c)) for a, b, c in (l.split() for l in out.splitlines() if l.strip())]

def f1(found, truth, tol=0.06):
    used = [False] * len(found); tp = 0
    for ts, te, tp_ in truth:
        for i, (fs, fe, fp) in enumerate(found):
            if not used[i] and fp == tp_ and abs(fs - ts) < tol:
                used[i] = True; tp += 1; break
    prec = tp / max(1, len(found)); rec = tp / max(1, len(truth))
    return 2 * prec * rec / max(1e-9, prec + rec), prec, rec

if __name__ == "__main__":
    mel, truth_notes = melody()
    truth = truth_notes
    dr = drums(len(mel))
    print("drum level (dB relative to melody) | raw mix F1 | after HPSS F1 (precision, recall)")
    for gain_db in (-6, 0, 6):
        g = 10 ** (gain_db / 20) * np.sqrt(np.mean(mel ** 2) / np.mean(dr ** 2))
        mix = mel + g * dr
        raw = f1(notes_of(mix), truth)
        clean = f1(notes_of(hpss(mix, SR)), truth)
        print(f"{gain_db:+4d} dB                  | {raw[0]*100:5.1f}%     | {clean[0]*100:5.1f}% (P {clean[1]*100:.0f}%, R {clean[2]*100:.0f}%)")
