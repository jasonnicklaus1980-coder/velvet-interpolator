"""Minimal Standard MIDI File (format 0) writer and reader. No external MIDI library needed."""
import struct


def _vlq(n):
    out = [n & 0x7F]
    n >>= 7
    while n:
        out.insert(0, (n & 0x7F) | 0x80)
        n >>= 7
    return bytes(out)


def write_smf(path, notes, bpm=120.0, ppq=480):
    """notes: list of (start_seconds, end_seconds, midi_pitch, velocity)."""
    us_per_quarter = int(round(60_000_000 / bpm))
    events = []  # (tick, order, bytes); note-offs sort before note-ons at the same tick
    for start, end, pitch, vel in notes:
        on = int(round(start * bpm / 60.0 * ppq))
        off = max(on + 1, int(round(end * bpm / 60.0 * ppq)))
        events.append((on, 1, bytes([0x90, pitch, vel])))
        events.append((off, 0, bytes([0x80, pitch, 0])))
    events.sort(key=lambda e: (e[0], e[1]))
    track = bytearray()
    track += _vlq(0) + b"\xFF\x51\x03" + us_per_quarter.to_bytes(3, "big")
    last = 0
    for tick, _, data in events:
        track += _vlq(tick - last) + data
        last = tick
    track += _vlq(0) + b"\xFF\x2F\x00"
    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, ppq)
    with open(path, "wb") as f:
        f.write(header + b"MTrk" + struct.pack(">I", len(track)) + bytes(track))


def read_smf_notes(path):
    """Parse a format-0 file written by write_smf. Returns list of (start_ticks, end_ticks, pitch)."""
    data = open(path, "rb").read()
    assert data[:4] == b"MThd", "not a MIDI file"
    ppq = struct.unpack(">H", data[12:14])[0]
    assert data[14:18] == b"MTrk", "missing track chunk"
    length = struct.unpack(">I", data[18:22])[0]
    body = data[22:22 + length]
    i, tick, active, out = 0, 0, {}, []
    while i < len(body):
        delta, shift = 0, 0
        while True:  # variable-length delta
            b = body[i]; i += 1
            delta = (delta << 7) | (b & 0x7F)
            if not b & 0x80:
                break
        tick += delta
        status = body[i]; i += 1
        if status == 0xFF:  # meta
            mtype = body[i]; i += 1
            mlen = 0
            while True:
                b = body[i]; i += 1
                mlen = (mlen << 7) | (b & 0x7F)
                if not b & 0x80:
                    break
            i += mlen
            if mtype == 0x2F:
                break
        elif status & 0xF0 in (0x80, 0x90):
            pitch, vel = body[i], body[i + 1]; i += 2
            if status & 0xF0 == 0x90 and vel > 0:
                active.setdefault(pitch, []).append(tick)
            else:
                starts = active.get(pitch)
                if starts:
                    out.append((starts.pop(0), tick, pitch))
        else:
            raise ValueError(f"unsupported status byte 0x{status:02X}")
    return ppq, sorted(out)
