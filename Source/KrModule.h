#pragma once
#include "KrPlatform.h"

typedef struct KrAudioSpec     KrAudioSpec;
typedef struct PL_AudioDevice   PL_AudioDevice;
typedef struct KrAudioContext  KrAudioContext;

typedef struct KrAudioDeviceId {
	void *PlatformSpecific;
} KrAudioDeviceId;

typedef u32  (*KrAudioUpdateProc)(PL_AudioDevice *, KrAudioSpec *, u8 *, u32, void *);
typedef void (*KrAudioResumedProc)(PL_AudioDevice *);
typedef void (*KrAudioPausedProc)(PL_AudioDevice *);
typedef void (*KrAudioResetProc)(PL_AudioDevice *);
typedef void (*KrAudioDeviceLostProc)(PL_AudioDevice *, KrAudioDeviceId);
typedef void (*KrAudioDeviceGainedProc)(PL_AudioDevice *, KrAudioDeviceId);

struct KrAudioContext {
	void *                  UserData;
	KrAudioUpdateProc       Update;
	KrAudioResumedProc      Resumed;
	KrAudioPausedProc       Paused;
	KrAudioResetProc        Reset;
	KrAudioDeviceLostProc   DeviceLost;
	KrAudioDeviceGainedProc DeviceGained;
};

typedef enum KrAudioFormat {
	KrAudioFormat_R32,
	KrAudioFormat_I32,
	KrAudioFormat_I16,
	KrAudioFormat_EnumMax
} KrAudioFormat;

enum KrAudioChannelMaskBits {
	KrAudioChannel_FrontLeft          = 0x00001,
	KrAudioChannel_FrontRight         = 0x00002,
	KrAudioChannel_FrontCenter        = 0x00004,
	KrAudioChannel_LowFrequency       = 0x00008,
	KrAudioChannel_BackLeft           = 0x00010,
	KrAudioChannel_BackRight          = 0x00020,
	KrAudioChannel_FrontLeftOfCenter  = 0x00040,
	KrAudioChannel_FrontRightOfCenter = 0x00080,
	KrAudioChannel_BackCenter         = 0x00100,
	KrAudioChannel_SideLeft           = 0x00200,
	KrAudioChannel_SideRight          = 0x00400,
	KrAudioChannel_TopCenter          = 0x00800,
	KrAudioChannel_TopFrontLeft       = 0x01000,
	KrAudioChannel_TopFrontCenter     = 0x02000,
	KrAudioChannel_TopFrontRight      = 0x04000,
	KrAudioChannel_TopBackLeft        = 0x08000,
	KrAudioChannel_TopBackCenter      = 0x10000,
	KrAudioChannel_TopBackRight       = 0x20000
};

struct KrAudioSpec {
	KrAudioFormat Format;
	u32           Channels;
	u32           ChannelMask;
	u32           SamplesPerSecond;
};

const char16_t * KrGetLastError();

PL_AudioDevice *  KrAudioDeviceOpen(const KrAudioContext *ctx, const KrAudioSpec *spec, const KrAudioDeviceId *id);
void             KrAudioDeviceClose(PL_AudioDevice *device);
bool             KrAudioDeviceIsPlaying(PL_AudioDevice *device);
void             KrAudioDeviceResume(PL_AudioDevice *device);
void             KrAudioDevicePause(PL_AudioDevice *device);
void             KrAudioDeviceReset(PL_AudioDevice *device);
void             KrAudioDeviceUpdate(PL_AudioDevice *device);
