#include <stdio.h>
#include <string.h>
#include "musicout.h"
#include "chordcore.h"
static int fails = 0;
#define CHECK(c, msg) do { if (c) printf("  ok:   %s\n", msg); else { printf("  FAIL: %s\n", msg); fails++; } } while (0)
static int state_of(int root, int q) { return root * CC_NQUAL + q; }
int main(void) {
    int out[8];
    int n = vi_voice(state_of(0, 0), 0, 0, 0, out);          /* C major */
    CHECK(n == 3 && out[0] == 48 && out[1] == 52 && out[2] == 55, "C major root position is 48 52 55");
    n = vi_voice(state_of(0, 6), 0, 0, 0, out);              /* C7 */
    CHECK(n == 4 && out[3] == 58, "C7 is 48 52 55 58");
    n = vi_voice(state_of(0, 0), 0, 1, 0, out);              /* first inversion */
    CHECK(n == 3 && out[0] == 52 && out[2] == 60, "C major first inversion is 52 55 60");
    n = vi_voice(state_of(0, 0), 0, 0, 1, out);              /* open triad */
    CHECK(n == 3 && out[0] == 48 && out[1] == 55 && out[2] == 64, "C major open voicing is 48 55 64");
    n = vi_voice(state_of(0, 0), 5, 0, 0, out);
    CHECK(n == 3 && out[0] == 53, "transpose +5 moves C to F");
    CHECK(vi_voice(state_of(7, 0), 80, 0, 0, out) == -1, "out-of-range transposition is rejected");
    CHECK(vi_voice(999, 0, 0, 0, out) == -1, "invalid chord state is rejected");
    /* MIDI file: two notes, 120 bpm, check the bytes of the header and the first note */
    vi_note notes[2] = {{0.0, 1.0, 48, 90}, {1.0, 2.0, 50, 90}};
    CHECK(vi_write_smf("/tmp/vi_test.mid", notes, 2, 120.0) == 0, "MIDI file written");
    FILE* f = fopen("/tmp/vi_test.mid", "rb"); unsigned char b[14];
    CHECK(f && fread(b, 1, 14, f) == 14 && !memcmp(b, "MThd", 4) && b[12] == 1 && b[13] == 0xE0, "header is MThd with ppq 480");
    if (f) fclose(f);
    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL C MUSIC TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
