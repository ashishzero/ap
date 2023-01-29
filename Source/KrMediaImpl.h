#pragma once
#include "KrMedia.h"

typedef bool (*KrAudio_IsPlayingProc)();
typedef void (*KrAudio_UpdateProc)();
typedef void (*KrAudio_ResumeProc)();
typedef void (*KrAudio_PauseProc)();
typedef void (*KrAudio_ResetProc)();
typedef bool (*KrAudio_SetRenderDeviceProc)(KrAudioDeviceId id);
typedef uint (*KrAudio_GetDeviceListProc)(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap);
typedef bool (*KrAudio_GetEffectiveDeviceProc)(KrAudioDeviceInfo *output);

typedef struct KrAudioLibrary {
	KrAudio_IsPlayingProc          IsPlaying;
	KrAudio_UpdateProc             Update;
	KrAudio_ResumeProc             Resume;
	KrAudio_PauseProc              Pause;
	KrAudio_ResetProc              Reset;
	KrAudio_SetRenderDeviceProc    SetRenderDevice;
	KrAudio_GetDeviceListProc      GetDeviceList;
	KrAudio_GetEffectiveDeviceProc GetEffectiveDevice;
} KrAudioLibrary;

typedef bool (*KrWindow_IsFullscreenProc)();
typedef void (*KrWindow_ToggleFullscreenProc)();

typedef struct KrWindowLibrary {
	KrWindow_IsFullscreenProc     IsFullscreen;
	KrWindow_ToggleFullscreenProc ToggleFullscreen;
} KrWindowLibrary;

typedef struct KrLibrary {
	KrAudioLibrary  Audio;
	KrWindowLibrary Window;
} KrLibrary;

extern const KrLibrary LibraryFallback;
extern KrLibrary       g_Library;
extern KrUserContext   g_UserContext;
