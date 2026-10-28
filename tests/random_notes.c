/* Measures note accuracy on random monophonic lines. Reports precision, recall, pitch accuracy and onset error. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "notes.h"
static unsigned rs = 12345;
static double rnd(void) { rs = rs * 1103515245u + 12345u; return ((rs >> 8) & 0xFFFFFF) / 16777216.0; }
int main(void) {
    const double sr = 48000;
    int lines = 40, hits = 0, truth_total = 0, found_total = 0, pitch_ok = 0;
    double onset_err_sum = 0;
    for (int L = 0; L < lines; L++) {
        int bass = L % 2 == 0;
        int count = 6 + (int)(rnd() * 5);
        int midi[16]; double secs[16]; double start[16];
        double t = 0;
        for (int i = 0; i < count; i++) {
            midi[i] = bass ? 36 + (int)(rnd() * 16) : 60 + (int)(rnd() * 20);
            secs[i] = 0.25 + rnd() * 0.35;
            start[i] = t; t += secs[i];
        }
        size_t total = (size_t)(t * sr);
        float* x = calloc(total, sizeof(float));
        size_t pos = 0;
        for (int i = 0; i < count; i++) {
            double f0 = 440.0 * pow(2.0, (midi[i] - 69) / 12.0);
            size_t n = (size_t)(secs[i] * sr);
            for (size_t j = 0; j < n && pos + j < total; j++) {
                double tt = j / sr;
                double env = fmin(1.0, fmin(tt / 0.01, (secs[i] - tt) / 0.01));
                double v = 0;
                for (int h = 1; h <= 5; h++) v += sin(2 * M_PI * f0 * h * tt) / h;
                x[pos + j] = (float)(0.3 * env * v);
            }
            pos += n;
        }
        vi_line_note* nt = malloc(sizeof(vi_line_note) * 256);
        long k = vi_extract_notes(x, total, sr, bass ? 40 : 150, bass ? 300 : 1000, 60, nt, 256);
        truth_total += count; found_total += (int)k;
        int used[16] = {0};
        for (long a = 0; a < k; a++) {
            for (int i = 0; i < count; i++) {
                if (used[i] || fabs(nt[a].start - start[i]) > 0.06) continue;
                used[i] = 1; hits++;
                onset_err_sum += fabs(nt[a].start - start[i]);
                if (nt[a].pitch == midi[i]) pitch_ok++;
                break;
            }
        }
        free(x); free(nt);
    }
    printf("random lines: %d, notes in truth %d, notes found %d\n", lines, truth_total, found_total);
    printf("precision (found notes that match a true note onset within 60 ms): %.1f%%\n", 100.0 * hits / (found_total ? found_total : 1));
    printf("recall (true notes matched): %.1f%%\n", 100.0 * hits / truth_total);
    printf("pitch correct among matched notes: %.1f%%\n", 100.0 * pitch_ok / (hits ? hits : 1));
    printf("mean onset error of matched notes: %.1f ms\n", 1000.0 * onset_err_sum / (hits ? hits : 1));
    return 0;
}
