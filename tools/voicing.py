"""Interpolation controls for chord voicings: transposition, inversion, and spread.

These change how a chord is voiced when it's written to the pad sheet and to pads. They never change
the analysed chord itself.
"""
from velvet_chords import chord_to_midi


def voice(label, transpose=0, inversion=0, spread=False, bass_octave_shift=0):
    """MIDI notes for a chord with the chosen voicing.

    transpose: semitones, any integer (applied to every note).
    inversion: 0 = root position, 1 = first inversion, 2 = second, ... (lowest note moves to the top
               one octave up, repeatedly).
    spread:    True moves the second and fourth chord tones up an octave (open voicing).
    bass_octave_shift: octaves for the whole chord after the above (e.g. -1 to lower it).
    """
    notes = sorted(chord_to_midi(label))
    if not notes:
        return []
    for _ in range(inversion % len(notes)):
        notes.append(notes.pop(0) + 12)
    if spread and len(notes) >= 3:
        for i in (1, 3):
            if i < len(notes):
                notes[i] += 12
        notes.sort()
    shift = transpose + 12 * bass_octave_shift
    out = [n + shift for n in notes]
    if any(n < 0 or n > 127 for n in out):
        raise ValueError(f"voicing of {label} leaves the MIDI range: {out}")
    return out
