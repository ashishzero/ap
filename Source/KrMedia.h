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
	KrAudioDeviceFlow_All
} KrAudioDeviceFlow;

typedef struct KrAudioDeviceInfo {
	KrAudioDeviceId Id;
	const char *    Name;
} KrAudioDeviceInfo;

proc bool KrAudio_IsPlaying();
proc void KrAudio_Update();
proc void KrAudio_Resume();
proc void KrAudio_Pause();
proc void KrAudio_Reset();
proc bool KrAudio_SetRenderDevice(KrAudioDeviceId id);
proc uint KrAudio_GetDeviceList(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap);
proc bool KrAudio_GetEffectiveDevice(KrAudioDeviceInfo *output);

//
// [Window]
//

bool KrWindow_IsFullscreen();
void KrWindow_ToggleFullscreen();

//
// [Keyboard/Mouse]
//

typedef enum KrButton {
	KrButton_Left,
	KrButton_Right,
	KrButton_Middle,
	KrButton_EnumMax
} KrButton;

typedef enum KrKey {
	KrKey_Unknown,
	KrKey_A, KrKey_B, KrKey_C, KrKey_D, KrKey_E, KrKey_F, KrKey_G, KrKey_H,
	KrKey_I, KrKey_J, KrKey_K, KrKey_L, KrKey_M, KrKey_N, KrKey_O, KrKey_P,
	KrKey_Q, KrKey_R, KrKey_S, KrKey_T, KrKey_U, KrKey_V, KrKey_W, KrKey_X,
	KrKey_Y, KrKey_Z,
	KrKey_0, KrKey_1, KrKey_2, KrKey_3, KrKey_4,
	KrKey_5, KrKey_6, KrKey_7, KrKey_8, KrKey_9,
	KrKey_Return, KrKey_Escape, KrKey_Backspace, KrKey_Tab, KrKey_Space, KrKey_Shift, KrKey_Ctrl,
	KrKey_F1, KrKey_F2, KrKey_F3, KrKey_F4, KrKey_F5, KrKey_F6,
	KrKey_F7, KrKey_F8, KrKey_F9, KrKey_F10, KrKey_F11, KrKey_F12,
	KrKey_PrintScreen, KrKey_Insert, KrKey_Home,
	KrKey_PageUp, KrKey_PageDown, KrKey_Delete, KrKey_End,
	KrKey_Right, KrKey_Left, KrKey_Down, KrKey_Up,
	KrKey_Divide, KrKey_Multiply, KrKey_Minus, KrKey_Plus,
	KrKey_Period, KrKey_BackTick,
	KrKey_EnumMax
} KrKey;

//
// [Events]
//

typedef enum KrEventKind {
	KrEvent_Startup,
	KrEvent_Quit,
	KrEvent_WindowCreated,
	KrEvent_WindowDestroyed,
	KrEvent_WindowActivated,
	KrEvent_WindowDeactivated,
	KrEvent_WindowResized,

	KrEvent_MouseMoved,
	KrEvent_ButtonPressed,
	KrEvent_ButtonReleased,
	KrEvent_DoubleClicked,
	KrEvent_WheelMoved,

	KrEvent_KeyPressed,
	KrEvent_KeyReleased,
	KrEvent_TextInput,

	KrEvent_AudioResumed,
	KrEvent_AudioPaused,
	KrEvent_AudioReset,
	KrEvent_AudioRenderDeviceChanged,
	KrEvent_AudioRenderDeviceLost,

	KrEvent_AudioRenderDeviceActived,
	KrEvent_AudioRenderDeviceDeactived,

	KrEvent_EnumMax
}  KrEventKind;

typedef struct KrEvent {
	KrEventKind Kind;

	union {
		struct {
			i32 Width, Height;
		} Window;
		struct {
			i32 X, Y;
			KrButton Button;
		} Mouse;
		struct {
			i32 X, Y;
			float Vert, Horz;
		} Wheel;
		struct {
			KrKey Code;
			i32   Repeat;
		} Key;
		struct {
			u16 Code;
		} Text;
		struct {
			KrAudioDeviceId Id;
			const char *    Name;
		} AudioDevice;
	};
} KrEvent;

const char *KrEvent_GetName(KrEventKind kind);

//
//
//

typedef void (*KrEventProc)(const KrEvent *event, void *user);
typedef void (*KrUpdateProc)(float w, float h, void *user);
typedef u32  (*KrUploadAudioProc)(const KrAudioSpec *spec, u8 *data, u32 count, void *user);

typedef struct KrUserContext {
	void *            Data;
	KrEventProc       OnEvent;
	KrUpdateProc      OnUpdate;
	KrUploadAudioProc OnUploadAudio;
} KrUserContext;

int Main(int argc, char **argv, KrUserContext *user);
