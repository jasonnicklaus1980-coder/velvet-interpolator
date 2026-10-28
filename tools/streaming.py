"""Streaming (real-time) chord analysis.

Audio is pushed in blocks of any size. A frame is analysed as soon as enough samples have arrived
(n_fft). Decisions use a fixed-lag Viterbi: each chord is committed `lag` frames after it was
analysed, so the output lags the input by about (n_fft + lag * hop) / sr seconds.
"""
import numpy as np
from scipy.signal import get_window
from chords import build_templates, label_str, viterbi  # noqa: F401  (viterbi kept for reference)


class ChordStream:
    def __init__(self, sr, n_fft=8192, hop=2048, lag=4, switch_penalty=6.0, fmin=65.0, fmax=2100.0):
        self.sr, self.n_fft, self.hop, self.lag = sr, n_fft, hop, lag
        self.switch_penalty = switch_penalty
        self.win = get_window("hann", n_fft)
        f = np.fft.rfftfreq(n_fft, 1.0 / sr)
        keep = (f >= fmin) & (f <= fmax)
        self.bins = np.where(keep)[0]
        midi = 69 + 12 * np.log2(f[keep] / 440.0)
        self.pc = np.round(midi).astype(int) % 12
        self.labels, self.T = build_templates()
        self.buf = np.zeros(0)
        self.frames_done = 0          # frames analysed so far
        self.max_energy = 1e-12
        # fixed-lag Viterbi state
        self.dp = None
        self.back = []                # backpointers for frames not yet committed
        self.committed = 0            # frames committed so far
        self.out = []                 # (frame_index, label) decisions, in order

    def _chroma_frame(self, frame):
        spec = np.abs(np.fft.rfft(frame * self.win)) ** 2
        p = spec[self.bins]
        c = np.zeros(12)
        np.add.at(c, self.pc, p)
        energy = c.sum()
        norm = np.linalg.norm(c)
        return (c / norm if norm > 0 else c), energy

    def _emission(self, frame):
        c, energy = self._chroma_frame(frame)
        self.max_energy = max(self.max_energy, energy)
        em = np.empty(len(self.labels) + 1)
        em[:-1] = 10.0 * (self.T @ c)
        if energy < 0.02 * self.max_energy:
            em[:-1] = -1.0
            em[-1] = 2.0
        else:
            em[-1] = 0.5
        return em

    def _step(self, em):
        n = em.shape[0]
        if self.dp is None:
            self.dp = em.copy()
            self.back.append(np.arange(n))
            return
        best = int(np.argmax(self.dp))
        stay = self.dp
        switch = self.dp[best] - self.switch_penalty
        move_from = np.where(stay >= switch, np.arange(n), best)
        self.dp = np.maximum(stay, switch) + em
        self.back.append(move_from)

    def _commit(self, final=False):
        # commit frames that are at least `lag` behind the newest analysed frame
        while len(self.back) > (0 if final else self.lag):
            # trace back from the current best state to the oldest uncommitted frame
            state = int(np.argmax(self.dp))
            path = [state]
            for bp in reversed(self.back[1:]):
                state = int(bp[state])
                path.append(state)
            path.reverse()        # path[i] = state at uncommitted frame i
            idx = self.committed
            s = path[0]
            name = label_str(self.labels[s]) if s < len(self.labels) else "N"
            self.out.append((idx, name))
            self.committed += 1
            self.back.pop(0)
            # shift dp so it stays consistent after dropping the oldest frame
            # (dp already describes the newest frame, so nothing to change)

    def push(self, block):
        self.buf = np.concatenate([self.buf, np.asarray(block, dtype=float)])
        new = []
        while len(self.buf) >= self.n_fft:
            frame = self.buf[: self.n_fft]
            self._step(self._emission(frame))
            self.frames_done += 1
            self.buf = self.buf[self.hop:]
            before = len(self.out)
            self._commit()
            new.extend(self.out[before:])
        return new

    def flush(self):
        before = len(self.out)
        self._commit(final=True)
        return self.out[before:]

    def latency_seconds(self):
        return (self.n_fft + self.lag * self.hop) / self.sr
