#pragma once
#include "KrPlatform.h"

//
// [Atomics]
//

proc i32   KrAtomicAdd(volatile i32 *dst, i32 val);
proc i32   KrAtomicCmpExg(volatile i32 *dst, i32 exchange, i32 compare);
proc void *KrAtomicCmpExgPtr(volatile void **dst, void *exchange, void *compare);
proc i32   KrAtomicExg(volatile i32 *dst, i32 val);
proc void  KrAtomicLock(volatile i32 *lock);
proc void  KrAtomicUnlock(volatile i32 *lock);

//
// [Audio]
//

typedef enum KrAudioFormat {
	KrAudioFormat_R32,
	KrAudioFormat_I32,
	KrAudioFormat_I16,
	KrAudioFormat_EnumMax
} KrAudioFormat;

typedef struct KrAudioSpec {
	KrAudioFormat Format;
	u32           Channels;
	u32           Frequency;
} KrAudioSpec;

typedef void * KrAudioDeviceId;

typedef enum KrAudioDeviceFlow {
	KrAudioDeviceFlow_Render,
	KrAudioDeviceFlow_Capture,
	KrAudioDeviceFlow_EnumMax
} KrAudioDeviceFlow;

typedef struct KrAudioDeviceInfo {
	KrAudioDeviceId Id;
	const char *    Name;
} KrAudioDeviceInfo;

proc bool KrAudioIsPlaying();
proc void KrAudioUpdate();
proc void KrAudioResume();
proc void KrAudioPause();
proc void KrAudioReset();
proc bool KrAudioSetRenderDevice(KrAudioDeviceId id);
proc uint KrAudioGetDevices(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap);

typedef u32(*KrUploadAudioProc)(const KrAudioSpec *spec, u8 *data, u32 count, void *user);

typedef struct KrUserConfig {
	void *            Data;
	KrUploadAudioProc OnUploadAudio;
} KrUserConfig;

KrUserConfig *KrUserConfigGet();
int           KrRun();

int Main(int argc, char **argv);
