#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "poly.h"
int main(void) {
    const double sr = 48000; size_t n = (size_t)(60 * sr);
    float* x = malloc(sizeof(float) * n);
    for (size_t i = 0; i < n; i++) x[i] = (float)(0.1 * sin(2 * M_PI * 220.0 * i / sr) + 0.1 * sin(2 * M_PI * 330.0 * i / sr));
    vi_poly_note* out = malloc(sizeof(vi_poly_note) * 100000);
    clock_t t0 = clock();
    long k = vi_poly_notes(x, n, sr, 80.0, out, 100000);
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("one minute of audio: %.2f s CPU on this machine (real-time factor %.3f), notes %ld\n", secs, secs / 60.0, k);
    return 0;
}
