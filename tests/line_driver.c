/* usage: line_driver audio.f32 sample_rate -> prints "start end pitch" per monophonic note (YIN) */
#include <stdio.h>
#include <stdlib.h>
#include "notes.h"
int main(int argc, char** argv) {
    if (argc < 3) return 2;
    FILE* f = fopen(argv[1], "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long b = ftell(f); fseek(f, 0, SEEK_SET);
    size_t n = (size_t)b / sizeof(float);
    float* x = malloc(n * sizeof(float));
    if (!x || fread(x, sizeof(float), n, f) != n) return 1;
    fclose(f);
    size_t cap = n / 64 + 64;
    vi_line_note* out = malloc(sizeof(vi_line_note) * cap);
    long k = vi_extract_notes(x, n, atof(argv[2]), 55.0, 1000.0, 60.0, out, cap);
    if (k < 0) return 1;
    for (long i = 0; i < k; i++) printf("%.4f %.4f %d\n", out[i].start, out[i].end, out[i].pitch);
    free(x); free(out);
    return 0;
}
