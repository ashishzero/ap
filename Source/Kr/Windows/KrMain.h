#pragma once
#include <windows.h>
#include "../KrMediaInternal.h"

#define PL_WM_AUDIO_RESUMED            (WM_USER + 1)
#define PL_WM_AUDIO_PAUSED             (WM_USER + 2)
#define PL_WM_AUDIO_DEVICE_CHANGED     (WM_USER + 3)
#define PL_WM_AUDIO_DEVICE_LOST        (WM_USER + 4)
#define PL_WM_AUDIO_DEVICE_ACTIVATED   (WM_USER + 5)
#define PL_WM_AUDIO_DEVICE_DEACTIVATED (WM_USER + 6)

typedef struct PL_AudioDeviceNative PL_AudioDeviceNative;

typedef struct PL_AudioDeviceNative {
	PL_AudioDeviceNative *Next;
	LPCWSTR               Id;
	LONG volatile         IsCapture;
	LONG volatile         IsActive;
	char                  *Name;
	char                  NameBufferA[64];
	char                  NameBufferB[64];
} PL_AudioDeviceNative;

int         PL_UTF16ToUTF8(char *utf8_buff, int utf8_buff_len, const char16_t *utf16_string);
const char *PL_GetLastError(void);
void        PL_InitThreadContext();
