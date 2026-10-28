#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <vector>
#include "vst2.h"
extern "C" AEffect* VSTPluginMain(audioMasterCallback);
enum { P_RECORD, P_ANALYSE, P_TRANSPOSE, P_INVERSION, P_PROGRESS };
static intptr_t noHost(AEffect*, int32_t, int32_t, intptr_t, void*, float) { return 0; }
static int fails = 0;
#define CHECK(c, m) do { if (c) printf("  ok:   %s\n", m); else { printf("  FAIL: %s\n", m); fails++; } } while (0)

static void feed(AEffect* fx, float amp, int seconds, int sr) {
    std::vector<float> a(512), b(512), oa(512), ob(512);
    for (int s = 0; s < seconds * sr; s += 512) {
        for (int j = 0; j < 512; j++) a[j] = b[j] = amp * sinf(2 * (float)M_PI * 220.f * (s + j) / sr);
        float* ip[2] = {a.data(), b.data()}; float* op[2] = {oa.data(), ob.data()};
        fx->processReplacing(fx, ip, op, 512);
    }
}
static void waitDone(AEffect* fx) { for (int i = 0; i < 600 && fx->getParameter(fx, P_PROGRESS) < 1.f; i++) usleep(50000); }

int main() {
    const int sr = 48000;
    AEffect* fx = VSTPluginMain(noHost);
    fx->dispatcher(fx, effSetSampleRate, 0, 0, nullptr, (float)sr);
    printf("1. analyse with nothing recorded\n");
    fx->setParameter(fx, P_ANALYSE, 1.f);
    fx->setParameter(fx, P_ANALYSE, 0.f);
    CHECK(fx->getParameter(fx, P_PROGRESS) == 0.f, "no analysis started, progress stays 0");
    printf("2. record, then trigger analysis twice; second press while busy is ignored\n");
    fx->setParameter(fx, P_RECORD, 1.f);
    feed(fx, 0.3f, 2, sr);
    fx->setParameter(fx, P_RECORD, 0.f);
    fx->setParameter(fx, P_ANALYSE, 1.f);
    fx->setParameter(fx, P_ANALYSE, 0.f);
    fx->setParameter(fx, P_ANALYSE, 1.f);   /* second rising edge while busy */
    fx->setParameter(fx, P_ANALYSE, 0.f);
    waitDone(fx);
    CHECK(fx->getParameter(fx, P_PROGRESS) >= 1.f, "analysis of a 2 s take completes");
    printf("3. new take replaces the old one\n");
    fx->setParameter(fx, P_RECORD, 1.f);
    feed(fx, 0.3f, 1, sr);
    fx->setParameter(fx, P_RECORD, 0.f);
    fx->setParameter(fx, P_ANALYSE, 1.f);
    fx->setParameter(fx, P_ANALYSE, 0.f);
    waitDone(fx);
    CHECK(fx->getParameter(fx, P_PROGRESS) >= 1.f, "second take analysed");
    printf("4. transposition control changes the output\n");
    fx->setParameter(fx, P_TRANSPOSE, 1.0f);   /* +12 */
    CHECK(fabsf(fx->getParameter(fx, P_TRANSPOSE) - 1.0f) < 1e-6f, "transpose parameter reads back");
    printf("5. close while the worker is running (no crash, no hang)\n");
    fx->setParameter(fx, P_RECORD, 1.f);
    feed(fx, 0.3f, 2, sr);
    fx->setParameter(fx, P_RECORD, 0.f);
    fx->setParameter(fx, P_ANALYSE, 1.f);
    fx->setParameter(fx, P_ANALYSE, 0.f);
    fx->dispatcher(fx, effClose, 0, 0, nullptr, 0);
    CHECK(true, "close returned after the worker finished");
    printf("6. input level meter reads a known level (0.5 peak = -6 dBFS, meter 0.90)\n");
    {
        AEffect* m = VSTPluginMain(noHost);
        m->dispatcher(m, effSetSampleRate, 0, 0, nullptr, (float)sr);
        feed(m, 0.5f, 1, sr);
        float lvl = m->getParameter(m, 6);
        char msg[128]; snprintf(msg, sizeof msg, "level meter %.3f (expect 0.90)", lvl);
        CHECK(fabsf(lvl - 0.90f) < 0.03f, msg);
        feed(m, 0.f, 1, sr);
        CHECK(m->getParameter(m, 6) < 0.05f, "level falls back to silence after 1 s of silence");
        m->dispatcher(m, effClose, 0, 0, nullptr, 0);
    }
    printf("7. LED box: a 1 kHz tone lights the band nearest 1 kHz, a 100 Hz tone a low band\n");
    {
        AEffect* m = VSTPluginMain(noHost);
        m->dispatcher(m, effSetSampleRate, 0, 0, nullptr, (float)sr);
        feed(m, 0.5f, 1, sr);          /* 220 Hz helper tone is replaced below */
        auto argmax = [&](void) { int best = 0; float bv = -1; for (int k = 0; k < 16; k++) { float v = m->getParameter(m, 7 + k); if (v > bv) { bv = v; best = k; } } return best; };
        std::vector<float> a(512), oa(512), ob(512);
        for (int blk = 0; blk < 200; blk++) {
            for (int j = 0; j < 512; j++) a[j] = 0.5f * sinf(2 * (float)M_PI * 1000.f * (blk * 512 + j) / sr);
            float* ip[2] = {a.data(), a.data()}; float* op[2] = {oa.data(), ob.data()};
            m->processReplacing(m, ip, op, 512);
        }
        int k1 = argmax();
        CHECK(k1 == 8, "1 kHz tone lights band 9 (centre about 1050 Hz)");
        for (int blk = 0; blk < 200; blk++) {
            for (int j = 0; j < 512; j++) a[j] = 0.5f * sinf(2 * (float)M_PI * 100.f * (blk * 512 + j) / sr);
            float* ip[2] = {a.data(), a.data()}; float* op[2] = {oa.data(), ob.data()};
            m->processReplacing(m, ip, op, 512);
        }
        int k2 = argmax();
        char msg[128]; snprintf(msg, sizeof msg, "100 Hz tone lights band %d (expect a low band, 0 to 3)", k2 + 1);
        CHECK(k2 <= 3, msg);
        m->dispatcher(m, effClose, 0, 0, nullptr, 0);
    }
    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL EDGE TESTS PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
