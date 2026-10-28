/* Turns a schedule of pad notes into VST MIDI events, one audio block at a time. Pure code: no host calls.
   The event layout is the VST 2.4 VstMidiEvent (32 bytes). */
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "musicout.h"

enum { ps_kVstMidiType = 1 };

typedef struct {
    int32_t type;                        /* ps_kVstMidiType */
    int32_t byteSize;                    /* sizeof(ps_MidiEvent) */
    int32_t deltaFrames;                 /* offset inside the audio block */
    int32_t flags;
    int32_t noteLength, noteOffset;
    char midiData[4];                    /* status, data1, data2, 0 */
    char detune, noteOffVelocity, reserved1, reserved2;
} ps_MidiEvent;

/* Writes the note-on and note-off events that fall inside [blockStart, blockStart + blockSize) of the schedule.
   Times in the schedule are seconds from the start of playback; blockStart is in samples at rate sr.
   Returns the number of events written (at most cap). */
size_t ps_block_events(const vi_note* sched, size_t n, long long blockStart, int blockSize, double sr,
                       ps_MidiEvent* out, size_t cap);
