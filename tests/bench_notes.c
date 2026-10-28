#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "notes.h"
int main(void) {
    const double sr = 48000; const size_t n = (size_t)(60 * sr);   /* one minute */
    float* x = malloc(sizeof(float) * n);
    for (size_t i = 0; i < n; i++) x[i] = (float)(0.3 * sin(2 * M_PI * 110.0 * i / sr));
    vi_line_note* notes = malloc(sizeof(vi_line_note) * 100000);
    clock_t t0 = clock();
    long k = vi_extract_notes(x, n, sr, 40, 400, 60, notes, 100000);
    double secs = (double)(clock() - t0) / CLOCKS_PER_SEC;
    printf("one minute of audio at 48 kHz: %.2f s of CPU on this machine (real-time factor %.3f), notes %ld\n", secs, secs / 60.0, k);
    free(x); free(notes);
    return 0;
}
