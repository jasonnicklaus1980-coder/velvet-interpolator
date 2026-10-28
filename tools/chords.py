"""Chord recognition prototype: chroma features, template matching, and Viterbi smoothing.

Limits (see README): this is a prototype for clean, single-chord-per-beat material. It is not a
polyphonic note transcriber. It uses no neural model.
"""
import numpy as np
import scipy.io.wavfile as wavfile
from scipy.signal import stft

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# quality name -> intervals above the root (semitones)
QUALITIES = {
    "": (0, 4, 7),          # major
    "m": (0, 3, 7),         # minor
    "dim": (0, 3, 6),
    "aug": (0, 4, 8),
    "sus2": (0, 2, 7),
    "sus4": (0, 5, 7),
    "7": (0, 4, 7, 10),
    "maj7": (0, 4, 7, 11),
    "m7": (0, 3, 7, 10),
}


def load_wav(path):
    sr, x = wavfile.read(path)
    if x.dtype == np.int16:
        x = x.astype(np.float64) / 32768.0
    elif x.dtype == np.int32:
        x = x.astype(np.float64) / 2147483648.0
    elif x.dtype == np.uint8:
        x = (x.astype(np.float64) - 128.0) / 128.0
    else:
        x = x.astype(np.float64)
    if x.ndim > 1:
        x = x.mean(axis=1)
    return sr, x


def chroma(x, sr, n_fft=8192, hop=2048, fmin=65.0, fmax=2100.0):
    """12-bin chroma per frame from STFT magnitudes, L2-normalised; also returns frame energy."""
    f, _, Z = stft(x, fs=sr, nperseg=n_fft, noverlap=n_fft - hop, window="hann", boundary=None, padded=False)
    power = np.abs(Z) ** 2
    keep = (f >= fmin) & (f <= fmax)
    f, power = f[keep], power[keep]
    midi = 69 + 12 * np.log2(f / 440.0)
    pcs = np.round(midi).astype(int) % 12
    out = np.zeros((12, power.shape[1]))
    for pc in range(12):
        out[pc] = power[pcs == pc].sum(axis=0)
    energy = out.sum(axis=0)
    norm = np.linalg.norm(out, axis=0)
    norm[norm == 0] = 1.0
    return out / norm, energy


def build_templates():
    labels, vecs = [], []
    for root in range(12):
        for q, ints in QUALITIES.items():
            v = np.zeros(12)
            for i in ints:
                v[(root + i) % 12] = 1.0
            vecs.append(v / np.linalg.norm(v))
            labels.append((root, q))
    return labels, np.array(vecs)


def label_str(state):
    root, q = state
    return NOTE_NAMES[root] + q


def viterbi(scores, switch_penalty=6.0):
    """scores: (n_states, n_frames) emission scores. Returns the best state path."""
    n_states, n_frames = scores.shape
    back = np.zeros((n_states, n_frames), dtype=int)
    dp = scores[:, 0].copy()
    for t in range(1, n_frames):
        best_prev = int(np.argmax(dp))
        stay = dp
        switch = dp[best_prev] - switch_penalty
        move_from = np.where(stay >= switch, np.arange(n_states), best_prev)
        dp = np.maximum(stay, switch) + scores[:, t]
        back[:, t] = move_from
    path = np.zeros(n_frames, dtype=int)
    path[-1] = int(np.argmax(dp))
    for t in range(n_frames - 1, 0, -1):
        path[t - 1] = back[path[t], t]
    return path


def analyze(x, sr, hop=2048, n_fft=8192, silence_rel=0.02, switch_penalty=6.0):
    # n_fft 8192 (5.4 Hz bins): 4096 put the low roots between bins and mislabelled them (see README).
    """Returns (frame_times, frame_labels, segments). Labels are strings, 'N' for silence/no chord."""
    C, energy = chroma(x, sr, n_fft=n_fft, hop=hop)
    labels, T = build_templates()
    sim = T @ C                      # (24, frames) cosine similarity
    scores = 10.0 * sim              # emission scores, sharpened
    no_chord = np.full((1, C.shape[1]), 0.5)   # silence/no-chord option
    emis = np.vstack([scores, no_chord])
    gate = energy < silence_rel * energy.max()
    emis[:, gate] = -1.0
    emis[-1, gate] = 2.0
    path = viterbi(emis, switch_penalty)
    names = [label_str(labels[s]) if s < len(labels) else "N" for s in path]
    times = np.arange(len(names)) * hop / sr
    segments = []
    start = 0
    for i in range(1, len(names) + 1):
        if i == len(names) or names[i] != names[start]:
            segments.append((times[start], times[i - 1] + hop / sr, names[start]))
            start = i
    return times, names, segments
