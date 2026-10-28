#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "chordcore.h"
static int fails = 0;
#define CHECK(c, m) do { if (c) printf("  ok:   %s\n", m); else { printf("  FAIL: %s\n", m); fails++; } } while (0)

static float* song(size_t* n_out, double sr, unsigned seed) {
    int chords[] = {0, 7, 9, 5, 0, 4, 2, 10};             /* roots; quality cycles maj/min/7 */
    int quals[] = {0, 1, 6, 0, 1, 0, 2, 7};
    size_t per = (size_t)(1.0 * sr), n = per * 8;
    float* x = calloc(n, sizeof(float));
    unsigned r = seed;
    for (int c = 0; c < 8; c++) {
        const int* iv = cc_quality_int[quals[c]];
        for (int k = 0; k < cc_quality_len[quals[c]]; k++) {
            double f0 = 440.0 * pow(2.0, ((48 + chords[c] + iv[k]) - 69) / 12.0);
            for (int h = 1; h <= 4; h++)
                for (size_t i = 0; i < per; i++) {
                    double t = i / sr, env = fmin(1.0, fmin(t / 0.02, (1.0 - t) / 0.02));
                    x[c * per + i] += (float)(0.25 / h * env * sin(2 * M_PI * f0 * h * t));
                }
        }
    }
    for (size_t i = 0; i < n; i++) { r = r * 1103515245u + 12345u; x[i] += (float)(0.002 * (((r >> 8) & 0xFFFF) / 65536.0 - 0.5)); }
    *n_out = n;
    return x;
}

int main(void) {
    const double sr = 44100;
    size_t n;
    float* x = song(&n, sr, 9);
    /* offline reference */
    size_t cap = n / CC_HOP + 8;
    cc_segment* seg = malloc(sizeof(cc_segment) * cap);
    size_t count = 0;
    long frames = cc_analyze(x, n, sr, seg, cap, &count);
    int* off = malloc(sizeof(int) * frames);
    for (size_t i = 0; i < count; i++) for (size_t f = seg[i].start_frame; f <= seg[i].end_frame; f++) off[f] = seg[i].state;
    /* streamed in random block sizes, flushed at the end */
    for (int trial = 0; trial < 3; trial++) {
        cc_stream st;
        CHECK(cc_stream_init(&st, sr) == 0, "stream initialised");
        unsigned r = 77 + trial;
        size_t pos = 0;
        while (pos < n) {
            r = r * 1103515245u + 12345u;
            size_t block = 64 + (r >> 8) % 3000;
            if (pos + block > n) block = n - pos;
            CHECK(cc_stream_push(&st, x + pos, block) == 0, "push ok") ;
            pos += block;
        }
        cc_stream_flush(&st);
        size_t same = 0, total = 0;
        for (size_t f = 0; f < st.ndecided && f < (size_t)frames; f++) {
            total++;
            same += st.decided[f] == off[f];
        }
        char msg[160];
        snprintf(msg, sizeof msg, "trial %d: %zu frames decided (offline %ld), %zu/%zu agree with offline",
                 trial, st.ndecided, frames, same, total);
        CHECK(st.ndecided == (size_t)frames && same == total, msg);
        cc_stream_free(&st);
    }
    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL STREAM TESTS PASSED", fails, fails == 1 ? "" : "s");
    free(x); free(seg); free(off);
    return fails ? 1 : 0;
}
