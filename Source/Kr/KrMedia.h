#pragma once

#include "KrPlatform.h"

typedef enum PL_Button {
	PL_Button_Left,
	PL_Button_Right,
	PL_Button_Middle,
	PL_Button_EnumCount
} PL_Button;

typedef enum PL_Key {
	PL_Key_Unknown,
	PL_Key_A, PL_Key_B, PL_Key_C, PL_Key_D, PL_Key_E, PL_Key_F, PL_Key_G, PL_Key_H,
	PL_Key_I, PL_Key_J, PL_Key_K, PL_Key_L, PL_Key_M, PL_Key_N, PL_Key_O, PL_Key_P,
	PL_Key_Q, PL_Key_R, PL_Key_S, PL_Key_T, PL_Key_U, PL_Key_V, PL_Key_W, PL_Key_X,
	PL_Key_Y, PL_Key_Z,
	PL_Key_0, PL_Key_1, PL_Key_2, PL_Key_3, PL_Key_4,
	PL_Key_5, PL_Key_6, PL_Key_7, PL_Key_8, PL_Key_9,
	PL_Key_Return, PL_Key_Escape, PL_Key_Backspace, PL_Key_Tab, PL_Key_Space, PL_Key_Shift, PL_Key_Ctrl,
	PL_Key_F1, PL_Key_F2, PL_Key_F3, PL_Key_F4, PL_Key_F5, PL_Key_F6,
	PL_Key_F7, PL_Key_F8, PL_Key_F9, PL_Key_F10, PL_Key_F11, PL_Key_F12,
	PL_Key_PrintScreen, PL_Key_Insert, PL_Key_Home,
	PL_Key_PageUp, PL_Key_PageDown, PL_Key_Delete, PL_Key_End,
	PL_Key_Right, PL_Key_Left, PL_Key_Down, PL_Key_Up,
	PL_Key_Divide, PL_Key_Multiply, PL_Key_Minus, PL_Key_Plus,
	PL_Key_Period, PL_Key_BackTick,
	PL_Key_EnumCount
} PL_Key;

typedef enum PL_AudioEndpointKind {
	PL_AudioEndpoint_Render,
	PL_AudioEndpoint_Capture,
	PL_AudioEndpoint_EnumCount
} PL_AudioEndpointKind;

typedef enum PL_EventKind {
	PL_Event_Startup,
	PL_Event_Terminate,

	PL_Event_WindowCreated,
	PL_Event_WindowDestroyed,
	PL_Event_WindowActivated,
	PL_Event_WindowDeactivated,
	PL_Event_WindowResized,
	PL_Event_WindowClosed,

	PL_Event_MouseMoved,
	PL_Event_ButtonPressed,
	PL_Event_ButtonReleased,
	PL_Event_DoubleClicked,
	PL_Event_WheelMoved,

	PL_Event_KeyPressed,
	PL_Event_KeyReleased,
	PL_Event_TextInput,

	PL_Event_AudioResumed,
	PL_Event_AudioPaused,
	PL_Event_AudioReset,
	PL_Event_AudioDeviceChanged,
	PL_Event_AudioDeviceLost,

	PL_Event_AudioDeviceActived,
	PL_Event_AudioDeviceDeactived,

	PL_Event_EnumCount
}  PL_EventKind;

typedef struct PL_Window PL_Window;

typedef struct PL_WindowEvent {
	PL_Window *Target;
	i32 Width, Height;
} PL_WindowEvent;

typedef struct PL_CursorEvent {
	i32 PosX, PosY;
} PL_CursorEvent;

typedef struct PL_ButtonEvent {
	i32 CursorX, CursorY;
	PL_Button Sym;
} PL_ButtonEvent;

typedef struct PL_WheelEvent {
	i32 CursorX, CursorY;
	float Horz, Vert;
} PL_WheelEvent;

typedef struct PL_KeyEvent {
	PL_Key Sym;
	i32    Repeat;
} PL_KeyEvent;

typedef struct PL_TextEvent {
	u32 Codepoint;
} PL_TextEvent;

typedef struct PL_AudioEvent {
	const void *         NativeHandle;
	const char *         Name;
	PL_AudioEndpointKind Endpoint;
} PL_AudioEvent;

typedef struct PL_Event {
	PL_EventKind       Kind;
	union {
		PL_WindowEvent Window;
		PL_ButtonEvent Button;
		PL_CursorEvent Cursor;
		PL_WheelEvent  Wheel;
		PL_KeyEvent    Key;
		PL_TextEvent   Text;
		PL_AudioEvent  Audio;
	};
} PL_Event;

typedef enum PL_AudioFormat {
	PL_AudioFormat_R32,
	PL_AudioFormat_I32,
	PL_AudioFormat_I16,
	PL_AudioFormat_EnumCount
} PL_AudioFormat;

typedef struct PL_AudioSpec {
	PL_AudioFormat Format;
	u32            Channels;
	u32            Frequency;
} PL_AudioSpec;

typedef void (*PL_EventProc)(PL_Event const *, void *);
typedef void (*PL_UpdateProc)(struct PL_IoDevice const *, void *);
typedef u32  (*PL_AudioRenderProc)(PL_AudioSpec const *, u8 *, u32, void *);
typedef u32  (*PL_AudioCaptureProc)(PL_AudioSpec const *, u8 *, u32, void *);

typedef struct PL_UserVTable {
	void *              Data;
	PL_EventProc        OnEvent;
	PL_UpdateProc       OnUpdate;
	PL_AudioRenderProc  OnAudioRender;
	PL_AudioCaptureProc OnAudioCapture;
} PL_UserVTable;

typedef struct PL_KeyState {
	u8 Down;
	u8 Pressed;
	u8 Released;
	u8 Transitions;
} PL_KeyState;

typedef struct PL_Keyboard {
	PL_KeyState Keys[PL_Key_EnumCount];
} PL_Keyboard;

typedef struct PL_MouseButtonState {
	u8 Down;
	u8 Pressed;
	u8 Released;
	u8 Transitions;
	u8 DoubleClicked;
} PL_MouseButtonState;

typedef struct PL_Axis {
	float X, Y;
} PL_Axis;

typedef struct PL_Mouse {
	PL_Axis             Cursor;
	PL_Axis             DeltaCursor;
	PL_Axis             Wheel;
	PL_MouseButtonState Buttons[PL_Button_EnumCount];
} PL_Mouse;

typedef struct PL_AudioDevice {
	void *Handle;
	char *Name;
} PL_AudioDevice;

typedef struct PL_AudioDeviceList {
	imem            Count;
	PL_AudioDevice *Data;
	PL_AudioDevice *Current;
	bool            Default;
} PL_AudioDeviceList;

typedef struct PL_IoDevice {
	PL_Keyboard        Keyboard;
	PL_Mouse           Mouse;
	PL_AudioDeviceList AudioRenderDevices;
	PL_AudioDeviceList AudioCaptureDevices;
} PL_IoDevice;

extern int      Main(int, char **);

i32             PL_AtomicAdd(volatile i32 *dst, i32 val);
i32             PL_AtomicCmpExg(volatile i32 *dst, i32 exchange, i32 compare);
void *          PL_AtomicCmpExgPtr(void *volatile *dst, void *exchange, void *compare);
i32             PL_AtomicExg(volatile i32 *dst, i32 val);
void            PL_AtomicLock(volatile i32 *lock);
void            PL_AtomicUnlock(volatile i32 *lock);

void            PL_InitAudio(bool render, bool capture);
void            PL_ReleaseAudioRender(void);
void            PL_ReleaseAudioCapture(void);
void            PL_ReleaseAudio(void);

void            PL_CreateWindow(const char *title, uint w, uint h, bool fullscreen);
void            PL_DestroyWindow(void);

void            PL_Terminate(int code);
void            PL_PostTerminateMessage(void);

PL_UserVTable   PL_GetUserVTable(void);
void            PL_SetUserVTable(PL_UserVTable vtbl);

bool            PL_IsFullscreen(void);
void            PL_ToggleFullscreen(void);
bool            PL_IsAudioRendering(void);
void            PL_UpdateAudioRender(void);
void            PL_PauseAudioRender(void);
void            PL_ResumeAudioRender(void);
void            PL_ResetAudioRender(void);
bool            PL_IsAudioCapturing(void);
void            PL_PauseAudioCapture(void);
void            PL_ResumeAudioCapture(void);
void            PL_ResetAudioCapture(void);
void            PL_SetAudioDevice(PL_AudioDevice const *device);
