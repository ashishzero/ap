#pragma once
#include "KrPlatform.h"

typedef struct KrAudioSpec     KrAudioSpec;
typedef struct KrAudioDevice   KrAudioDevice;
typedef struct KrAudioContext  KrAudioContext;

typedef struct KrAudioDeviceId {
	void *PlatformSpecific;
} KrAudioDeviceId;

typedef u32  (*KrAudioUpdateProc)(KrAudioDevice *, KrAudioSpec *, u8 *, u32, void *);
typedef void (*KrAudioResumedProc)(KrAudioDevice *);
typedef void (*KrAudioPausedProc)(KrAudioDevice *);
typedef void (*KrAudioResetProc)(KrAudioDevice *);
typedef void (*KrAudioDeviceLostProc)(KrAudioDevice *, KrAudioDeviceId);
typedef void (*KrAudioDeviceGainedProc)(KrAudioDevice *, KrAudioDeviceId);

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

enum KrAudioChannelMasks {
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

KrAudioDevice *  KrAudioDeviceOpen(const KrAudioContext *ctx, const KrAudioSpec *spec, const KrAudioDeviceId *id);
void             KrAudioDeviceClose(KrAudioDevice *device);
bool             KrAudioDeviceIsPlaying(KrAudioDevice *device);
void             KrAudioDeviceResume(KrAudioDevice *device);
void             KrAudioDevicePause(KrAudioDevice *device);
void             KrAudioDeviceReset(KrAudioDevice *device);
void             KrAudioDeviceUpdate(KrAudioDevice *device);
