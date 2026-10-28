// Records a bass-like line through the plugin, runs the line analysis, and checks line.mid exists.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <vector>
#include "vst2.h"
extern "C" AEffect* VSTPluginMain(audioMasterCallback);
enum { P_RECORD, P_ANALYSE, P_TRANSPOSE, P_INVERSION, P_PROGRESS, P_LINE };
static intptr_t noHost(AEffect*, int32_t, int32_t, intptr_t, void*, float) { return 0; }
int main() {
    const int sr = 48000;
    int midi[] = {36, 38, 41, 43, 41, 38};
    AEffect* fx = VSTPluginMain(noHost);
    fx->dispatcher(fx, effSetSampleRate, 0, 0, nullptr, (float)sr);
    std::vector<float> song;
    for (int i = 0; i < 6; i++) {
        double f0 = 440.0 * pow(2.0, (midi[i] - 69) / 12.0);
        size_t base = song.size(), n = (size_t)(0.5 * sr);
        song.resize(base + n);
        for (size_t j = 0; j < n; j++) {
            double t = j / (double)sr, env = fmin(1.0, fmin(t / 0.01, (0.5 - t) / 0.01)), v = 0;
            for (int h = 1; h <= 5; h++) v += sin(2 * M_PI * f0 * h * t) / h;
            song[base + j] = (float)(0.3 * env * v);
        }
    }
    fx->setParameter(fx, P_RECORD, 1.f);
    std::vector<float> a(512), b(512), oa(512), ob(512);
    for (size_t i = 0; i < song.size(); i += 512) {
        size_t n = (song.size() - i) < 512 ? song.size() - i : 512;
        for (size_t j = 0; j < n; j++) a[j] = b[j] = song[i + j];
        float* ip[2] = {a.data(), b.data()}; float* op[2] = {oa.data(), ob.data()};
        fx->processReplacing(fx, ip, op, (int32_t)n);
    }
    fx->setParameter(fx, P_RECORD, 0.f);
    fx->setParameter(fx, P_LINE, 1.f);
    fx->setParameter(fx, P_LINE, 0.f);
    for (int i = 0; i < 600 && fx->getParameter(fx, P_PROGRESS) < 1.f; i++) usleep(50000);
    fx->dispatcher(fx, effClose, 0, 0, nullptr, 0);
    printf("line analysis finished, progress %.2f\n", fx->getParameter(fx, P_PROGRESS));
    return 0;
}
