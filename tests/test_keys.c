#include <stdio.h>
#include <string.h>
#include "keys.h"
static int fails = 0;
#define CHECK(c, m) do { if (c) printf("  ok:   %s\n", m); else { printf("  FAIL: %s\n", m); fails++; } } while (0)
/* Builds a histogram from chord tones: each chord's notes weighted equally, as the plugin does. */
static void add_chord(double* h, int root, int minor) {
    int tones[3] = {0, minor ? 3 : 4, 7};
    for (int i = 0; i < 3; i++) h[(root + tones[i]) % 12] += 1.0;
}
int main(void) {
    double h[12];
    double conf;
    /* C major: C, F, G, Am (I IV V vi) */
    memset(h, 0, sizeof h);
    add_chord(h, 0, 0); add_chord(h, 5, 0); add_chord(h, 7, 0); add_chord(h, 9, 1);
    add_chord(h, 0, 0); add_chord(h, 7, 0);
    int k = vi_estimate_key(h, &conf);
    CHECK(k == 0, "C, F, G, Am, C, G estimates C major");
    printf("        -> %s, confidence %.2f\n", vi_key_name(k), conf);
    /* A minor: Am, Dm, E, F (i iv V VI) */
    memset(h, 0, sizeof h);
    add_chord(h, 9, 1); add_chord(h, 2, 1); add_chord(h, 4, 0); add_chord(h, 5, 0);
    add_chord(h, 9, 1); add_chord(h, 4, 0);
    k = vi_estimate_key(h, &conf);
    CHECK(k == 12 + 9, "Am, Dm, E, F, Am, E estimates A minor");
    printf("        -> %s, confidence %.2f\n", vi_key_name(k), conf);
    /* G major chords only: G, C, D, Em */
    memset(h, 0, sizeof h);
    add_chord(h, 7, 0); add_chord(h, 0, 0); add_chord(h, 2, 0); add_chord(h, 4, 1);
    add_chord(h, 7, 0); add_chord(h, 2, 0);
    k = vi_estimate_key(h, &conf);
    CHECK(k == 7, "G, C, D, Em, G, D estimates G major");
    printf("        -> %s, confidence %.2f\n", vi_key_name(k), conf);
    /* an empty histogram gives zero confidence, not a fake key */
    memset(h, 0, sizeof h);
    vi_estimate_key(h, &conf);
    CHECK(conf == 0.0, "no chords gives zero confidence");
    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL KEY TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
