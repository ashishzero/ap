#pragma once
#include "KrMedia.h"

bool PL_Media_Fallback_IsFullscreen();
void PL_Media_Fallback_ToggleFullscreen();
bool PL_Media_Fallback_IsAudioRendering();
void PL_Media_Fallback_UpdateAudioRender();
void PL_Media_Fallback_PauseAudioRender();
void PL_Media_Fallback_ResumeAudioRender();
void PL_Media_Fallback_ResetAudioRender();
bool PL_Media_Fallback_IsAudioCapturing();
void PL_Media_Fallback_PauseAudioCapture();
void PL_Media_Fallback_ResumeAudioCapture();
void PL_Media_Fallback_ResetAudioCapture();
void PL_Media_Fallback_SetAudioDevice(PL_AudioDevice const *device);

#define PL_MAX_ACTIVE_AUDIO_DEVICE 1024

typedef struct PL_AudioDeviceBuffer {
	PL_AudioDevice Data[PL_MAX_ACTIVE_AUDIO_DEVICE];
} PL_AudioDeviceBuffer;

typedef struct PL_MediaVTable {
	bool (*IsFullscreen)       ();
	void (*ToggleFullscreen)   ();
	bool (*IsAudioRendering)   ();
	void (*UpdateAudioRender)  ();
	void (*PauseAudioRender)   ();
	void (*ResumeAudioRender)  ();
	void (*ResetAudioRender)   ();
	bool (*IsAudioCapturing)   ();
	void (*PauseAudioCapture)  ();
	void (*ResumeAudioCapture) ();
	void (*ResetAudioCapture)  ();
	void (*SetAudioDevice)     (PL_AudioDevice const *);
} PL_MediaVTable;

typedef struct PL_Media {
	PL_MediaVTable        VTable;
	PL_UserVTable         UserVTable;
	PL_IoDevice           IoDevice;
	PL_AudioDeviceBuffer  AudioDeviceBuffer[PL_AudioEndpoint_EnumCount];
} PL_Media;

extern PL_Media Media;
