// Velvet Interpolator. Live mode: while Record is on, audio is copied into a lock-free ring buffer and a worker
// thread analyses it as it arrives (chords, with a fixed lag of about 0.2 s). When Record goes off, the worker
// writes pads.txt and the progression MIDI files. Analyse re-runs the chord analysis on the whole take, and Line
// writes line.mid. The audio thread never waits for analysis.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <atomic>
#include <new>
#include "vst2.h"
extern "C" {
#include "chordcore.h"
#include "musicout.h"
#include "notes.h"
#include "poly.h"
#include "hpss.h"
#include "keys.h"
#include "padsend.h"
}

#ifndef VI_OUT_DIR
#define VI_OUT_DIR "/media/az01-internal/VelvetInterp"
#endif
static const int32_t kUniqueId = 0x56494E32;     /* 'VIN2' */
enum { P_RECORD, P_ANALYSE, P_TRANSPOSE, P_INVERSION, P_PROGRESS, P_LINE, P_LEVEL, P_BAND0,
       P_OCTAVE = P_BAND0 + 16, P_VOICING, P_KEY, P_KEYCONF, P_COUNT };
static const int kBands = 16;                       /* LED box: 16 log-spaced frequency bands, 80 Hz to 10 kHz */
static const double kMaxSeconds = 60.0;            /* take length */
static const char* kBanks = "ABCD";
static const double kLineFmin = 40.0, kLineFmax = 1000.0;   /* bass to lead */
enum { JOB_NONE, JOB_ANALYSE, JOB_LINE, JOB_LIVE };
static const int kPadMax = 1024;                    /* pad hits kept for playback */
static const int kEvMax = 256;                      /* MIDI events per audio block */

/* the list the host reads for audioMasterProcessEvents: a count, then that many event pointers */
struct VstEventList {
    int32_t numEvents;
    intptr_t reserved;
    void* events[kEvMax];
};

struct Interp {
    AEffect fx;
    audioMasterCallback host;
    double sr = 44100.0;
    /* take buffer (mono), used by Analyse and Line */
    float* rec = nullptr;
    size_t cap = 0;
    std::atomic<size_t> len{0};
    /* live ring buffer: single producer (audio thread), single consumer (worker thread) */
    float* ring = nullptr;
    size_t ringCap = 0;
    std::atomic<size_t> ringWr{0}, ringRd{0};
    std::atomic<size_t> overruns{0};
    /* controls */
    std::atomic<int> recording{0};
    std::atomic<int> liveStop{0};
    float recordParam = 0.f, analyseParam = 0.f, transposeParam = 0.5f, inversionParam = 0.f, lineParam = 0.f;
    float octaveParam = 0.f, voicingParam = 0.f;
    std::atomic<float> keyParam{0.f}, keyConfParam{0.f};   /* key index / 23, confidence 0..1 (read-only) */
    std::atomic<float> progress{0.f};
    std::atomic<float> level{0.f};
    std::atomic<float> band[16];       /* per-band level for the LED box, same scale as level */
    double gcoef[16];                  /* Goertzel coefficients 2cos(w), set when the sample rate is known */
    /* worker */
    std::atomic<int> busy{0};
    std::atomic<int> jobMode{JOB_NONE};
    float* job = nullptr;
    size_t jobLen = 0;
    pthread_t worker;
    int workerStarted = 0;
    std::atomic<int> lastStatus{0};
    /* pad schedule: the worker fills the inactive buffer and publishes it; the audio thread reads the active one */
    ps_note padBuf[2][kPadMax];
    size_t padN[2] = {0, 0};
    std::atomic<int> padActive{-1};
    /* playback: the schedule restarts from zero each time the host transport starts */
    bool wasPlaying = false;
    double playStart = 0.0;
    ps_MidiEvent evBuf[kEvMax];
};

static Interp* P(AEffect* e) { return (Interp*)e->object; }
static int transposeOf(const Interp* p) { return (int)lroundf((p->transposeParam - 0.5f) * 24.0f); }   /* -12..+12 */
static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int octaveBase(const Interp* p) { return 36 + 12 * clampi((int)lroundf(p->octaveParam * 4.0f), 0, 4); }   /* C2 .. C6 */
static int voicingMode(const Interp* p) { return clampi((int)lroundf(p->voicingParam * 2.0f), 0, 2); }   /* 0 knob, 1 root, 2 open */
static int inversionOf(const Interp* p) { return (int)lroundf(p->inversionParam * 2.0f); }         /* 0..2 */

/* ---------- writing results ---------- */

/* chord segments -> progression_A..D.mid (pad hits) and voiced_chords.mid (the chord notes, transposed and
   inverted by the knobs). Times are frame indices at hop CC_HOP. */
static void writeChords(Interp* p, const cc_segment* seg, size_t count) {
    mkdir(VI_OUT_DIR, 0777);
    int order[108]; int nOrder = 0;
    for (size_t i = 0; i < count; i++) {
        int s = seg[i].state;
        if (s >= 12 * CC_NQUAL) continue;
        int seen = 0; for (int k = 0; k < nOrder; k++) if (order[k] == s) seen = 1;
        if (!seen && nOrder < 64) order[nOrder++] = s;
    }
    int inv = inversionOf(p), tr = transposeOf(p);
    char path[512];
    vi_note* notes = (vi_note*)malloc(sizeof(vi_note) * (count + 1));
    if (!notes) return;
    for (int bank = 0; bank < 4; bank++) {
        size_t m = 0;
        for (size_t i = 0; i < count; i++) {
            int s = seg[i].state;
            if (s >= 12 * CC_NQUAL) continue;
            int idx = -1; for (int k = 0; k < nOrder; k++) if (order[k] == s) idx = k;
            if (idx < 0 || idx / 16 != bank) continue;
            notes[m].start = seg[i].start_frame * (double)CC_HOP / p->sr;
            notes[m].end = (seg[i].end_frame + 1) * (double)CC_HOP / p->sr;
            notes[m].pitch = 36 + idx % 16; notes[m].velocity = 100; m++;
        }
        if (m > 0) {
            snprintf(path, sizeof path, "%s/progression_%c.mid", VI_OUT_DIR, kBanks[bank]);
            vi_write_smf(path, notes, m, 120.0);
        }
    }
    /* the same pad hits as a playable schedule: bank A..D is the MIDI channel, pad note is 36 + index % 16 */
    int next = p->padActive.load() == 1 ? 0 : 1;
    size_t pn = 0;
    for (size_t i = 0; i < count && pn < (size_t)kPadMax; i++) {
        int s = seg[i].state;
        if (s >= 12 * CC_NQUAL) continue;
        int idx = -1; for (int k = 0; k < nOrder; k++) if (order[k] == s) idx = k;
        if (idx < 0) continue;
        ps_note* d = &p->padBuf[next][pn++];
        d->start = seg[i].start_frame * (double)CC_HOP / p->sr;
        d->end = (seg[i].end_frame + 1) * (double)CC_HOP / p->sr;
        d->pitch = 36 + idx % 16; d->channel = idx / 16; d->velocity = 100;
    }
    p->padN[next] = pn;
    p->padActive.store(next);

    /* key estimate: pitch classes of the chord tones, weighted by how long each chord lasts */
    double hist[12] = {0};
    for (size_t i = 0; i < count; i++) {
        int s = seg[i].state;
        if (s >= 12 * CC_NQUAL) continue;
        int tn[8];
        int k = vi_voice(s, 0, 0, 0, tn);
        double dur = (double)(seg[i].end_frame - seg[i].start_frame + 1);
        for (int j2 = 0; j2 < k; j2++) hist[((tn[j2] % 12) + 12) % 12] += dur;
    }
    double keyConf = 0.0;
    int key = vi_estimate_key(hist, &keyConf);
    p->keyParam = key / 23.0f;
    p->keyConfParam = (float)keyConf;

    /* voiced chord notes: what a keyboard or synth should play, with the transpose and inversion knobs applied */
    vi_note* voiced = (vi_note*)malloc(sizeof(vi_note) * (count * 4 + 1));
    if (voiced) {
        size_t m = 0;
        for (size_t i = 0; i < count; i++) {
            int s = seg[i].state;
            if (s >= 12 * CC_NQUAL) continue;
            int vn[8];
            int k = voicingMode(p) == 0 ? vi_voice(s, tr, inv, 0, vn)
                  : vi_voice(s, tr, 0, voicingMode(p) == 2 ? 1 : 0, vn);
            for (int j2 = 0; j2 < k; j2++) {
                voiced[m].start = seg[i].start_frame * (double)CC_HOP / p->sr;
                voiced[m].end = (seg[i].end_frame + 1) * (double)CC_HOP / p->sr;
                voiced[m].pitch = vn[j2]; voiced[m].velocity = 90; m++;
            }
        }
        if (m > 0) {
            snprintf(path, sizeof path, "%s/voiced_chords.mid", VI_OUT_DIR);
            vi_write_smf(path, voiced, m, 120.0);
        }
        free(voiced);
    }
    free(notes);
}

/* Drum-tolerant melody: harmonic-percussive separation, then monophonic note detection -> melody.mid */
static void writeMelody(Interp* p) {
    float* harmonic = (float*)malloc(sizeof(float) * p->jobLen);
    if (!harmonic) return;
    p->progress = 0.15f;
    if (vi_hpss(p->job, p->jobLen, p->sr, harmonic) != 0) { free(harmonic); return; }
    size_t cap = p->jobLen / 64 + 16;
    vi_line_note* lines = (vi_line_note*)malloc(sizeof(vi_line_note) * cap);
    vi_note* notes = (vi_note*)malloc(sizeof(vi_note) * cap);
    if (lines && notes) {
        long k = vi_extract_notes(harmonic, p->jobLen, p->sr, kLineFmin, kLineFmax, 60.0, lines, cap);
        if (k > 0) {
            for (long i = 0; i < k; i++) {
                notes[i].start = lines[i].start; notes[i].end = lines[i].end;
                notes[i].pitch = lines[i].pitch; notes[i].velocity = 100;
            }
            char path[512];
            snprintf(path, sizeof path, "%s/melody.mid", VI_OUT_DIR);
            mkdir(VI_OUT_DIR, 0777);
            vi_write_smf(path, notes, (size_t)k, 120.0);
            /* melody pads: each note folded by octaves into the 16 pads above the octave setting */
            int base = octaveBase(p);
            for (long i = 0; i < k; i++) {
                int q = notes[i].pitch;
                while (q < base) q += 12;
                while (q >= base + 16) q -= 12;
                notes[i].pitch = q;
            }
            snprintf(path, sizeof path, "%s/melody_pads.mid", VI_OUT_DIR);
            vi_write_smf(path, notes, (size_t)k, 120.0);
        }
    }
    free(lines); free(notes); free(harmonic);
}

static void writeLine(Interp* p) {
    size_t cap = p->jobLen / 64 + 16;
    vi_line_note* lines = (vi_line_note*)malloc(sizeof(vi_line_note) * cap);
    vi_note* notes = (vi_note*)malloc(sizeof(vi_note) * cap);
    if (!lines || !notes) { free(lines); free(notes); p->lastStatus = -1; return; }
    p->progress = 0.3f;
    long k = vi_extract_notes(p->job, p->jobLen, p->sr, kLineFmin, kLineFmax, 60.0, lines, cap);
    p->progress = 0.5f;
    if (k < 0) { p->lastStatus = -1; free(lines); free(notes); return; }
    for (long i = 0; i < k; i++) {
        notes[i].start = lines[i].start; notes[i].end = lines[i].end;
        notes[i].pitch = lines[i].pitch; notes[i].velocity = 100;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/line.mid", VI_OUT_DIR);
    mkdir(VI_OUT_DIR, 0777);
    if (k > 0) vi_write_smf(path, notes, (size_t)k, 120.0);
    free(lines); free(notes);
    writeMelody(p);
    /* polyphonic note analysis of the same take -> notes.mid (every note found, chords included) */
    size_t pcap = p->jobLen / 256 + 64;
    vi_poly_note* pn = (vi_poly_note*)malloc(sizeof(vi_poly_note) * pcap);
    if (pn) {
        p->progress = 0.85f;
        long m = vi_poly_notes(p->job, p->jobLen, p->sr, 80.0, pn, pcap);
        if (m > 0) {
            vi_note* all = (vi_note*)malloc(sizeof(vi_note) * (size_t)m);
            if (all) {
                for (long i = 0; i < m; i++) {
                    all[i].start = pn[i].start; all[i].end = pn[i].end;
                    all[i].pitch = pn[i].pitch; all[i].velocity = 90;
                }
                snprintf(path, sizeof path, "%s/notes.mid", VI_OUT_DIR);
                vi_write_smf(path, all, (size_t)m, 120.0);
                free(all);
            }
        }
        free(pn);
    }
    p->lastStatus = 1;
}

/* ---------- worker thread ---------- */

/* Live: drains the ring buffer into the streaming analyser until Record is off and the ring is empty. */
static void runLive(Interp* p) {
    cc_stream st;
    if (cc_stream_init(&st, p->sr) != 0) { p->lastStatus = -1; return; }
    float* block = (float*)malloc(sizeof(float) * 4096);
    if (!block) { cc_stream_free(&st); p->lastStatus = -1; return; }
    for (;;) {
        size_t wr = p->ringWr.load(std::memory_order_acquire);
        size_t rd = p->ringRd.load(std::memory_order_relaxed);
        size_t avail = wr - rd;
        if (avail == 0) {
            if (p->liveStop.load()) break;
            usleep(5000);
            continue;
        }
        size_t take = avail < 4096 ? avail : 4096;
        for (size_t i = 0; i < take; i++) block[i] = p->ring[(rd + i) % p->ringCap];
        p->ringRd.store(rd + take, std::memory_order_release);
        if (cc_stream_push(&st, block, take) != 0) { p->lastStatus = -1; break; }
        p->progress = 0.5f;
    }
    cc_stream_flush(&st);
    p->progress = 0.9f;
    /* decided states -> segments */
    size_t count = 0, cap = st.ndecided + 1;
    cc_segment* seg = (cc_segment*)malloc(sizeof(cc_segment) * cap);
    if (seg && st.ndecided > 0) {
        size_t start = 0;
        for (size_t f = 1; f <= st.ndecided; f++) {
            if (f == st.ndecided || st.decided[f] != st.decided[start]) {
                seg[count].start_frame = start; seg[count].end_frame = f - 1; seg[count].state = st.decided[start];
                count++; start = f;
            }
        }
        writeChords(p, seg, count);
        p->lastStatus = 1;
    } else {
        p->lastStatus = -1;
    }
    free(seg); free(block);
    cc_stream_free(&st);
    /* one step: the same take also gives the melody and the note files (recording has stopped, so rec is stable) */
    size_t take = p->len.load();
    if (take >= CC_NFFT && p->rec) {
        free(p->job);
        p->job = (float*)malloc(sizeof(float) * take);
        if (p->job) {
            memcpy(p->job, p->rec, sizeof(float) * take);
            p->jobLen = take;
            writeLine(p);
        }
    }
}

static void* workerMain(void* arg) {
    Interp* p = (Interp*)arg;
    p->progress = 0.05f;
    int mode = p->jobMode.load();
    if (mode == JOB_LIVE) {
        runLive(p);
    } else if (mode == JOB_LINE) {
        writeLine(p);
    } else if (mode == JOB_ANALYSE) {
        static cc_segment seg[8192];
        size_t count = 0;
        long frames = cc_analyze(p->job, p->jobLen, p->sr, seg, 8192, &count);
        if (frames < 0) p->lastStatus = -1;
        else { writeChords(p, seg, count); p->lastStatus = 1; }
    }
    p->progress = 1.f;
    p->busy = 0;
    return nullptr;
}

static void joinWorker(Interp* p) {
    if (p->workerStarted) { pthread_join(p->worker, nullptr); p->workerStarted = 0; }
}

static void startWorker(Interp* p, int mode, const float* data, size_t n) {
    if (p->busy) return;
    if (n < CC_NFFT) return;             /* nothing (or too little) recorded: nothing to analyse */
    joinWorker(p);
    if (data) {
        free(p->job);
        p->job = (float*)malloc(sizeof(float) * n);
        if (!p->job) return;
        memcpy(p->job, data, sizeof(float) * n);
        p->jobLen = n;
    }
    p->jobMode = mode;
    p->busy = 1;
    p->progress = 0.f;
    if (pthread_create(&p->worker, nullptr, workerMain, p) == 0) p->workerStarted = 1;
    else p->busy = 0;
}

static void startLive(Interp* p) {
    if (p->busy) return;                 /* previous job still running: ignore the new take */
    joinWorker(p);
    p->ringWr = 0; p->ringRd = 0;
    p->liveStop = 0;
    p->jobMode = JOB_LIVE;
    p->busy = 1;
    p->progress = 0.f;
    if (pthread_create(&p->worker, nullptr, workerMain, p) == 0) p->workerStarted = 1;
    else p->busy = 0;
}

/* ---------- plugin callbacks ---------- */

static const char* kParamNames[P_COUNT] = {"Record", "Analyse", "Transpose", "Inversion", "Progress", "Line", "Level",
    "Band 1", "Band 2", "Band 3", "Band 4", "Band 5", "Band 6", "Band 7", "Band 8",
    "Band 9", "Band 10", "Band 11", "Band 12", "Band 13", "Band 14", "Band 15", "Band 16",
    "Octave", "Voicing", "Key", "Key conf"};

static double band_hz(int k) { return 80.0 * pow(10000.0 / 80.0, k / 15.0); }   /* 80 Hz .. 10 kHz, log spaced */

static intptr_t dispatcher(AEffect* e, int32_t op, int32_t idx, intptr_t, void* ptr, float opt) {
    Interp* p = P(e);
    switch (op) {
    case effCanDo:
        if (ptr && (!strcmp((const char*)ptr, "sendVstMidiEvent") || !strcmp((const char*)ptr, "sendVstEvents")))
            return 1;
        return 0;
    case effSetSampleRate:
        p->sr = opt;
        for (int k = 0; k < kBands; k++) p->gcoef[k] = 2.0 * cos(2.0 * M_PI * band_hz(k) / opt);
        free(p->rec); free(p->ring);
        p->cap = (size_t)(kMaxSeconds * opt);
        p->rec = (float*)malloc(sizeof(float) * p->cap);
        p->ringCap = (size_t)(kMaxSeconds * opt);
        p->ring = (float*)malloc(sizeof(float) * p->ringCap);
        p->len.store(0);
        return 0;
    case effClose:
        p->liveStop = 1;
        joinWorker(p);
        free(p->rec); free(p->ring); free(p->job);
        p->~Interp();
        free(p);
        return 0;
    case effGetVstVersion:    return 2400;
    case effGetVendorVersion: return 1;
    case effGetTailSize:      return 0;
    case effGetProductString: if (ptr) strncpy((char*)ptr, "Velvet Interpolator", 63); return 0;
    case effGetVendorString:  if (ptr) strncpy((char*)ptr, "Velvet", 63); return 0;
    case effGetParamName:
        if (ptr && idx >= 0 && idx < P_COUNT) strncpy((char*)ptr, kParamNames[idx], 8);
        return 0;
    case effGetParamDisplay:
        if (!ptr) return 0;
        if (idx == P_TRANSPOSE) snprintf((char*)ptr, 8, "%+d", transposeOf(p));
        else if (idx == P_INVERSION) {
            static const char* inv[3] = {"ROOT", "1ST", "2ND"};
            snprintf((char*)ptr, 8, "%s", inv[inversionOf(p)]);
        }
        else if (idx == P_PROGRESS) snprintf((char*)ptr, 8, "%d%%", (int)(p->progress * 100.f));
        else if (idx == P_LEVEL) snprintf((char*)ptr, 8, "%.0fdB", p->level * 60.0f - 60.0f);
        else if (idx == P_OCTAVE) {
            static const char* nm[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            int base = octaveBase(p);
            snprintf((char*)ptr, 8, "%s%d", nm[base % 12], base / 12 - 1);   /* e.g. C2 */
        }
        else if (idx == P_VOICING) {
            static const char* vn[3] = {"Knob", "Root", "Open"};
            snprintf((char*)ptr, 8, "%s", vn[voicingMode(p)]);
        }
        else if (idx == P_KEY) snprintf((char*)ptr, 8, "%s", vi_key_name((int)lroundf(p->keyParam * 23.0f)));
        else if (idx == P_KEYCONF) snprintf((char*)ptr, 8, "%.0f%%", p->keyConfParam * 100.0f);
        else if (idx >= P_BAND0) snprintf((char*)ptr, 8, "%.0fdB", p->band[idx - P_BAND0] * 60.0f - 60.0f);
        else snprintf((char*)ptr, 8, "%s", p->recording ? "on" : "off");
        return 0;
    case effCanBeAutomated:   return (idx == P_PROGRESS || idx == P_LEVEL || idx == P_KEY || idx == P_KEYCONF || idx >= P_BAND0) ? 0 : 1;
    default: (void)opt; return 0;
    }
}

static void setParameter(AEffect* e, int32_t i, float v) {
    Interp* p = P(e);
    switch (i) {
    case P_RECORD: {
        int want = v >= 0.5f;
        if (want && !p->recording) {          /* rising edge: a new take starts, analysed live */
            p->len.store(0);
            p->overruns = 0;
            startLive(p);
        }
        if (!want && p->recording) {          /* falling edge: finish the live analysis */
            p->liveStop = 1;
        }
        p->recording = want;
        p->recordParam = v;
        break;
    }
    case P_ANALYSE:
        if (p->analyseParam < 0.5f && v >= 0.5f) {
            p->recording = 0;
            p->liveStop = 1;
            joinWorker(p);
            startWorker(p, JOB_ANALYSE, p->rec, p->len.load());
        }
        p->analyseParam = v;
        break;
    case P_LINE:
        if (p->lineParam < 0.5f && v >= 0.5f) {
            p->recording = 0;
            p->liveStop = 1;
            joinWorker(p);
            startWorker(p, JOB_LINE, p->rec, p->len.load());
        }
        p->lineParam = v;
        break;
    case P_TRANSPOSE: p->transposeParam = v; break;
    case P_OCTAVE: p->octaveParam = v; break;
    case P_VOICING: p->voicingParam = v; break;
    case P_INVERSION: p->inversionParam = v; break;
    default: break;
    }
}

static float getParameter(AEffect* e, int32_t i) {
    Interp* p = P(e);
    switch (i) {
    case P_RECORD: return p->recordParam;
    case P_ANALYSE: return p->analyseParam;
    case P_TRANSPOSE: return p->transposeParam;
    case P_INVERSION: return p->inversionParam;
    case P_OCTAVE: return p->octaveParam;
    case P_VOICING: return p->voicingParam;
    case P_KEY: return p->keyParam;
    case P_KEYCONF: return p->keyConfParam;
    case P_PROGRESS: return p->progress;
    case P_LINE: return p->lineParam;
    case P_LEVEL: return p->level;
    default:
        if (i >= P_BAND0 && i < P_COUNT) return p->band[i - P_BAND0];
        return 0.f;
    }
}

/* MIDI out. The host sends the events back through audioMasterProcessEvents, once per block. */
static void emitEvents(Interp* p, const ps_MidiEvent* ev, size_t m) {
    if (m == 0) return;
    VstEventList list;
    list.numEvents = (int32_t)m;
    list.reserved = 0;
    for (size_t i = 0; i < m; i++) list.events[i] = (void*)&ev[i];
    p->host(&p->fx, audioMasterProcessEvents, 0, 0, &list, 0.f);
}

/* While the host transport plays, emits the pad hits that fall in this block, counted from when it started.
   When it stops, every note gets a note-off so nothing hangs. Reads the schedule only; never waits. */
static void sendPadEvents(Interp* p, int32_t n) {
    VstTimeInfo* ti = (VstTimeInfo*)p->host(&p->fx, audioMasterGetTime, 0, kVstTransportPlaying, nullptr, 0.f);
    bool playing = ti && (ti->flags & kVstTransportPlaying);
    int act = p->padActive.load();
    if (act < 0) return;
    const ps_note* sched = p->padBuf[act];
    size_t total = p->padN[act];
    if (!playing && p->wasPlaying) {
        for (size_t off = 0; off < total; off += kEvMax) {
            size_t m = ps_all_off(sched + off, total - off < (size_t)kEvMax ? total - off : (size_t)kEvMax, p->evBuf, kEvMax);
            emitEvents(p, p->evBuf, m);
        }
    }
    if (playing && !p->wasPlaying) p->playStart = ti->samplePos;
    p->wasPlaying = playing;
    if (!playing) return;
    long long pos = llround(ti->samplePos - p->playStart);
    size_t m = ps_block_events(sched, total, pos, n, p->sr, p->evBuf, kEvMax);
    emitEvents(p, p->evBuf, m);
}

static void processReplacing(AEffect* e, float** in, float** out, int32_t n) {
    Interp* p = P(e);
    sendPadEvents(p, n);
    float peak = 0.f;
    bool rec = p->recording.load();
    double s1[16] = {0}, s2[16] = {0};
    size_t wr = p->ringWr.load(std::memory_order_relaxed);
    for (int32_t i = 0; i < n; i++) {
        float l = in[0][i], r = in[1][i];
        out[0][i] = l;                        /* pass-through */
        out[1][i] = r;
        float a = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
        if (a > peak) peak = a;
        double m = 0.5 * (l + r);
        for (int k = 0; k < kBands; k++) {          /* Goertzel: energy at each band centre */
            double s0 = m + p->gcoef[k] * s1[k] - s2[k];
            s2[k] = s1[k]; s1[k] = s0;
        }
        if (rec) {
            float mono = 0.5f * (l + r);
            size_t k = p->len.load(std::memory_order_relaxed);
            if (p->rec && k < p->cap) { p->rec[k] = mono; p->len.store(k + 1, std::memory_order_release); }
            if (p->ring) {
                size_t rd = p->ringRd.load(std::memory_order_acquire);
                if (wr - rd < p->ringCap) { p->ring[wr % p->ringCap] = mono; wr++; }
                else p->overruns++;           /* worker is behind: drop the sample, never block */
            }
        }
    }
    if (rec) p->ringWr.store(wr, std::memory_order_release);
    float db = peak > 1e-6f ? 20.0f * log10f(peak) : -120.0f;
    float norm = (db + 60.0f) / 60.0f;
    if (norm < 0.f) norm = 0.f;
    if (norm > 1.f) norm = 1.f;
    float fall = p->level.load() * 0.9f;
    p->level.store(norm > fall ? norm : fall);
    for (int k = 0; k < kBands; k++) {
        double power = s1[k] * s1[k] + s2[k] * s2[k] - p->gcoef[k] * s1[k] * s2[k];
        double amp = n > 0 ? 2.0 * sqrt(fabs(power)) / n : 0.0;      /* amplitude of that band in this block */
        float bdb = amp > 1e-6 ? (float)(20.0 * log10(amp)) : -120.0f;
        float bn = (bdb + 60.0f) / 60.0f;
        if (bn < 0.f) bn = 0.f;
        if (bn > 1.f) bn = 1.f;
        float bfall = p->band[k].load() * 0.85f;
        p->band[k].store(bn > bfall ? bn : bfall);
    }
}

extern "C" __attribute__((visibility("default"))) AEffect* VSTPluginMain(audioMasterCallback host) {
    /* placement new on malloc memory: no libstdc++ allocation calls, so the .so needs only libc, libm and libpthread */
    Interp* p = (Interp*)malloc(sizeof(Interp));
    if (!p) return nullptr;
    new (p) Interp();
    p->host = host;
    AEffect& fx = p->fx;
    memset(&fx, 0, sizeof fx);
    fx.magic = kEffectMagic;
    fx.dispatcher = dispatcher;
    fx.setParameter = setParameter;
    fx.getParameter = getParameter;
    fx.processReplacing = processReplacing;
    fx.numPrograms = 1;
    fx.numParams = P_COUNT;
    fx.numInputs = 2;
    fx.numOutputs = 2;
    fx.flags = effFlagsCanReplacing;
    fx.uniqueID = kUniqueId;
    fx.version = 1000;
    fx.object = p;
    return &fx;
}
