"""Turns a chord analysis into MPC pad assignments across up to 64 pads (banks A-D, pads 01-16).

Each distinct chord gets its own pad, in order of first appearance. The MPC has four banks of 16 pads.
Within each bank, pad 01..16 sends MIDI notes 36..51 (the default layout, to verify on the unit). Bank
choice is not in the MIDI data, so each bank gets its own progression file, and you switch banks on the MPC.

Verify on the MPC: the note numbers per pad, and that the bank buttons select A..D as expected.
"""
from smf import write_smf
from velvet_chords import chord_to_midi

PAD_BASE_NOTE = 36
PADS_PER_BANK = 16
BANKS = "ABCD"
MAX_PADS = PADS_PER_BANK * len(BANKS)


def pad_name(index):
    bank = BANKS[index // PADS_PER_BANK]
    return f"{bank}{index % PADS_PER_BANK + 1:02d}"


def build_pads(segments):
    """segments: list of (start, end, chord_label). Returns (table, bank_segments, overflow).

    table: one row per assigned chord with pad name, bank, MIDI note, and chord notes.
    bank_segments: {bank letter: [(start, end, midi_note), ...]} in song order.
    overflow: chords that did not fit in 64 pads.
    """
    order = []
    for _, _, label in segments:
        if label != "N" and label not in order:
            order.append(label)
    assigned = order[:MAX_PADS]
    overflow = order[MAX_PADS:]
    index_of = {c: i for i, c in enumerate(assigned)}
    table = []
    for c in assigned:
        i = index_of[c]
        table.append({"pad": pad_name(i), "bank": BANKS[i // PADS_PER_BANK],
                      "note": PAD_BASE_NOTE + i % PADS_PER_BANK,
                      "chord": c, "chord_notes": chord_to_midi(c)})
    bank_segments = {b: [] for b in BANKS}
    for s, e, label in segments:
        if label in index_of:
            i = index_of[label]
            bank_segments[BANKS[i // PADS_PER_BANK]].append((s, e, PAD_BASE_NOTE + i % PADS_PER_BANK))
    return table, bank_segments, overflow


def write_pad_sheet(path, table, overflow):
    with open(path, "w") as f:
        f.write("pad   note  chord    chord notes (MIDI)\n")
        for row in table:
            notes = ", ".join(str(n) for n in row["chord_notes"])
            f.write(f"{row['pad']}   {row['note']:4d}  {row['chord']:<8} {notes}\n")
        if overflow:
            f.write(f"\nNot assigned (more than {MAX_PADS} distinct chords): {', '.join(overflow)}\n")


def write_progression_midi(path, bank_segments, bank, bpm=120.0):
    segs = bank_segments.get(bank, [])
    write_smf(path, [(s, e, note, 100) for s, e, note in segs], bpm=bpm)


def write_all_progressions(prefix, bank_segments, bpm=120.0):
    """Writes one progression file per bank that has any chords. Returns the file names written."""
    written = []
    for bank in BANKS:
        if bank_segments.get(bank):
            name = f"{prefix}_progression_{bank}.mid"
            write_progression_midi(name, bank_segments, bank, bpm=bpm)
            written.append(name)
    return written
