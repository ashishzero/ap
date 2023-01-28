#pragma once
#include "KrMedia.h"

typedef bool (*KrAudio_IsPlayingProc)();
typedef void (*KrAudio_Update)();
typedef void (*KrAudio_Resume)();
typedef void (*KrAudio_Pause)();
typedef void (*KrAudio_Reset)();
typedef bool (*KrAudio_SetRenderDevice)(KrAudioDeviceId id);
typedef uint (*KrAudio_GetDevices)(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap);

typedef struct KrAudioLibrary {
	KrAudio_IsPlayingProc   IsPlaying;
	KrAudio_Update          Update;
	KrAudio_Resume          Resume;
	KrAudio_Pause           Pause;
	KrAudio_Reset           Reset;
	KrAudio_SetRenderDevice SetRenderDevice;
	KrAudio_GetDevices      GetDevices;
} KrAudioLibrary;

typedef int (*KrMain_Run)();

typedef struct KrMain_Library {
	KrMain_Run Run;
} KrMain_Library;

typedef struct KrLibrary {
	KrAudioLibrary Audio;
	KrMain_Library Main;
} KrLibrary;

extern KrLibrary    g_Library;
extern KrUserConfig g_UserConfig;
