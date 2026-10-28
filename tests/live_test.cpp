// Live mode: record a progression, then turn Record off. The chord files must appear without pressing Analyse.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <vector>
#include "vst2.h"
extern "C" AEffect* VSTPluginMain(audioMasterCallback);
enum { P_RECORD, P_ANALYSE, P_TRANSPOSE, P_INVERSION, P_PROGRESS, P_LINE, P_LEVEL };
static intptr_t noHost(AEffect*, int32_t, int32_t, intptr_t, void*, float) { return 0; }
int main(void) {
    const int sr = 48000;
    AEffect* fx = VSTPluginMain(noHost);
    fx->dispatcher(fx, effSetSampleRate, 0, 0, nullptr, (float)sr);
    std::vector<float> song;
    static const int roots[] = {0, 7, 9, 5}, mins[] = {0, 0, 1, 0};
    for (int c = 0; c < 4; c++) {
        size_t base = song.size(), n = (size_t)(2.0 * sr);
        song.resize(base + n, 0.f);
        static const int tri[] = {0, 4, 7}, mi[] = {0, 3, 7};
        const int* ints = mins[c] ? mi : tri;
        for (int k = 0; k < 3; k++) {
            double f0 = 440.0 * pow(2.0, ((48 + roots[c] + ints[k]) - 69) / 12.0);
            for (int h = 1; h <= 4; h++)
                for (size_t j = 0; j < n; j++) song[base + j] += (float)(0.25 / h * sin(2 * M_PI * f0 * h * (j / (double)sr)));
        }
    }
    fx->setParameter(fx, P_RECORD, 1.f);
    std::vector<float> a(512), b(512), oa(512), ob(512);
    for (size_t i = 0; i < song.size(); i += 512) {
        size_t n = (song.size() - i) < 512 ? song.size() - i : 512;
        for (size_t j = 0; j < n; j++) a[j] = b[j] = song[i + j];
        float* ip[2] = {a.data(), b.data()}; float* op[2] = {oa.data(), ob.data()};
        fx->processReplacing(fx, ip, op, (int32_t)n);
        usleep(2000);                        /* roughly real time: the worker keeps up while recording */
    }
    fx->setParameter(fx, P_RECORD, 0.f);      /* no Analyse press: the live result should be written now */
    for (int i = 0; i < 600 && fx->getParameter(fx, P_PROGRESS) < 1.f; i++) usleep(50000);
    const int P_KEY = 25, P_KEYCONF = 26;
    float keyIdx = fx->getParameter(fx, P_KEY) * 23.0f;
    float conf = fx->getParameter(fx, P_KEYCONF);
    printf("key estimate for C, G, Am, F: index %.0f (0 = C major), confidence %.2f\n", keyIdx, conf);
    fx->dispatcher(fx, effClose, 0, 0, nullptr, 0);
    return (keyIdx > 0.5f || conf <= 0.0f) ? 1 : 0;
}
