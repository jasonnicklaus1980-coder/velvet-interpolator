#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "notes.h"
static int fails = 0;
#define CHECK(c, m) do { if (c) printf("  ok:   %s\n", m); else { printf("  FAIL: %s\n", m); fails++; } } while (0)

/* builds a monophonic line: each note is (midi, seconds); harmonics decay with 1/h, with a soft attack/release */
static float* make_line(const int* midi, const double* secs, int count, double sr, unsigned seed, double noise, size_t* out_n) {
    size_t total = 0; for (int i = 0; i < count; i++) total += (size_t)(secs[i] * sr);
    float* x = calloc(total, sizeof(float));
    size_t pos = 0; unsigned r = seed;
    for (int i = 0; i < count; i++) {
        double f0 = 440.0 * pow(2.0, (midi[i] - 69) / 12.0);
        size_t n = (size_t)(secs[i] * sr);
        for (size_t j = 0; j < n; j++) {
            double t = j / sr;
            double env = fmin(1.0, fmin(t / 0.01, (secs[i] - t) / 0.01));
            double v = 0;
            for (int h = 1; h <= 5; h++) v += sin(2 * M_PI * f0 * h * t) / h;
            x[pos + j] = (float)(0.3 * env * v);
            if (noise > 0) { r = r * 1103515245u + 12345u; x[pos + j] += (float)(noise * (((r >> 8) / 16777216.0) * 2 - 1)); }
        }
        pos += n;
    }
    *out_n = total;
    return x;
}

int main(void) {
    const double sr = 48000;
    /* A bass-like line in the 40..60 Hz to 300 Hz range, then a lead-like line */
    int bass[] = {36, 36, 39, 41, 43, 41};            /* C2 C2 Eb2 F2 G2 F2 */
    double bdur[] = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
    int lead[] = {64, 67, 69, 72, 71, 69};            /* E4 G4 A4 C5 B4 A4 */
    double ldur[] = {0.4, 0.4, 0.4, 0.4, 0.4, 0.4};
    vi_line_note notes[256];
    size_t n;

    printf("1. clean bass line\n");
    float* x = make_line(bass, bdur, 6, sr, 1, 0.0, &n);
    long k = vi_extract_notes(x, n, sr, 40, 400, 60, notes, 256);
    int ok = k == 6;
    for (int i = 0; ok && i < 6; i++) ok = notes[i].pitch == bass[i] && fabs(notes[i].start - i * 0.5) < 0.06;
    CHECK(ok, "6 bass notes with the right pitches and onsets (within 60 ms)");
    free(x);

    printf("2. clean lead line\n");
    x = make_line(lead, ldur, 6, sr, 2, 0.0, &n);
    k = vi_extract_notes(x, n, sr, 150, 1000, 60, notes, 256);
    ok = k == 6;
    for (int i = 0; ok && i < 6; i++) ok = notes[i].pitch == lead[i];
    CHECK(ok, "6 lead notes with the right pitches");
    free(x);

    printf("3. bass line with noise (about -30 dB)\n");
    x = make_line(bass, bdur, 6, sr, 3, 0.01, &n);
    k = vi_extract_notes(x, n, sr, 40, 400, 60, notes, 256);
    int right = 0;
    for (long i = 0; i < k && i < 6; i++) right += notes[i].pitch == bass[i];
    printf("  info: %ld notes found, %d with the correct pitch\n", k, right);
    CHECK(k >= 5 && right >= 5, "at least 5 of 6 bass notes correct under noise");
    free(x);

    printf("4. silence produces no notes\n");
    float* z = calloc((size_t)sr, sizeof(float));
    k = vi_extract_notes(z, (size_t)sr, sr, 40, 400, 60, notes, 256);
    CHECK(k == 0, "silence gives 0 notes");
    free(z);

    printf("5. bad input is rejected\n");
    CHECK(vi_extract_notes(NULL, 1000, sr, 40, 400, 60, notes, 256) == -1, "null buffer rejected");
    float tiny[100] = {0};
    CHECK(vi_extract_notes(tiny, 100, sr, 40, 400, 60, notes, 256) == -1, "input shorter than one analysis window rejected");

    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL NOTE TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
