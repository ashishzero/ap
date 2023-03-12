#pragma once
#include "KrMedia.h"

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
	PL_MediaVTable      VTable;
	PL_IoDevice           IoDevice;
	PL_UserVTable         UserVTable;
	PL_AudioDeviceBuffer  AudioDeviceBuffer[PL_AudioEndpoint_EnumCount];
	u32                   Flags;
	u32                   Features;
	PL_Config             Config;
} PL_Media;

extern PL_Media Media;
