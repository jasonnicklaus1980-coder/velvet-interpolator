// Drives the plugin through a fake host: records the progression, then plays it back with the transport running.
// Checks that pad note-ons and note-offs go out as MIDI events while the transport plays, that a stop sends
// note-offs, and that a new start plays the schedule again from zero.
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <vector>
#include "vst2.h"
#include "padsend.h"
extern "C" AEffect* VSTPluginMain(audioMasterCallback);
enum { P_RECORD, P_ANALYSE, P_TRANSPOSE, P_INVERSION, P_PROGRESS };

static bool transportPlaying = false;
static double samplePos = 0.0;
static VstTimeInfo timeInfo;
static int ons = 0, offs = 0, firstOnDelta = -1;
static long long firstOnSample = -1;
static long long blockStart = 0;
static long long playStart = 0;   /* sample where the transport last started */

struct EventList { int32_t numEvents; intptr_t reserved; void* events[256]; };

static intptr_t host(AEffect*, int32_t op, int32_t, intptr_t, void* ptr, float) {
    if (op == audioMasterGetTime) {
        memset(&timeInfo, 0, sizeof timeInfo);
        timeInfo.samplePos = samplePos;
        timeInfo.flags = transportPlaying ? kVstTransportPlaying : 0;
        return (intptr_t)&timeInfo;
    }
    if (op == audioMasterProcessEvents) {
        EventList* list = (EventList*)ptr;
        for (int32_t i = 0; i < list->numEvents; i++) {
            ps_MidiEvent* e = (ps_MidiEvent*)list->events[i];
            int status = (unsigned char)e->midiData[0] & 0xF0;
            if (status == 0x90 && (unsigned char)e->midiData[2] > 0) {
                ons++;
                if (firstOnSample < 0) { firstOnSample = blockStart + e->deltaFrames; firstOnDelta = e->deltaFrames; }
            } else if (status == 0x80 || status == 0x90) offs++;
        }
    }
    return 0;
}

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

static void runBlocks(AEffect* fx, double seconds, int sr, int B) {
    std::vector<float> in(B, 0.f), l(B), r(B);
    long long total = (long long)(seconds * sr);
    for (long long done = 0; done < total; done += B) {
        int n = (int)((total - done) < B ? total - done : B);
        blockStart = (long long)samplePos;
        float* ip[2] = {in.data(), in.data()};
        float* op[2] = {l.data(), r.data()};
        fx->processReplacing(fx, ip, op, n);
        samplePos += n;
    }
}

int main() {
    const int sr = 48000, B = 512;
    AEffect* fx = VSTPluginMain(host);
    fx->dispatcher(fx, effSetSampleRate, 0, 0, nullptr, (float)sr);

    // record C, G, Am, F (1.5 s each) with the transport stopped, then stop and analyse
    std::vector<float> song;
    chordTone(song, sr, 0, 0, 1.5f); chordTone(song, sr, 7, 0, 1.5f);
    chordTone(song, sr, 9, 1, 1.5f); chordTone(song, sr, 5, 0, 1.5f);
    fx->setParameter(fx, P_RECORD, 1.f);
    std::vector<float> l(B), r(B), ol(B), or_(B);
    for (size_t i = 0; i < song.size(); i += B) {
        size_t n = (song.size() - i) < (size_t)B ? song.size() - i : B;
        for (size_t j = 0; j < n; j++) l[j] = r[j] = song[i + j];
        float* ip[2] = {l.data(), r.data()}; float* op[2] = {ol.data(), or_.data()};
        fx->processReplacing(fx, ip, op, (int32_t)n);
        samplePos += n;
    }
    fx->setParameter(fx, P_RECORD, 0.f);
    fx->setParameter(fx, P_ANALYSE, 1.f);
    int waited = 0;
    while ((float)fx->getParameter(fx, P_PROGRESS) < 1.f && waited < 600) { usleep(50000); waited++; }
    if (waited >= 600) { printf("FAIL: analysis timed out\n"); return 1; }

    int fails = 0;
    // transport stopped: nothing is sent
    ons = offs = 0;
    runBlocks(fx, 0.2, sr, B);
    if (ons || offs) { fails++; printf("FAIL: events while stopped (%d on, %d off)\n", ons, offs); }

    // transport plays for 6 s: the four chords go out, each note-on matched by a note-off
    transportPlaying = true; playStart = (long long)samplePos;
    ons = offs = 0; firstOnSample = -1;
    runBlocks(fx, 6.0, sr, B);
    printf("playing: %d note-ons, %d note-offs, first %lld samples after start\n", ons, offs, firstOnSample - playStart);
    if (ons < 4) { fails++; printf("FAIL: expected at least 4 pad hits, got %d\n", ons); }
    if (offs != ons) { fails++; printf("FAIL: %d note-ons but %d note-offs\n", ons, offs); }
    if (firstOnSample < 0 || firstOnSample - playStart > sr / 10) { fails++; printf("FAIL: playback did not start near zero\n"); }

    // transport stops: the stop sends note-offs for the schedule, and nothing more
    transportPlaying = false;
    ons = offs = 0;
    runBlocks(fx, 0.1, sr, B);
    if (offs < 4 || ons) { fails++; printf("FAIL: stop sent %d note-offs and %d note-ons\n", offs, ons); }

    // a new start plays the schedule again from zero
    transportPlaying = true; playStart = (long long)samplePos;
    ons = offs = 0; firstOnSample = -1;
    runBlocks(fx, 1.0, sr, B);
    if (ons < 1 || firstOnSample < 0 || firstOnSample - playStart > sr / 10) { fails++; printf("FAIL: restart did not replay from zero\n"); }

    printf(fails ? "MIDI OUT TESTS FAILED (%d)\n" : "MIDI OUT TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
