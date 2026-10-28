/* Checks the pad MIDI event generator: every note-on and note-off appears exactly once across all blocks,
   at the right frame offset, with the right status bytes. */
#include <stdio.h>
#include <string.h>
#include "padsend.h"

int main(void) {
    const double sr = 48000.0;
    const ps_note sched[] = {{0.00, 0.50, 36, 0, 100}, {0.25, 1.00, 51, 1, 90}, {0.9999, 1.2, 40, 2, 80}};
    const size_t n = sizeof sched / sizeof sched[0];
    const int block = 512;
    int fails = 0;
    size_t total = 0, ons = 0, offs = 0;
    ps_MidiEvent ev[64];
    for (long long b = 0; b < (long long)(1.5 * sr); b += block) {
        size_t m = ps_block_events(sched, n, b, block, sr, ev, 64);
        for (size_t k = 0; k < m; k++) {
            int status = (unsigned char)ev[k].midiData[0] & 0xF0;   /* message type; the channel is in the low nibble */
            if (ev[k].deltaFrames < 0 || ev[k].deltaFrames >= block) { fails++; printf("FAIL: delta %d out of block\n", ev[k].deltaFrames); }
            if (ev[k].type != ps_kVstMidiType || ev[k].byteSize != (int)sizeof ev[k]) { fails++; printf("FAIL: bad event header\n"); }
            if (status == 0x90) ons++; else if (status == 0x80) offs++; else { fails++; printf("FAIL: status %x\n", status); }
            total++;
        }
    }
    printf("events: %zu (on %zu, off %zu)\n", total, ons, offs);
    if (ons != n || offs != n) { fails++; printf("FAIL: expected %zu on and %zu off\n", n, n); }

    /* the first note starts at sample 0 and must be on the first block at offset 0 */
    ps_MidiEvent first[4];
    size_t m = ps_block_events(sched, n, 0, block, sr, first, 4);
    if (m < 1 || first[0].deltaFrames != 0 || (unsigned char)first[0].midiData[1] != 36) { fails++; printf("FAIL: first note wrong\n"); }

    /* a note exactly on a block boundary belongs to the block that starts there, once */
    ps_note edge[] = {{512.0 / sr, 1000.0, 60, 0, 100}};
    size_t a = ps_block_events(edge, 1, 0, block, sr, first, 4);
    size_t c = ps_block_events(edge, 1, block, block, sr, first, 4);
    if (a != 0 || c < 1 || first[0].deltaFrames != 0) { fails++; printf("FAIL: boundary note not in the next block\n"); }

    /* stopping sends a note-off per note, each on its own channel */
    ps_MidiEvent allOff[8];
    size_t oc = ps_all_off(sched, n, allOff, 8);
    if (oc != n || (unsigned char)allOff[1].midiData[0] != 0x81 || allOff[1].deltaFrames != 0) { fails++; printf("FAIL: all-off\n"); }
    printf(fails ? "PAD EVENT TESTS FAILED (%d)\n" : "PAD EVENT TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
