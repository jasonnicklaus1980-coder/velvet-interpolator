/* Polyphonic transcription on synthetic music with known notes. Reports precision, recall and F1
   (onset within 50 ms, same pitch). Synthetic harmonic tones only: real recordings will score lower. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "poly.h"
static unsigned rs = 2024;
static double partial_decay = 1.0;   /* amplitude of harmonic h is h^-decay */
static double rnd(void) { rs = rs * 1103515245u + 12345u; return ((rs >> 8) & 0xFFFFFF) / 16777216.0; }
typedef struct { double start, end; int pitch; } truth_t;

static float* make_mix(truth_t* t, int nt, double sr, size_t n) {
    float* x = calloc(n, sizeof(float));
    for (int k = 0; k < nt; k++) {
        double f0 = 440.0 * pow(2.0, (t[k].pitch - 69) / 12.0);
        size_t a = (size_t)(t[k].start * sr), b = (size_t)(t[k].end * sr);
        for (size_t i = a; i < b && i < n; i++) {
            double tt = (i - a) / sr, dur = (b - a) / sr;
            double env = fmin(1.0, fmin(tt / 0.01, (dur - tt) / 0.03));
            double v = 0;
            for (int h = 1; h <= 5; h++) v += sin(2 * M_PI * f0 * h * tt) / pow(h, partial_decay);
            x[i] += (float)(0.12 * env * v);
        }
    }
    return x;
}

int main(int argc, char** argv) {
    const double sr = 48000;
    int pieces = 12, voices_max = 4;
    unsigned seed_arg = argc > 1 ? (unsigned)atoi(argv[1]) : 2024;
    rs = seed_arg;
    if (argc > 2) voices_max = atoi(argv[2]);
    if (argc > 3) partial_decay = atof(argv[3]);
    long tp = 0, fp = 0, fn = 0;
    for (int pc = 0; pc < pieces; pc++) {
        int voices = 2 + (pc % (voices_max - 1));   /* 2..voices_max voices */
        int nt = 0; truth_t t[256];
        double len = 6.0;
        for (int v = 0; v < voices; v++) {
            double tt = rnd() * 0.5;
            while (tt < len - 0.5) {
                double dur = 0.4 + rnd() * 0.8;
                if (tt + dur > len) dur = len - tt;
                t[nt].start = tt; t[nt].end = tt + dur; t[nt].pitch = 48 + (int)(rnd() * 30);
                nt++;
                tt += dur + rnd() * 0.3;
            }
        }
        size_t n = (size_t)(len * sr);
        float* x = make_mix(t, nt, sr, n);
        vi_poly_note* out = malloc(sizeof(vi_poly_note) * 4096);
        long k = vi_poly_notes(x, n, sr, 80.0, out, 4096);
        int* used = calloc(k > 0 ? k : 1, sizeof(int));
        long matched = 0;
        for (int i = 0; i < nt; i++) {
            int hit = 0;
            for (long j = 0; j < k && !hit; j++) {
                if (used[j]) continue;
                if (out[j].pitch == t[i].pitch && fabs(out[j].start - t[i].start) < 0.05) { used[j] = 1; hit = 1; }
            }
            matched += hit;
        }
        tp += matched; fn += nt - matched; fp += k - matched;
        if (argc <= 1) printf("piece %2d: %d voices, %3d true notes, %3ld found, %3ld matched\n", pc, voices, nt, k, matched);
        free(x); free(out); free(used);
    }
    double prec = tp + fp ? (double)tp / (tp + fp) : 0, rec = tp + fn ? (double)tp / (tp + fn) : 0;
    double f1 = prec + rec > 0 ? 2 * prec * rec / (prec + rec) : 0;
    printf("\nseed %u, 2..%d voices, partial decay %.2f: precision %.1f%%  recall %.1f%%  F1 %.1f%%  (synthetic, onset tolerance 50 ms)\n",
           seed_arg, voices_max, partial_decay, 100 * prec, 100 * rec, 100 * f1);
    return 0;
}
