"""Drum-tolerant melody analysis: harmonic-percussive separation (HPSS) before note detection.

Method (Fitzgerald 2010, median filtering): sustained tones form horizontal lines in the spectrogram, drum hits
form vertical lines. A median filter along time keeps the harmonic part; along frequency, the percussive part.
A soft mask on the mixture's STFT then gives the harmonic (melodic) signal. The notes are detected on that signal.
"""
import numpy as np
from scipy.signal import stft, istft
from scipy.ndimage import median_filter


def hpss(x, sr, n_fft=2048, hop=512, kernel=17, power=2.0):
    """Returns the harmonic (sustained, melodic) part of x as a signal of the same length."""
    _, _, Z = stft(x, fs=sr, nperseg=n_fft, noverlap=n_fft - hop, boundary="zeros", padded=True)
    mag = np.abs(Z)
    H = median_filter(mag, size=(1, kernel))     # along time: sustained tones survive
    P = median_filter(mag, size=(kernel, 1))     # along frequency: drum hits survive
    mh = H ** power
    mp = P ** power
    mask = mh / (mh + mp + 1e-12)                # soft harmonic mask
    _, y = istft(Z * mask, fs=sr, nperseg=n_fft, noverlap=n_fft - hop, boundary=True)
    out = np.zeros_like(x)
    m = min(len(y), len(x))
    out[:m] = y[:m]                              # same padding as the forward transform: no time shift
    return out
