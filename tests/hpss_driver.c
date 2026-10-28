/* usage: hpss_driver in.f32 sr out.f32 : writes the harmonic part */
#include <stdio.h>
#include <stdlib.h>
#include "hpss.h"
int main(int argc, char** argv) {
    if (argc < 4) return 2;
    FILE* f = fopen(argv[1], "rb"); if (!f) return 1;
    fseek(f, 0, SEEK_END); long b = ftell(f); fseek(f, 0, SEEK_SET);
    size_t n = (size_t)b / sizeof(float);
    float* x = malloc(n * sizeof(float)), *y = malloc(n * sizeof(float));
    if (!x || !y || fread(x, sizeof(float), n, f) != n) return 1;
    fclose(f);
    if (vi_hpss(x, n, atof(argv[2]), y) != 0) return 1;
    FILE* g = fopen(argv[3], "wb"); if (!g) return 1;
    fwrite(y, sizeof(float), n, g); fclose(g);
    free(x); free(y);
    return 0;
}
