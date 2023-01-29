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

typedef struct KrLibrary {
	KrAudioLibrary Audio;
} KrLibrary;

extern const KrLibrary LibraryFallback;
extern KrLibrary       g_Library;
extern KrUserContext   g_UserContext;
