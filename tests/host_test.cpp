// Drives Velvet Interpolator the way the MPC would: record a synthetic progression, analyse it, check the files.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <vector>
#include "vst2.h"
extern "C" AEffect* VSTPluginMain(audioMasterCallback);
enum { P_RECORD, P_ANALYSE, P_TRANSPOSE, P_INVERSION, P_PROGRESS };
static intptr_t noHost(AEffect*, int32_t, int32_t, intptr_t, void*, float) { return 0; }

// C major, G major, A minor, F major, 1.5 s each, with harmonics (same idea as the Python tests)
static void chordTone(std::vector<float>& out, int sr, int root, int quality, float seconds) {
    static const int triad[] = {0, 4, 7}, minor[] = {0, 3, 7};
    const int* ints = quality ? minor : triad;
    int n = (int)(seconds * sr);
    size_t base = out.size();
    out.resize(base + n, 0.f);
    for (int k = 0; k < 3; k++) {
        double f0 = 440.0 * pow(2.0, ((48 + root + ints[k]) - 69) / 12.0);
        for (int h = 1; h <= 4; h++)
            for (int i = 0; i < n; i++) out[base + i] += (float)(0.25 / h * sin(2 * M_PI * f0 * h * i / sr));
    }
}

int main() {
    const int sr = 48000;
    AEffect* fx = VSTPluginMain(noHost);
    fx->dispatcher(fx, effSetSampleRate, 0, 0, nullptr, (float)sr);
    fx->setParameter(fx, P_RECORD, 1.f);
    std::vector<float> song;
    chordTone(song, sr, 0, 0, 1.5f);   /* C  */
    chordTone(song, sr, 7, 0, 1.5f);   /* G  */
    chordTone(song, sr, 9, 1, 1.5f);   /* Am */
    chordTone(song, sr, 5, 0, 1.5f);   /* F  */
    const int B = 512;
    std::vector<float> l(B), r(B), ol(B), or_(B);
    for (size_t i = 0; i < song.size(); i += B) {
        size_t n = (song.size() - i) < (size_t)B ? song.size() - i : B;
        for (size_t j = 0; j < n; j++) { l[j] = r[j] = song[i + j]; }
        float* ip[2] = {l.data(), r.data()}; float* op[2] = {ol.data(), or_.data()};
        fx->processReplacing(fx, ip, op, (int32_t)n);
        for (size_t j = 0; j < n; j++) if (fabsf(ol[j] - song[i + j]) > 0.f) { printf("FAIL: pass-through changed audio\n"); return 1; }
    }
    fx->setParameter(fx, P_RECORD, 0.f);
    fx->setParameter(fx, P_ANALYSE, 1.f);
    int waited = 0;
    while ((float)fx->getParameter(fx, P_PROGRESS) < 1.f && waited < 600) { usleep(50000); waited++; }
    printf("analysis finished: %s (progress %.2f)\n", waited < 600 ? "yes" : "NO, timed out", fx->getParameter(fx, P_PROGRESS));
    fx->dispatcher(fx, effClose, 0, 0, nullptr, 0);
    return waited < 600 ? 0 : 1;
}
