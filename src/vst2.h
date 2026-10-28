// Minimal hand-written VST 2.4 ABI declarations (clean-room, no Steinberg SDK needed).
// Only what this plugin uses. Layout matches the binary interface VST2 hosts expect.
#pragma once
#include <stdint.h>

struct AEffect;
typedef intptr_t (*audioMasterCallback)(AEffect*, int32_t, int32_t, intptr_t, void*, float);
typedef intptr_t (*AEffectDispatcherProc)(AEffect*, int32_t, int32_t, intptr_t, void*, float);
typedef void (*AEffectProcessProc)(AEffect*, float**, float**, int32_t);
typedef void (*AEffectProcessDoubleProc)(AEffect*, double**, double**, int32_t);
typedef void (*AEffectSetParameterProc)(AEffect*, int32_t, float);
typedef float (*AEffectGetParameterProc)(AEffect*, int32_t);

struct AEffect {
    int32_t magic;                       // 'VstP'
    AEffectDispatcherProc dispatcher;
    AEffectProcessProc process;          // deprecated accumulating process
    AEffectSetParameterProc setParameter;
    AEffectGetParameterProc getParameter;
    int32_t numPrograms, numParams, numInputs, numOutputs;
    int32_t flags;
    intptr_t resvd1, resvd2;
    int32_t initialDelay;
    int32_t realQualities, offQualities;
    float ioRatio;
    void* object;
    void* user;
    int32_t uniqueID;
    int32_t version;
    AEffectProcessProc processReplacing;
    AEffectProcessDoubleProc processDoubleReplacing;
    char future[56];
};

enum {
    kEffectMagic = 0x56737450,          // 'VstP'
    effFlagsHasEditor = 1 << 0,
    effFlagsCanReplacing = 1 << 4,
    effFlagsProgramChunks = 1 << 5,
    effFlagsIsSynth = 1 << 8,
    effFlagsNoSoundInStop = 1 << 9,

    effOpen = 0, effClose = 1, effSetProgram = 2, effGetProgram = 3, effSetProgramName = 4,
    effGetProgramName = 5, effGetParamLabel = 6, effGetParamDisplay = 7, effGetParamName = 8,
    effSetSampleRate = 10, effSetBlockSize = 11, effMainsChanged = 12, effEditGetRect = 13,
    effGetChunk = 23, effSetChunk = 24, effProcessEvents = 25, effCanBeAutomated = 26,
    effString2Parameter = 27, effGetProgramNameIndexed = 29, effGetInputProperties = 33,
    effGetOutputProperties = 34, effGetPlugCategory = 35, effSetSpeakerArrangement = 42,
    effSetBypass = 44, effGetEffectName = 45, effGetVendorString = 47, effGetProductString = 48,
    effGetVendorVersion = 49, effVendorSpecific = 50, effCanDo = 51, effGetTailSize = 52,
    effGetVstVersion = 58, effStartProcess = 71, effStopProcess = 72,

    audioMasterAutomate = 0, audioMasterVersion = 1,

    kPlugCategEffect = 1,
    kVstMaxProgNameLen = 24, kVstMaxParamStrLen = 8, kVstMaxEffectNameLen = 32,
    kVstMaxVendorStrLen = 64, kVstMaxProductStrLen = 64
};
