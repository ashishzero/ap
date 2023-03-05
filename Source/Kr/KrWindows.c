#include "KrMedia.h"

#pragma warning(push)
#pragma warning(disable: 5105)

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>
#include <VersionHelpers.h>
#include <Functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Avrt.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Dwmapi.lib")

#pragma warning(pop)

//
// Atomics
//

static_assert(sizeof(i32) == sizeof(LONG), "");

i32 PL_AtomicAdd(volatile i32 *dst, i32 val) {
	return InterlockedAdd(dst, val);
}

i32 PL_AtomicCmpExg(volatile i32 *dst, i32 exchange, i32 compare) {
	return InterlockedCompareExchange(dst, exchange, compare);
}

void *PL_AtomicCmpExgPtr(void *volatile *dst, void *exchange, void *compare) {
	return InterlockedCompareExchangePointer(dst, exchange, compare);
}

i32 PL_AtomicExg(volatile i32 *dst, i32 val) {
	return InterlockedExchange(dst, val);
}

void PL_AtomicLock(volatile i32 *lock) {
	while (InterlockedCompareExchange(lock, 1, 0) == 1);
}

void PL_AtomicUnlock(volatile i32 *lock) {
	InterlockedExchange(lock, 0);
}

//
// Unicode
//

static int PL_UTF16ToUTF8(char *utf8_buff, int utf8_buff_len, const char16_t *utf16_string) {
	int bytes_written = WideCharToMultiByte(CP_UTF8, 0, utf16_string, -1, utf8_buff, utf8_buff_len, 0, 0);
	return bytes_written;
}

//
// Error
//

static thread_local char LastErrorBuff[4096];

static const char *PL_GetLastError(void) {
	DWORD error = GetLastError();
	if (error) {
		LPWSTR message = 0;
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&message, 0, NULL);
		PL_UTF16ToUTF8(LastErrorBuff, sizeof(LastErrorBuff), message);
		LocalFree(message);
	}
	return "unknown";
}

//
// Thread Context
//

#include <stdio.h>

static void PL_LogEx(void *data, LogKind kind, const char *fmt, va_list args) {
	static volatile LONG Guard = 0;

	static const WORD ColorsMap[] = {
		FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,
		FOREGROUND_GREEN,
		FOREGROUND_RED | FOREGROUND_GREEN,
		FOREGROUND_RED
	};
	_Static_assert(ArrayCount(ColorsMap) == LogKind_Error + 1, "");

	char    buff[KB(16)];
	wchar_t buff16[KB(16)];

	int len   = vsnprintf(buff, ArrayCount(buff), fmt, args);
	int len16 = MultiByteToWideChar(CP_UTF8, 0, buff, -1, buff16, ArrayCount(buff16));

	while (InterlockedCompareExchange(&Guard, 1, 0) == 1) {
		// spin lock
	}

	HANDLE handle = kind == LogKind_Error ?
		GetStdHandle(STD_ERROR_HANDLE) :
		GetStdHandle(STD_OUTPUT_HANDLE);

	if (handle != INVALID_HANDLE_VALUE) {
		CONSOLE_SCREEN_BUFFER_INFO buffer_info;
		GetConsoleScreenBufferInfo(handle, &buffer_info);
		SetConsoleTextAttribute(handle, ColorsMap[kind]);
		DWORD written = 0;
		WriteConsoleW(handle, buff16, len16, &written, 0);
		SetConsoleTextAttribute(handle, buffer_info.wAttributes);
	}

	if (IsDebuggerPresent())
		OutputDebugStringW(buff16);

	InterlockedCompareExchange(&Guard, 0, 1);
}

static void PL_HandleAssertion(const char *file, int line, const char *proc, const char *string) {
	LogError("Assertion Failed: %s(%d): %s : %s\n", file, line, proc, string);
	TriggerBreakpoint();
}

static void PL_FatalError(const char *message) {
	int wlen     = MultiByteToWideChar(CP_UTF8, 0, message, (int)strlen(message), NULL, 0);
	wchar_t *msg = (wchar_t *)HeapAlloc(GetProcessHeap(), 0, (wlen + 1) * sizeof(wchar_t));
	if (msg) {
		MultiByteToWideChar(CP_UTF8, 0, message, (int)strlen(message), msg, wlen + 1);
		msg[wlen] = 0;
	}
	FatalAppExitW(0, msg);
}

static void PL_InitThreadContext() {
	Thread.Logger.Handle = PL_LogEx;
	Thread.OnAssertion   = PL_HandleAssertion;
	Thread.OnFatalError  = PL_FatalError;
}

//
// Media
//

#define PL_MAX_ACTIVE_AUDIO_DEVICE 1024
#define PL_WM_AUDIO_RESUMED            (WM_USER + 1)
#define PL_WM_AUDIO_PAUSED             (WM_USER + 2)
#define PL_WM_AUDIO_DEVICE_CHANGED     (WM_USER + 3)
#define PL_WM_AUDIO_DEVICE_LOST        (WM_USER + 4)
#define PL_WM_AUDIO_DEVICE_ACTIVATED   (WM_USER + 5)
#define PL_WM_AUDIO_DEVICE_DEACTIVATED (WM_USER + 6)

typedef struct PL_Window {
	HWND            Handle;
	WINDOWPLACEMENT Placement;
	u16             LastText;
} PL_Window;

typedef struct PL_AudioDeviceNative PL_AudioDeviceNative;

typedef struct PL_AudioDeviceNative {
	PL_AudioDeviceNative *Next;
	LPCWSTR               Id;
	LONG volatile         Age;
	LONG volatile         IsCapture;
	LONG volatile         IsActive;
	char                  *Name;
	char                  NameBuff0[64];
	char                  NameBuff1[64];
} PL_AudioDeviceNative;

typedef enum PL_AudioCommand {
	PL_AudioCommand_Update,
	PL_AudioCommand_Resume,
	PL_AudioCommand_Pause,
	PL_AudioCommand_Reset,
	PL_AudioCommand_LoadDefault,
	PL_AudioCommand_LoadDesired,
	PL_AudioCommand_Terminate,
	PL_AudioCommand_EnumCount
} PL_AudioCommand;

typedef struct PL_AudioEndpoint {
	IAudioClient         * Client;
	IAudioRenderClient   * Render;
	IAudioCaptureClient  * Capture;
	UINT                   MaxFrames;
	PL_AudioSpec           Spec;
	HANDLE                 Commands[PL_AudioCommand_EnumCount];
	LONG volatile          Age;
	LONG volatile          DeviceLost;
	LONG volatile          Resumed;
	HANDLE                 Thread;
	PL_AudioDeviceNative * DesiredDevice;
	PL_AudioDeviceNative * EffectiveDevice;
} PL_AudioEndpoint;

typedef struct PL_Audio {
	i32 volatile           Lock;
	PL_AudioDeviceNative   FirstDevice;
	IMMDeviceEnumerator  * DeviceEnumerator;
	PL_AudioEndpoint       Endpoints[PL_AudioEndpoint_EnumCount];
} PL_Audio;

typedef struct PL_AudioDeviceBuffer {
	PL_AudioDevice Data[PL_MAX_ACTIVE_AUDIO_DEVICE];
} PL_AudioDeviceBuffer;

typedef struct PL_ContextVTable {
	bool (*IsFullscreen)      (void);
	void (*ToggleFullscreen)  (void);
	bool (*IsAudioRendering)  (void);
	void (*UpdateAudioRender) (void);
	void (*PauseAudioRender)  (void);
	void (*ResumeAudioRender) (void);
	void (*ResetAudioRender)  (void);
	void (*SetAudioDevice)    (PL_AudioDevice const *);
} PL_ContextVTable;

typedef struct PL_Context {
	PL_ContextVTable      VTable;
	PL_IoDevice           IoDevice;

	PL_Window             Window;
	PL_Audio              Audio;

	PL_UserVTable         UserVTable;

	PL_AudioDeviceBuffer  AudioDeviceBuffer[PL_AudioEndpoint_EnumCount];

	u32                   Flags;
	u32                   Features;
	PL_Config             Config;
} PL_Context;

static bool PL_Media_Fallback_IsFullscreen(void)                           { return false; }
static void PL_Media_Fallback_ToggleFullscreen(void)                       { }
static bool PL_Media_Fallback_IsAudioRendering(void)                       { return false; }
static void PL_Media_Fallback_UpdateAudioRender(void)                      {}
static void PL_Media_Fallback_PauseAudioRender(void)                       {}
static void PL_Media_Fallback_ResumeAudioRender(void)                      {}
static void PL_Media_Fallback_ResetAudioRender(void)                       {}
static void PL_Media_Fallback_SetAudioDevice(PL_AudioDevice const *device) {}

static void PL_User_Fallback_OnEvent(PL_Event const *event, void *data)                               {}
static void PL_User_Fallback_OnUpdate(PL_IoDevice const *io, void *data)                              {}
static u32  PL_User_Fallback_OnAudioRender(PL_AudioSpec const *spec, u8 *out, u32 frames, void *data) { return 0; }

static PL_Context Context = {
	.VTable = {
		.IsFullscreen         = PL_Media_Fallback_IsFullscreen,
		.ToggleFullscreen     = PL_Media_Fallback_ToggleFullscreen,
		.IsAudioRendering     = PL_Media_Fallback_IsAudioRendering,
		.UpdateAudioRender    = PL_Media_Fallback_UpdateAudioRender,
		.PauseAudioRender     = PL_Media_Fallback_PauseAudioRender,
		.ResumeAudioRender    = PL_Media_Fallback_ResumeAudioRender,
		.ResetAudioRender     = PL_Media_Fallback_ResetAudioRender,
		.SetAudioDevice       = PL_Media_Fallback_SetAudioDevice,
	},
	.UserVTable = {
		.OnEvent              = PL_User_Fallback_OnEvent,
		.OnUpdate             = PL_User_Fallback_OnUpdate,
		.OnAudioRender        = PL_User_Fallback_OnAudioRender
	}
};

//
// Public API
//

void PL_SetConfig(PL_Config const *config) {
	Context.Config = *config;

	if (!Context.Config.User.OnEvent)
		Context.Config.User.OnEvent = PL_User_Fallback_OnEvent;

	if (!Context.Config.User.OnUpdate)
		Context.Config.User.OnUpdate = PL_User_Fallback_OnUpdate;

	if (!Context.Config.User.OnAudioRender)
		Context.Config.User.OnAudioRender = PL_User_Fallback_OnAudioRender;
}

bool PL_IsFullscreen(void) {
	return Context.VTable.IsFullscreen();
}

void PL_ToggleFullscreen(void) {
	Context.VTable.ToggleFullscreen();
}

bool PL_IsAudioRendering(void) {
	return Context.VTable.IsAudioRendering();
}

void PL_UpdateAudioRender(void) {
	Context.VTable.UpdateAudioRender();
}

void PL_PauseAudioRender(void) {
	Context.VTable.PauseAudioRender();
}

void PL_ResumeAudioRender(void) {
	Context.VTable.ResumeAudioRender();
}

void PL_ResetAudioRender(void) {
	Context.VTable.ResetAudioRender();
}

void PL_SetAudioDevice(PL_AudioDevice const *device) {
	Context.VTable.SetAudioDevice(device);
}

//
// Window
//

static const PL_Key VirtualKeyMap[] = {
	['A'] = PL_Key_A, ['B'] = PL_Key_B,
	['C'] = PL_Key_C, ['D'] = PL_Key_D,
	['E'] = PL_Key_E, ['F'] = PL_Key_F,
	['G'] = PL_Key_G, ['H'] = PL_Key_H,
	['I'] = PL_Key_I, ['J'] = PL_Key_J,
	['K'] = PL_Key_K, ['L'] = PL_Key_L,
	['M'] = PL_Key_M, ['N'] = PL_Key_N,
	['O'] = PL_Key_O, ['P'] = PL_Key_P,
	['Q'] = PL_Key_Q, ['R'] = PL_Key_R,
	['S'] = PL_Key_S, ['T'] = PL_Key_T,
	['U'] = PL_Key_U, ['V'] = PL_Key_V,
	['W'] = PL_Key_W, ['X'] = PL_Key_X,
	['Y'] = PL_Key_Y, ['Z'] = PL_Key_Z,

	['0'] = PL_Key_0, ['1'] = PL_Key_1,
	['2'] = PL_Key_2, ['3'] = PL_Key_3,
	['4'] = PL_Key_4, ['5'] = PL_Key_5,
	['6'] = PL_Key_6, ['7'] = PL_Key_7,
	['8'] = PL_Key_8, ['9'] = PL_Key_9,

	[VK_NUMPAD0] = PL_Key_0, [VK_NUMPAD1] = PL_Key_1,
	[VK_NUMPAD2] = PL_Key_2, [VK_NUMPAD3] = PL_Key_3,
	[VK_NUMPAD4] = PL_Key_4, [VK_NUMPAD5] = PL_Key_5,
	[VK_NUMPAD6] = PL_Key_6, [VK_NUMPAD7] = PL_Key_7,
	[VK_NUMPAD8] = PL_Key_8, [VK_NUMPAD9] = PL_Key_9,

	[VK_F1]  = PL_Key_F1,  [VK_F2]  = PL_Key_F2,
	[VK_F3]  = PL_Key_F3,  [VK_F4]  = PL_Key_F4,
	[VK_F5]  = PL_Key_F5,  [VK_F6]  = PL_Key_F6,
	[VK_F7]  = PL_Key_F7,  [VK_F8]  = PL_Key_F8,
	[VK_F9]  = PL_Key_F9,  [VK_F10] = PL_Key_F10,
	[VK_F11] = PL_Key_F11, [VK_F12] = PL_Key_F12,

	[VK_SNAPSHOT] = PL_Key_PrintScreen,  [VK_INSERT]   = PL_Key_Insert,
	[VK_HOME]     = PL_Key_Home,         [VK_PRIOR]    = PL_Key_PageUp,
	[VK_NEXT]     = PL_Key_PageDown,     [VK_DELETE]   = PL_Key_Delete,
	[VK_END]      = PL_Key_End,          [VK_RIGHT]    = PL_Key_Right,
	[VK_LEFT]     = PL_Key_Left,         [VK_DOWN]     = PL_Key_Down,
	[VK_UP]       = PL_Key_Up,           [VK_DIVIDE]   = PL_Key_Divide,
	[VK_MULTIPLY] = PL_Key_Multiply,     [VK_ADD]      = PL_Key_Plus,
	[VK_SUBTRACT] = PL_Key_Minus,        [VK_DECIMAL]  = PL_Key_Period,
	[VK_OEM_3]    = PL_Key_BackTick,     [VK_CONTROL]  = PL_Key_Ctrl,
	[VK_RETURN]   = PL_Key_Return,       [VK_ESCAPE]   = PL_Key_Escape,
	[VK_BACK]     = PL_Key_Backspace,    [VK_TAB]      = PL_Key_Tab,
	[VK_SPACE]    = PL_Key_Space,        [VK_SHIFT]    = PL_Key_Shift,
};

static void PL_NormalizeCursorPosition(HWND hwnd, int x, int y, i32 *nx, i32 *ny) {
	RECT rc;
	GetClientRect(hwnd, &rc);
	int window_h = rc.bottom - rc.top;
	*nx          = x;
	*ny          = window_h - y;
}

static void PL_ActivateAudioDevice(PL_AudioDeviceNative *native) {
	PL_AudioDeviceList *devices = native->IsCapture == 0 ? &Context.IoDevice.AudioRenderDevices : &Context.IoDevice.AudioCaptureDevices;

	if (devices->Count >= PL_MAX_ACTIVE_AUDIO_DEVICE) {
		LogWarning("Max audio %s device quota reached. Ignoring newly added device\n", native->IsCapture ? "capture" : "render");
		return;
	}

	imem pos = 0;
	int  res = -1;
	for (; pos < devices->Count; ++pos) {
		res = strcmp(devices->Data[pos].Name, native->Name);
		if (res >= 0) {
			break;
		}
	}
	if (res == 0) return;

	for (imem i = devices->Count; i > pos; i -= 1) {
		devices->Data[i] = devices->Data[i - 1];
		PL_AudioDeviceNative *handle = devices->Data[i].Handle;
	}
	devices->Data[pos] = (PL_AudioDevice){ .Handle = native, .Name = native->Name };
	devices->Count += 1;
}

static void PL_DeactivateAudioDevice(PL_AudioDeviceNative *native) {
	PL_AudioDeviceList *devices = native->IsCapture == 0 ? &Context.IoDevice.AudioRenderDevices : &Context.IoDevice.AudioCaptureDevices;

	if (devices->Count == 0) return;

	imem pos = 0;
	int  res = -1;
	for (; pos < devices->Count; ++pos) {
		res = strcmp(devices->Data[pos].Name, native->Name);
		if (res >= 0) {
			break;
		}
	}
	if (res == 0) {
		for (imem i = pos; i < devices->Count - 1; i += 1) {
			devices->Data[i] = devices->Data[i + 1];
			PL_AudioDeviceNative *handle = devices->Data[i].Handle;
		}
		devices->Count -= 1;
	}
}

static void PL_UnsetCurrentAudioDevice(PL_AudioEndpointKind endpoint) {
	PL_AudioDeviceList *devices = endpoint == PL_AudioEndpoint_Render ? &Context.IoDevice.AudioRenderDevices : &Context.IoDevice.AudioCaptureDevices;
	devices->Default = false;
	devices->Current = 0;
}

static void PL_SetCurrentAudioDevice(PL_AudioDeviceNative *native) {
	PL_AudioDeviceList *devices = native->IsCapture == 0 ? &Context.IoDevice.AudioRenderDevices : &Context.IoDevice.AudioCaptureDevices;

	for (int i = 0; i < devices->Count; ++i) {
		if (devices->Data[i].Handle == native) {
			devices->Current = &devices->Data[i];
			break;
		}
	}

	PL_AudioEndpointKind endpoint = native->IsCapture ? PL_AudioEndpoint_Capture : PL_AudioEndpoint_Render;
	devices->Default = PL_AtomicCmpExgPtr(&Context.Audio.Endpoints[endpoint].DesiredDevice, nullptr, nullptr) == nullptr;
}

static LRESULT CALLBACK PL_HandleWindowsEvent(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	LRESULT res = 0;
	PL_Event event;

	switch (msg) {
		case WM_ACTIVATE:
		{
			int low    = LOWORD(wparam);
			event.Kind = (low == WA_ACTIVE || low == WA_CLICKACTIVE) ?
				PL_Event_WindowActivated : PL_Event_WindowDeactivated;
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;

		case WM_CREATE:
		{
			res = DefWindowProcW(wnd, msg, wparam, lparam);
			event.Kind = PL_Event_WindowCreated;
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case WM_DESTROY:
		{
			event.Kind = PL_Event_WindowDestroyed;
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			res = DefWindowProcW(wnd, msg, wparam, lparam);
			PostQuitMessage(0);
		} break;

		case WM_SIZE:
		{
			int x               = LOWORD(lparam);
			int y               = HIWORD(lparam);
			event.Kind          = PL_Event_WindowResized;
			event.Window.Width  = x;
			event.Window.Height = y;
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case WM_MOUSEMOVE: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			CURSORINFO info;
			info.cbSize = sizeof(info);
			if (GetCursorInfo(&info) && (info.flags == 0)) {
				// Cursor is hidden, place cursor always at center
				RECT rc;
				GetClientRect(wnd, &rc);
				POINT pt  = { rc.left, rc.top };
				POINT pt2 = { rc.right, rc.bottom };
				ClientToScreen(wnd, &pt);
				ClientToScreen(wnd, &pt2);
				SetRect(&rc, pt.x, pt.y, pt2.x, pt2.y);
				ClipCursor(&rc);
				int c_x = rc.left + (rc.right - rc.left) / 2;
				int c_y = rc.top + (rc.bottom - rc.top) / 2;
				SetCursorPos(c_x, c_y);

				Context.IoDevice.Mouse.Cursor.X = (float)c_x;
				Context.IoDevice.Mouse.Cursor.Y = (float)c_y;

				event.Kind        = PL_Event_MouseMoved;
				event.Cursor.PosX = c_x;
				event.Cursor.PosY = (rc.bottom - rc.top) - c_y;
				Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			} else {
				Context.IoDevice.Mouse.Cursor.X = (float)x;
				Context.IoDevice.Mouse.Cursor.Y = (float)y;

				event.Kind = PL_Event_MouseMoved;
				PL_NormalizeCursorPosition(wnd, x, y, &event.Cursor.PosX, &event.Cursor.PosY);
				Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			}
		} break;

		case WM_INPUT: {
			if (GET_RAWINPUT_CODE_WPARAM(wparam) == RIM_INPUT) {
				RAWINPUT input;
				UINT     size = sizeof(input);
				if (GetRawInputData((HRAWINPUT)lparam, RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER))) {
					if (input.header.dwType == RIM_TYPEMOUSE) {
						MONITORINFO monitor_info;
						monitor_info.cbSize = sizeof(monitor_info);
						if (GetMonitorInfoW(MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST), &monitor_info)) {
							LONG monitor_w = monitor_info.rcMonitor.right - monitor_info.rcMonitor.left;
							LONG monitor_h = monitor_info.rcMonitor.bottom - monitor_info.rcMonitor.top;
							LONG xrel = input.data.mouse.lLastX;
							LONG yrel = input.data.mouse.lLastY;
							Context.IoDevice.Mouse.DeltaCursor.X = (float)xrel / (float)monitor_w;
							Context.IoDevice.Mouse.DeltaCursor.Y = (float)yrel / (float)monitor_h;
						}
					}
				}
			}
			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;

		case WM_LBUTTONUP:
		case WM_LBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			PL_MouseButtonState *button = &Context.IoDevice.Mouse.Buttons[PL_Button_Left];
			if (msg == WM_LBUTTONDOWN) {
				event.Kind       = PL_Event_ButtonPressed;
				button->Down   = 1;
				button->Pressed  = 1;
			} else {
				event.Kind       = PL_Event_ButtonReleased;
				button->Down   = 0;
				button->Released = 1;
				button->Transitions += 1;
			}

			event.Button.Sym = PL_Button_Left;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Button.CursorX, &event.Button.CursorY);
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case WM_RBUTTONUP:
		case WM_RBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			PL_MouseButtonState *button = &Context.IoDevice.Mouse.Buttons[PL_Button_Right];
			if (msg == WM_RBUTTONDOWN) {
				event.Kind       = PL_Event_ButtonPressed;
				button->Down   = 1;
				button->Pressed  = 1;
			} else {
				event.Kind       = PL_Event_ButtonReleased;
				button->Down   = 0;
				button->Released = 1;
				button->Transitions += 1;
			}
			
			event.Button.Sym = PL_Button_Right;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Button.CursorX, &event.Button.CursorY);
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case WM_MBUTTONUP:
		case WM_MBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			PL_MouseButtonState *button = &Context.IoDevice.Mouse.Buttons[PL_Button_Middle];
			if (msg == WM_MBUTTONDOWN) {
				event.Kind       = PL_Event_ButtonPressed;
				button->Down   = 1;
				button->Pressed  = 1;
			} else {
				event.Kind       = PL_Event_ButtonReleased;
				button->Down   = 0;
				button->Released = 1;
				button->Transitions += 1;
			}

			event.Button.Sym = PL_Button_Middle;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Button.CursorX, &event.Button.CursorY);
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case WM_LBUTTONDBLCLK: {
			Context.IoDevice.Mouse.Buttons[PL_Button_Left].DoubleClicked = 1;
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			event.Kind = PL_Event_DoubleClicked;
			event.Button.Sym = PL_Button_Left;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Button.CursorX, &event.Button.CursorY);
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case WM_RBUTTONDBLCLK: {
			Context.IoDevice.Mouse.Buttons[PL_Button_Right].DoubleClicked = 1;
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			event.Kind = PL_Event_DoubleClicked;
			event.Button.Sym = PL_Button_Right;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Button.CursorX, &event.Button.CursorY);
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case WM_MBUTTONDBLCLK: {
			Context.IoDevice.Mouse.Buttons[PL_Button_Middle].DoubleClicked = 1;
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			event.Kind = PL_Event_DoubleClicked;
			event.Button.Sym = PL_Button_Middle;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Button.CursorX, &event.Button.CursorY);
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case WM_MOUSEWHEEL: {
			Context.IoDevice.Mouse.Wheel.Y = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			event.Kind = PL_Event_WheelMoved;
			event.Wheel.Vert = Context.IoDevice.Mouse.Wheel.Y;
			event.Wheel.Horz = 0.0f;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Wheel.CursorX, &event.Wheel.CursorY);
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case WM_MOUSEHWHEEL: {
			Context.IoDevice.Mouse.Wheel.X = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			event.Kind = PL_Event_WheelMoved;
			event.Wheel.Vert = 0.0f;
			event.Wheel.Horz = Context.IoDevice.Mouse.Wheel.X;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Wheel.CursorX, &event.Wheel.CursorY);
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case WM_KEYUP:
		case WM_KEYDOWN: {
			if (wparam < ArrayCount(VirtualKeyMap)) {
				PL_Key sym       = VirtualKeyMap[wparam];
				PL_KeyState *key = &Context.IoDevice.Keyboard.Keys[sym];

				bool repeat = HIWORD(lparam) & KF_REPEAT;

				if (msg == WM_KEYDOWN) {
					key->Down = 1;
					if (!repeat)
						key->Pressed = 1;
					event.Kind = PL_Event_KeyPressed;
				} else {
					key->Down   = 0;
					key->Released = 1;
					key->Transitions += 1;
					event.Kind = PL_Event_KeyReleased;
				}

				event.Key.Sym    = sym;
				event.Key.Repeat = repeat;
				Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);

				if (event.Kind == PL_Event_KeyPressed && !event.Key.Repeat) {
					if (Context.Flags & PL_Flag_ToggleFullscreenF11 && event.Key.Sym == PL_Key_F11) {
						PL_ToggleFullscreen();
					} else if (Context.Flags & PL_Flag_ExitEscape && event.Key.Sym == PL_Key_Escape) {
						ExitProcess(0);
					}
				}
			}
		} break;

		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN: {
			if (wparam == VK_F10) {
				PL_KeyState *key = &Context.IoDevice.Keyboard.Keys[PL_Key_F10];

				bool repeat = HIWORD(lparam) & KF_REPEAT;

				if (msg == WM_SYSKEYDOWN) {
					key->Down = 1;
					if (!repeat)
						key->Pressed = 1;
					event.Kind = PL_Event_KeyPressed;
				} else {
					key->Down   = 0;
					key->Released = 1;
					key->Transitions += 1;
					event.Kind = PL_Event_KeyReleased;
				}

				event.Key.Sym    = PL_Key_F10;
				event.Key.Repeat = repeat;
				Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			}

			if ((Context.Flags & PL_Flag_ExitAltF4) && 
				msg == WM_SYSKEYDOWN && wparam == VK_F4 &&
				!(HIWORD(lparam) & KF_REPEAT)) {
				ExitProcess(0);
			}
		} break;

		case WM_CHAR: {
			u32 value = (u32)wparam;

			if (IS_HIGH_SURROGATE(value)) {
				Context.Window.LastText = value;
			} else if (IS_LOW_SURROGATE(value)) {
				u32 high                = Context.Window.LastText;
				u32 low                 = value;
				Context.Window.LastText = 0;
				u32 codepoint           = 
				event.Kind              = PL_Event_TextInput;
				event.Text.Codepoint    = (((high - 0xd800) << 10) + (low - 0xdc00) + 0x10000);
				Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			} else {
				Context.Window.LastText = 0;
				event.Kind              = PL_Event_TextInput;
				event.Text.Codepoint    = value;
				Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			}
		} break;

		case WM_DPICHANGED: {
			RECT *rect   = (RECT *)lparam;
			LONG  left   = rect->left;
			LONG  top    = rect->top;
			LONG  width  = rect->right - rect->left;
			LONG  height = rect->bottom - rect->top;
			SetWindowPos(wnd, 0, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
		} break;

		case PL_WM_AUDIO_DEVICE_ACTIVATED: {
			PL_AudioDeviceNative *native  = (PL_AudioDeviceNative *)wparam;
			PL_AudioEndpointKind endpoint = native->IsCapture ? PL_AudioEndpoint_Capture : PL_AudioEndpoint_Render;
			event.Kind                    = PL_Event_AudioDeviceActived;
			event.Audio.Endpoint          = endpoint;
			event.Audio.NativeHandle      = native;
			event.Audio.Name              = native->Name;
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			PL_ActivateAudioDevice(native);
		} break;

		case PL_WM_AUDIO_DEVICE_DEACTIVATED: {
			PL_AudioDeviceNative *native  = (PL_AudioDeviceNative *)wparam;
			PL_AudioEndpointKind endpoint = native->IsCapture ? PL_AudioEndpoint_Capture : PL_AudioEndpoint_Render;
			event.Kind                    = PL_Event_AudioDeviceDeactived;
			event.Audio.Endpoint          = endpoint;
			event.Audio.NativeHandle      = native;
			event.Audio.Name              = native->Name;
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			PL_DeactivateAudioDevice(native);
		} break;

		case PL_WM_AUDIO_PAUSED: {
			event.Kind               = PL_Event_AudioPaused;
			event.Audio.Endpoint     = (PL_AudioEndpointKind)wparam;
			event.Audio.NativeHandle = 0;
			event.Audio.Name         = nullptr;
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case PL_WM_AUDIO_RESUMED: {
			event.Kind               = PL_Event_AudioResumed;
			event.Audio.Endpoint     = (PL_AudioEndpointKind)wparam;
			event.Audio.NativeHandle = 0;
			event.Audio.Name         = nullptr;
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
		} break;

		case PL_WM_AUDIO_DEVICE_LOST: {
			event.Kind               = PL_Event_AudioDeviceLost;
			event.Audio.Endpoint     = (PL_AudioEndpointKind)wparam;
			event.Audio.NativeHandle = 0;
			event.Audio.Name         = nullptr;
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			PL_UnsetCurrentAudioDevice(event.Audio.Endpoint);
		} break;

		case PL_WM_AUDIO_DEVICE_CHANGED: {
			PL_AudioDeviceNative *native  = (PL_AudioDeviceNative *)wparam;
			PL_AudioEndpointKind endpoint = native->IsCapture ? PL_AudioEndpoint_Capture : PL_AudioEndpoint_Render;
			event.Kind                    = PL_Event_AudioDeviceChanged;
			event.Audio.Endpoint          = endpoint;
			event.Audio.NativeHandle      = native;
			event.Audio.Name              = native->Name;
			Context.UserVTable.OnEvent(&event, Context.UserVTable.Data);
			PL_SetCurrentAudioDevice(native);
		} break;

		default: {
			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;
	}

	return res;
}

static bool PL_Media_Window_IsFullscreen(void) {
	DWORD dwStyle = GetWindowLongW(Context.Window.Handle, GWL_STYLE);
	if (dwStyle & WS_OVERLAPPEDWINDOW) {
		return false;
	}
	return true;
}

static void PL_Media_Window_ToggleFullscreen(void) {
	HWND hwnd           = Context.Window.Handle;
	DWORD dwStyle       = GetWindowLongW(hwnd, GWL_STYLE);
	if (dwStyle & WS_OVERLAPPEDWINDOW) {
		MONITORINFO mi  = { sizeof(mi) };
		if (GetWindowPlacement(hwnd, &Context.Window.Placement) &&
			GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
			SetWindowLongW(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	} else {
		SetWindowLongW(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(hwnd, &Context.Window.Placement);
		SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

static void PL_Media_Window_InitVTable(void) {
	HMODULE instance = GetModuleHandleW(nullptr);

	WNDCLASSEXW wnd_class = {
		.cbSize        = sizeof(wnd_class),
		.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
		.lpfnWndProc   = PL_HandleWindowsEvent,
		.hInstance     = instance,
		.hIcon         = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(102), IMAGE_ICON, 0, 0, LR_SHARED),
		.lpszClassName = L"PL Window"
	};

	RegisterClassExW(&wnd_class);

	wchar_t title[4000] = L"KrWindow | Windows";

	if (Context.Config.Window.Title[0]) {
		MultiByteToWideChar(CP_UTF8, 0, Context.Config.Window.Title, -1, title, ArrayCount(title));
	}

	int width = CW_USEDEFAULT, height = CW_USEDEFAULT;

	if (Context.Config.Window.Width)
		width = Context.Config.Window.Width;
	if (Context.Config.Window.Height)
		height = Context.Config.Window.Height;

	Context.Window.Handle = CreateWindowExW(WS_EX_APPWINDOW, wnd_class.lpszClassName, title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, instance, nullptr);
	if (!Context.Window.Handle) {
		LogError("Failed to create window. Reason: %s\n", PL_GetLastError());
		return;
	}

	RAWINPUTDEVICE device;
	device.usUsagePage = 0x1;
	device.usUsage     = 0x2;
	device.dwFlags     = 0;
	device.hwndTarget  = Context.Window.Handle;

	RegisterRawInputDevices(&device, 1, sizeof(device));

	GetWindowPlacement(Context.Window.Handle, &Context.Window.Placement);

	ShowWindow(Context.Window.Handle, SW_SHOWNORMAL);
	UpdateWindow(Context.Window.Handle);

	Context.VTable.IsFullscreen     = PL_Media_Window_IsFullscreen;
	Context.VTable.ToggleFullscreen = PL_Media_Window_ToggleFullscreen;
	Context.Features |= PL_Feature_Window;
}

//
// Audio
//

DEFINE_GUID(PL_CLSID_MMDeviceEnumerator,0xbcde0395,0xe52f,0x467c,0x8e,0x3d,0xc4,0x57,0x92,0x91,0x69,0x2e);
DEFINE_GUID(PL_IID_IUnknown,0x00000000,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(PL_IID_IMMDeviceEnumerator,0xa95664d2,0x9614,0x4f35,0xa7,0x46,0xde,0x8d,0xb6,0x36,0x17,0xe6);
DEFINE_GUID(PL_IID_IMMNotificationClient,0x7991eec9,0x7e89,0x4d85,0x83,0x90,0x6c,0x70,0x3c,0xec,0x60,0xc0);
DEFINE_GUID(PL_IID_IMMEndpoint,0x1BE09788,0x6894,0x4089,0x85,0x86,0x9A,0x2A,0x6C,0x26,0x5A,0xC5);
DEFINE_GUID(PL_IID_IAudioClient,0x1cb9ad4c,0xdbfa,0x4c32,0xb1,0x78,0xc2,0xf5,0x68,0xa7,0x03,0xb2);
DEFINE_GUID(PL_IID_IAudioRenderClient,0xf294acfc,0x3146,0x4483,0xa7,0xbf,0xad,0xdc,0xa7,0xc2,0x60,0xe2);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_PCM,0x00000001,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,0x00000003,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX,0x00000000L,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);

HRESULT STDMETHODCALLTYPE PL_QueryInterface(IMMNotificationClient *, REFIID, void **);
ULONG   STDMETHODCALLTYPE PL_AddRef(IMMNotificationClient *);
ULONG   STDMETHODCALLTYPE PL_Release(IMMNotificationClient *);
HRESULT STDMETHODCALLTYPE PL_OnDeviceStateChanged(IMMNotificationClient *, LPCWSTR, DWORD);
HRESULT STDMETHODCALLTYPE PL_OnDeviceAdded(IMMNotificationClient *, LPCWSTR);
HRESULT STDMETHODCALLTYPE PL_OnDeviceRemoved(IMMNotificationClient *, LPCWSTR);
HRESULT STDMETHODCALLTYPE PL_OnDefaultDeviceChanged(IMMNotificationClient *, EDataFlow, ERole, LPCWSTR);
HRESULT STDMETHODCALLTYPE PL_OnPropertyValueChanged(IMMNotificationClient *, LPCWSTR, const PROPERTYKEY);

HRESULT STDMETHODCALLTYPE PL_QueryInterface(IMMNotificationClient *This, REFIID riid, void **ppv_object) {
	if (memcmp(&PL_IID_IUnknown, riid, sizeof(GUID)) == 0) {
		*ppv_object = (IUnknown *)This;
	} else if (memcmp(&PL_IID_IMMNotificationClient, riid, sizeof(GUID)) == 0) {
		*ppv_object = (IMMNotificationClient *)This;
	} else {
		*ppv_object = NULL;
		return E_NOINTERFACE;
	}
	return S_OK;
}

ULONG STDMETHODCALLTYPE PL_AddRef(IMMNotificationClient *This)  { return 1; }
ULONG STDMETHODCALLTYPE PL_Release(IMMNotificationClient *This) { return 1; }

static PL_AudioDeviceNative *PL_FindAudioDeviceNative(LPCWSTR id) {
	PL_AudioDeviceNative *device = Context.Audio.FirstDevice.Next;
	for (; device; device = device->Next) {
		if (wcscmp(device->Id, id) == 0)
			return device;
	}
	return nullptr;
}

static PL_AudioDeviceNative *PL_AddAudioDeviceNative(IMMDevice *immdevice) {
	HRESULT hr                   = S_OK;
	DWORD state                  = 0;
	LPWSTR id                    = nullptr;
	IMMEndpoint *endpoint        = nullptr;
	PL_AudioDeviceNative *native = nullptr;

	hr = immdevice->lpVtbl->GetState(immdevice, &state);
	if (FAILED(hr)) goto failed;

	hr = immdevice->lpVtbl->GetId(immdevice, &id);
	if (FAILED(hr)) goto failed;

	hr = immdevice->lpVtbl->QueryInterface(immdevice, &PL_IID_IMMEndpoint, &endpoint);
	if (FAILED(hr)) goto failed;

	EDataFlow flow = eRender;
	hr = endpoint->lpVtbl->GetDataFlow(endpoint, &flow);
	if (FAILED(hr)) goto failed;
	endpoint->lpVtbl->Release(endpoint);

	native = CoTaskMemAlloc(sizeof(PL_AudioDeviceNative));
	if (FAILED(hr)) goto failed;

	memset(native, 0, sizeof(*native));

	native->Next      = Context.Audio.FirstDevice.Next;
	native->Age       = 1;
	native->IsActive  = (state == DEVICE_STATE_ACTIVE);
	native->IsCapture = (flow == eCapture);
	native->Id        = id;

	IPropertyStore *prop = nullptr;
	hr = immdevice->lpVtbl->OpenPropertyStore(immdevice, STGM_READ, &prop);
	if (SUCCEEDED(hr)) {
		PROPVARIANT pv;
		hr = prop->lpVtbl->GetValue(prop, &PKEY_Device_FriendlyName, &pv);
		if (SUCCEEDED(hr)) {
			if (pv.vt == VT_LPWSTR) {
				native->Name = native->NameBuff0;
				PL_UTF16ToUTF8(native->NameBuff0, sizeof(native->NameBuff0), pv.pwszVal);
			}
		}
		prop->lpVtbl->Release(prop);
	}

	Context.Audio.FirstDevice.Next = native;

	return native;

failed:

	if (endpoint) {
		endpoint->lpVtbl->Release(endpoint);
	}

	if (id) {
		CoTaskMemFree(id);
	}

	return nullptr;
}

static PL_AudioDeviceNative *PL_AddAudioDeviceNativeFromId(LPCWSTR id) {
	IMMDevice *immdevice = nullptr;
	HRESULT hr = Context.Audio.DeviceEnumerator->lpVtbl->GetDevice(Context.Audio.DeviceEnumerator, id, &immdevice);
	if (FAILED(hr)) return nullptr;
	PL_AudioDeviceNative *native = PL_AddAudioDeviceNative(immdevice);
	immdevice->lpVtbl->Release(immdevice);
	return native;
}

HRESULT STDMETHODCALLTYPE PL_OnDeviceStateChanged(IMMNotificationClient *this_, LPCWSTR device_id, DWORD new_state) {
	PL_AtomicLock(&Context.Audio.Lock);

	PL_AudioDeviceNative *native = PL_FindAudioDeviceNative(device_id);

	u32 active = (new_state == DEVICE_STATE_ACTIVE);
	u32 prev   = 0;

	if (native) {
		bool changed = (native->IsActive != active);
		InterlockedExchange(&native->IsActive, active);
		InterlockedIncrement(&native->Age);

		if (changed) {
			UINT msg = active ? PL_WM_AUDIO_DEVICE_ACTIVATED : PL_WM_AUDIO_DEVICE_DEACTIVATED;
			PostMessageW(Context.Window.Handle, msg, (WPARAM)native, 0);
		}
	} else {
		native = PL_AddAudioDeviceNativeFromId(device_id);
		UINT msg = active ? PL_WM_AUDIO_DEVICE_ACTIVATED : PL_WM_AUDIO_DEVICE_DEACTIVATED;
		PostMessageW(Context.Window.Handle, msg, (WPARAM)native, 0);
	}

	PL_AtomicUnlock(&Context.Audio.Lock);

	for (int i = 0; i < PL_AudioEndpoint_EnumCount; ++i) {
		PL_AudioDeviceNative *desired   = PL_AtomicCmpExgPtr(&Context.Audio.Endpoints[i].DesiredDevice, 0, 0);
		PL_AudioDeviceNative *effective = PL_AtomicCmpExgPtr(&Context.Audio.Endpoints[i].EffectiveDevice, 0, 0);
		if (desired == nullptr) continue; // Handled by Default's notification
		if (native->IsActive) {
			if (native == desired) {
				ReleaseSemaphore(Context.Audio.Endpoints[i].Commands[PL_AudioCommand_LoadDesired], 1, 0);
			} else if (!effective) {
				ReleaseSemaphore(Context.Audio.Endpoints[i].Commands[PL_AudioCommand_LoadDefault], 1, 0);
			}
		} else if (native == effective) {
			ReleaseSemaphore(Context.Audio.Endpoints[i].Commands[PL_AudioCommand_LoadDefault], 1, 0);
		}
	}

	return S_OK;
}

// These events is handled in "OnDeviceStateChanged"
HRESULT STDMETHODCALLTYPE PL_OnDeviceAdded(IMMNotificationClient *this_, LPCWSTR device_id)   { return S_OK; }
HRESULT STDMETHODCALLTYPE PL_OnDeviceRemoved(IMMNotificationClient *this_, LPCWSTR device_id) { return S_OK; }

HRESULT STDMETHODCALLTYPE PL_OnDefaultDeviceChanged(IMMNotificationClient *this_, EDataFlow flow, ERole role, LPCWSTR device_id) {
	if (role != eMultimedia) return S_OK;

	if (flow == eRender) {
		if (!device_id) {
			// No rendering device
			PostMessageW(Context.Window.Handle, PL_WM_AUDIO_DEVICE_LOST, PL_AudioEndpoint_Render, 0);
			return S_OK;
		}
		PL_AudioDevice *desired = PL_AtomicCmpExgPtr(&Context.Audio.Endpoints[PL_AudioEndpoint_Render].DesiredDevice, nullptr, nullptr);
		if (desired == nullptr) {
			ReleaseSemaphore(Context.Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_LoadDefault], 1, 0);
		}
	} else if (flow == eCapture) {
		if (!device_id) {
			// No capturing device
			PostMessageW(Context.Window.Handle, PL_WM_AUDIO_DEVICE_LOST, PL_AudioEndpoint_Capture, 0);
			return S_OK;
		}
		PL_AudioDevice *desired = PL_AtomicCmpExgPtr(&Context.Audio.Endpoints[PL_AudioEndpoint_Capture].DesiredDevice, nullptr, nullptr);
		if (desired == nullptr) {
			ReleaseSemaphore(Context.Audio.Endpoints[PL_AudioEndpoint_Capture].Commands[PL_AudioCommand_LoadDefault], 1, 0);
		}
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE PL_OnPropertyValueChanged(IMMNotificationClient *this_, LPCWSTR device_id, const PROPERTYKEY key) {
	if (memcmp(&key, &PKEY_Device_FriendlyName, sizeof(PROPERTYKEY)) == 0) {
		PL_AtomicLock(&Context.Audio.Lock);

		IMMDevice *immdevice = nullptr;
		HRESULT hr = Context.Audio.DeviceEnumerator->lpVtbl->GetDevice(Context.Audio.DeviceEnumerator, device_id, &immdevice);
		if (FAILED(hr)) {
			PL_AtomicUnlock(&Context.Audio.Lock);
			return S_OK;
		}

		PL_AudioDeviceNative *native = PL_FindAudioDeviceNative(device_id);
		if (!native) {
			PL_AddAudioDeviceNativeFromId(device_id);
			PL_AtomicUnlock(&Context.Audio.Lock);
			return S_OK;
		}

		IPropertyStore *prop = nullptr;
		hr = immdevice->lpVtbl->OpenPropertyStore(immdevice, STGM_READ, &prop);
		if (SUCCEEDED(hr)) {
			PROPVARIANT pv;
			hr = prop->lpVtbl->GetValue(prop, &PKEY_Device_FriendlyName, &pv);
			if (SUCCEEDED(hr)) {
				if (pv.vt == VT_LPWSTR) {
					native->Name = native->Name == native->NameBuff0 ? native->NameBuff1 : native->NameBuff0;
					PL_UTF16ToUTF8(native->Name, sizeof(native->NameBuff0), pv.pwszVal);
				}
			}
			prop->lpVtbl->Release(prop);
		}

		PL_AtomicUnlock(&Context.Audio.Lock);
	}
	return S_OK;
}

static IMMNotificationClientVtbl NotificationClientVTable = {
	.QueryInterface         = PL_QueryInterface,
	.AddRef                 = PL_AddRef,
	.Release                = PL_Release,
	.OnDeviceStateChanged   = PL_OnDeviceStateChanged,
	.OnDeviceAdded          = PL_OnDeviceAdded,
	.OnDeviceRemoved        = PL_OnDeviceRemoved,
	.OnDefaultDeviceChanged = PL_OnDefaultDeviceChanged,
	.OnPropertyValueChanged = PL_OnPropertyValueChanged,
};

static IMMNotificationClient NotificationClient = {
	.lpVtbl = &NotificationClientVTable
};

static void PL_Media_EnumerateAudioDevices(void) {
	HRESULT hr = S_OK;

	IMMDeviceCollection *device_col = nullptr;
	hr = Context.Audio.DeviceEnumerator->lpVtbl->EnumAudioEndpoints(Context.Audio.DeviceEnumerator, eAll, DEVICE_STATEMASK_ALL, &device_col);
	if (FAILED(hr)) return;

	UINT count;
	hr = device_col->lpVtbl->GetCount(device_col, &count);
	if (FAILED(hr)) { device_col->lpVtbl->Release(device_col); return; }

	PL_AtomicLock(&Context.Audio.Lock);

	LPWSTR id = nullptr;
	IMMDevice *immdevice = nullptr;
	for (UINT index = 0; index < count; ++index) {
		hr = device_col->lpVtbl->Item(device_col, index, &immdevice);
		if (FAILED(hr)) continue;

		hr = immdevice->lpVtbl->GetId(immdevice, &id);
		if (SUCCEEDED(hr)) {
			PL_AudioDeviceNative *native = PL_AddAudioDeviceNative(immdevice);
			if (native->IsActive) {
				PostMessageW(Context.Window.Handle, PL_WM_AUDIO_DEVICE_ACTIVATED, (WPARAM)native, 0);
			}
			CoTaskMemFree(id);
			id = nullptr;
		}

		immdevice->lpVtbl->Release(immdevice);
		immdevice = nullptr;
	}

	device_col->lpVtbl->Release(device_col);

	PL_AtomicUnlock(&Context.Audio.Lock);
}

static void PL_AudioEndpoint_Release(PL_AudioEndpoint *endpoint) {
	if (endpoint->Thread) {
		InterlockedExchange(&endpoint->DeviceLost, 1);
		ReleaseSemaphore(endpoint->Commands[PL_AudioCommand_Terminate], 1, 0);
		WaitForSingleObject(endpoint->Thread, INFINITE);
		TerminateThread(endpoint->Thread, 0);
		CloseHandle(endpoint->Thread);
	}
	if (endpoint->Render) {
		endpoint->Render->lpVtbl->Release(endpoint->Render);
	}
	if (endpoint->Capture) {
		endpoint->Capture->lpVtbl->Release(endpoint->Capture);
	}
	if (endpoint->Client) {
		endpoint->Client->lpVtbl->Release(endpoint->Client);
	}
	for (int j = 0; j < PL_AudioCommand_EnumCount; ++j) {
		if (endpoint->Commands[j])
			CloseHandle(endpoint->Commands[j]);
	}
}

static void PL_AudioEndpoint_LoadFrames(void) {
	PL_AudioEndpoint *endpoint = &Context.Audio.Endpoints[PL_AudioEndpoint_Render];

	if (endpoint->DeviceLost)
		return;

	BYTE *data     = nullptr;
	u32 frames     = 0;
	UINT32 padding = 0;

	HRESULT hr = endpoint->Client->lpVtbl->GetCurrentPadding(endpoint->Client, &padding);
	if (SUCCEEDED(hr)) {
		frames = endpoint->MaxFrames - padding;
		if (frames) {
			hr = endpoint->Render->lpVtbl->GetBuffer(endpoint->Render, frames, &data);
			if (SUCCEEDED(hr)) {
				u32 written = Context.UserVTable.OnAudioRender(&endpoint->Spec, data, frames, Context.UserVTable.Data);
				DWORD flags = written < frames ? AUDCLNT_BUFFERFLAGS_SILENT : 0;
				hr = endpoint->Render->lpVtbl->ReleaseBuffer(endpoint->Render, written, flags);
			}
		} else {
			hr = endpoint->Render->lpVtbl->ReleaseBuffer(endpoint->Render, 0, 0);
		}
	}

	if (FAILED(hr)) {
		InterlockedExchange(&endpoint->DeviceLost, 1);
	}
}

static void PL_AudioEndpoint_ReleaseRenderDevice(void) {
	PL_AudioEndpoint *endpoint = &Context.Audio.Endpoints[PL_AudioEndpoint_Render];
	if (endpoint->Render) {
		endpoint->Render->lpVtbl->Release(endpoint->Render);
		endpoint->Render = nullptr;
	}
	if (endpoint->Client) {
		endpoint->Client->lpVtbl->Release(endpoint->Client);
		endpoint->Client = nullptr;
	}
	endpoint->MaxFrames = 0;
	InterlockedExchange(&endpoint->DeviceLost, 1);
	InterlockedExchange(&endpoint->Age, 0);
	InterlockedExchangePointer(&endpoint->EffectiveDevice, nullptr);
}

static bool PL_AudioEndpoint_SetAudioSpec(PL_AudioEndpoint *endpoint, WAVEFORMATEX *wave_format) {
	const WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)wave_format;

	if (wave_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT &&
		wave_format->wBitsPerSample == 32) {
		endpoint->Spec.Format = PL_AudioFormat_R32;
	} else if (wave_format->wFormatTag == WAVE_FORMAT_PCM &&
		wave_format->wBitsPerSample == 32) {
		endpoint->Spec.Format = PL_AudioFormat_I32;
	} else if (wave_format->wFormatTag == WAVE_FORMAT_PCM &&
		wave_format->wBitsPerSample == 16) {
		endpoint->Spec.Format = PL_AudioFormat_I16;
	} else if (wave_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 32)) {
			endpoint->Spec.Format = PL_AudioFormat_R32;
		} else if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 32)) {
			endpoint->Spec.Format = PL_AudioFormat_I32;
		} else if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 16)) {
			endpoint->Spec.Format = PL_AudioFormat_I16;
		} else {
			return false;
		}
	} else {
		return false;
	}

	endpoint->Spec.Channels  = wave_format->nChannels;
	endpoint->Spec.Frequency = wave_format->nSamplesPerSec;

	return true;
}

static bool PL_AudioEndpoint_TryLoadRenderDevice(PL_AudioDeviceNative *native) {
	PL_AudioEndpoint *endpoint = &Context.Audio.Endpoints[PL_AudioEndpoint_Render];
	IMMDevice *immdevice       = nullptr;
	WAVEFORMATEX *wave_format  = nullptr;

	HRESULT hr;

	if (!native) {
		hr = Context.Audio.DeviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(Context.Audio.DeviceEnumerator, eRender, eMultimedia, &immdevice);
		if (FAILED(hr)) return false;

		LPCWSTR id = nullptr;
		hr         = immdevice->lpVtbl->GetId(immdevice, &id);
		if (SUCCEEDED(hr)) {
			native = PL_FindAudioDeviceNative(id);
			CoTaskMemFree((void *)id);
		}
		if (!native) {
			immdevice->lpVtbl->Release(immdevice);
			return false;
		}
	} else {
		hr = Context.Audio.DeviceEnumerator->lpVtbl->GetDevice(Context.Audio.DeviceEnumerator, native->Id, &immdevice);
		if (FAILED(hr)) return false;
	}

	hr = immdevice->lpVtbl->Activate(immdevice, &PL_IID_IAudioClient, CLSCTX_ALL, nullptr, &endpoint->Client);
	if (FAILED(hr)) goto failed;

	hr = endpoint->Client->lpVtbl->GetMixFormat(endpoint->Client, &wave_format);
	if (FAILED(hr)) goto failed;

	if (!PL_AudioEndpoint_SetAudioSpec(endpoint, wave_format)) {
		LogError("Window: Unsupported audio specification, device = %s\n", native->Name);
		goto failed;
	}

	REFERENCE_TIME device_period;
	hr = endpoint->Client->lpVtbl->GetDevicePeriod(endpoint->Client, nullptr, &device_period);
	if (FAILED(hr)) goto failed;

	hr = endpoint->Client->lpVtbl->Initialize(endpoint->Client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		device_period, device_period, wave_format, nullptr);
	if (FAILED(hr)) goto failed;

	hr = endpoint->Client->lpVtbl->SetEventHandle(endpoint->Client, endpoint->Commands[PL_AudioCommand_Update]);
	if (FAILED(hr)) goto failed;

	hr = endpoint->Client->lpVtbl->GetBufferSize(endpoint->Client, &endpoint->MaxFrames);
	if (FAILED(hr)) goto failed;

	hr = endpoint->Client->lpVtbl->GetService(endpoint->Client, &PL_IID_IAudioRenderClient, &endpoint->Render);
	if (FAILED(hr)) goto failed;

	CoTaskMemFree(wave_format);
	immdevice->lpVtbl->Release(immdevice);

	endpoint->Age = InterlockedCompareExchange(&native->Age, 0, 0);
	InterlockedExchangePointer(&endpoint->EffectiveDevice, native);
	InterlockedExchange(&endpoint->DeviceLost, 0);

	PostMessageW(Context.Window.Handle, PL_WM_AUDIO_DEVICE_CHANGED, (WPARAM)Context.Audio.Endpoints[PL_AudioEndpoint_Render].EffectiveDevice, 0);

	PL_AudioEndpoint_LoadFrames();

	if (Context.Audio.Endpoints[PL_AudioEndpoint_Render].Resumed) {
		IAudioClient *client = Context.Audio.Endpoints[PL_AudioEndpoint_Render].Client;
		HRESULT hr = client->lpVtbl->Start(client);
		if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED) {
			PostMessageW(Context.Window.Handle, PL_WM_AUDIO_RESUMED, PL_AudioEndpoint_Render, 0);
		} else {
			InterlockedExchange(&Context.Audio.Endpoints[PL_AudioEndpoint_Render].DeviceLost, 1);
			goto failed;
		}
	}

	return true;

failed:

	if (wave_format)
		CoTaskMemFree(wave_format);

	immdevice->lpVtbl->Release(immdevice);

	return false;
}

static void PL_AudioEndpoint_RestartRenderDevice(PL_AudioDeviceNative *native) {
	PL_AudioEndpoint_ReleaseRenderDevice();
	if (!PL_AudioEndpoint_TryLoadRenderDevice(native)) {
		PL_AudioEndpoint_ReleaseRenderDevice();
		if (native) {
			if (!PL_AudioEndpoint_TryLoadRenderDevice(nullptr)) {
				PL_AudioEndpoint_ReleaseRenderDevice();
				PostMessageW(Context.Window.Handle, PL_WM_AUDIO_DEVICE_LOST, PL_AudioEndpoint_Render, 0);
			}
		} else {
			PostMessageW(Context.Window.Handle, PL_WM_AUDIO_DEVICE_LOST, PL_AudioEndpoint_Render, 0);
		}
	}
}

static DWORD WINAPI PL_AudioRenderThread(LPVOID param) {
	HANDLE thread = GetCurrentThread();
	SetThreadDescription(thread, L"PL Audio Thread");

	PL_InitThreadContext();

	DWORD task_index;
	HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	PL_AudioEndpoint *endpoint = &Context.Audio.Endpoints[PL_AudioEndpoint_Render];

	PL_AudioEndpoint_RestartRenderDevice(endpoint->DesiredDevice);

	HRESULT hr;

	while (1) {
		DWORD wait = WaitForMultipleObjects(PL_AudioCommand_EnumCount, endpoint->Commands, FALSE, INFINITE);

		if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Update) {
			PL_AudioEndpoint_LoadFrames();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Resume) {
			InterlockedExchange(&endpoint->Resumed, 1);
			if (!endpoint->DeviceLost) {
				hr = endpoint->Client->lpVtbl->Start(endpoint->Client);
				if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED) {
					PostMessageW(Context.Window.Handle, PL_WM_AUDIO_RESUMED, PL_AudioEndpoint_Render, 0);
				} else {
					InterlockedExchange(&endpoint->DeviceLost, 1);
				}
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Pause) {
			InterlockedExchange(&endpoint->Resumed, 0);
			if (!endpoint->DeviceLost) {
				hr = endpoint->Client->lpVtbl->Stop(endpoint->Client);
				if (SUCCEEDED(hr)) {
					PostMessageW(Context.Window.Handle, PL_WM_AUDIO_PAUSED, PL_AudioEndpoint_Render, 0);
				} else {
					InterlockedExchange(&endpoint->DeviceLost, 1);
				}
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Reset) {
			LONG resumed = InterlockedExchange(&endpoint->Resumed, 0);
			if (!endpoint->DeviceLost) {
				if (resumed) {
					endpoint->Client->lpVtbl->Stop(endpoint->Client);
				}
				hr = endpoint->Client->lpVtbl->Reset(endpoint->Client);
				if (FAILED(hr)) {
					InterlockedExchange(&endpoint->DeviceLost, 1);
				}
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_LoadDefault) {
			PL_AudioEndpoint_RestartRenderDevice(nullptr);
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_LoadDesired) {
			PL_AudioDeviceNative *desired   = InterlockedCompareExchangePointer(&endpoint->DesiredDevice, nullptr, nullptr);
			PL_AudioDeviceNative *effective = InterlockedCompareExchangePointer(&endpoint->EffectiveDevice, nullptr, nullptr);

			if (desired) {
				if (effective != desired || effective->Age > endpoint->Age) {
					PL_AudioEndpoint_RestartRenderDevice(desired);
				}
			} else {
				PL_AudioEndpoint_RestartRenderDevice(nullptr);
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Terminate) {
			break;
		}
	}

	AvRevertMmThreadCharacteristics(avrt);

	return 0;
}

static bool PL_Media_Audio_IsAudioRendering(void) {
	if (InterlockedOr(&Context.Audio.Endpoints[PL_AudioEndpoint_Render].DeviceLost, 0))
		return false;
	return InterlockedOr(&Context.Audio.Endpoints[PL_AudioEndpoint_Render].Resumed, 0);
}

static void PL_Media_Audio_UpdateAudioRender(void) {
	if (!InterlockedOr(&Context.Audio.Endpoints[PL_AudioEndpoint_Render].Resumed, 0)) {
		SetEvent(Context.Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Update]);
	}
}

static void PL_Media_Audio_PauseAudioRender(void) {
	ReleaseSemaphore(Context.Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Pause], 1, 0);
}

static void PL_Media_Audio_ResumeAudioRender(void) {
	ReleaseSemaphore(Context.Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Resume], 1, 0);
}

static void PL_Media_Audio_ResetAudioRender(void) {
	ReleaseSemaphore(Context.Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Reset], 1, 0);
}

static void PL_Media_Audio_SetAudioDevice(PL_AudioDevice const *device) {
	Assert((device >= Context.IoDevice.AudioRenderDevices.Data && 
			device <= Context.IoDevice.AudioRenderDevices.Data + Context.IoDevice.AudioRenderDevices.Count) ||
			(device >= Context.IoDevice.AudioCaptureDevices.Data &&
			device <= Context.IoDevice.AudioCaptureDevices.Data + Context.IoDevice.AudioCaptureDevices.Count));

	PL_AudioDeviceNative * native   = device->Handle;
	PL_AudioEndpointKind   endpoint = native->IsCapture == 0 ? PL_AudioEndpoint_Render : PL_AudioEndpoint_Capture;

	PL_AudioDeviceNative *prev = InterlockedExchangePointer(&Context.Audio.Endpoints[endpoint].DesiredDevice, native);
	if (prev != native) {
		ReleaseSemaphore(Context.Audio.Endpoints[endpoint].Commands[PL_AudioCommand_LoadDesired], 1, 0);
	}

	if (endpoint == PL_AudioEndpoint_Render) {
		Context.IoDevice.AudioRenderDevices.Current  = (PL_AudioDevice *)device;
	} else {
		Context.IoDevice.AudioCaptureDevices.Current = (PL_AudioDevice *)device;
	}
}

static bool PL_AudioEndpoint_InitRender(void) {
	PL_AudioEndpoint *endpoint = &Context.Audio.Endpoints[PL_AudioEndpoint_Render];

	endpoint->Commands[PL_AudioCommand_Update] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!endpoint->Commands[PL_AudioCommand_Update]) {
		return false;
	}

	// Using semaphore for rest of the audio events
	for (int i = 0; i < PL_AudioCommand_EnumCount; ++i) {
		if (i == PL_AudioCommand_Update) continue;
		endpoint->Commands[i] = CreateSemaphoreW(nullptr, 0, 16, nullptr);
		if (!endpoint->Commands[i]) {
			return false;
		}
	}

	endpoint->Thread = CreateThread(nullptr, 0, PL_AudioRenderThread, nullptr, 0, 0);
	if (!endpoint->Thread) {
		return false;
	}

	Context.VTable.IsAudioRendering  = PL_Media_Audio_IsAudioRendering;
	Context.VTable.UpdateAudioRender = PL_Media_Audio_UpdateAudioRender;
	Context.VTable.PauseAudioRender  = PL_Media_Audio_PauseAudioRender;
	Context.VTable.ResumeAudioRender = PL_Media_Audio_ResumeAudioRender;
	Context.VTable.ResetAudioRender  = PL_Media_Audio_ResetAudioRender;
	Context.VTable.SetAudioDevice    = PL_Media_Audio_SetAudioDevice;

	return true;
}

static bool PL_AudioEndpoint_InitCapture(void) {
	PL_AudioEndpoint *endpoint = &Context.Audio.Endpoints[PL_AudioEndpoint_Capture];
	Unimplemented();
	return true;
}

static void PL_Media_Audio_InitVTable(void) {
	Context.IoDevice.AudioRenderDevices.Data  = Context.AudioDeviceBuffer[PL_AudioEndpoint_Render].Data;
	Context.IoDevice.AudioCaptureDevices.Data = Context.AudioDeviceBuffer[PL_AudioEndpoint_Capture].Data;

	HRESULT hr = CoCreateInstance(&PL_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &PL_IID_IMMDeviceEnumerator, &Context.Audio.DeviceEnumerator);
	if (FAILED(hr)) {
		LogWarning("Windows: Failed to create audio instance. Reason: %s\n", PL_GetLastError());
		return;
	}

	hr = Context.Audio.DeviceEnumerator->lpVtbl->RegisterEndpointNotificationCallback(Context.Audio.DeviceEnumerator, &NotificationClient);

	if (FAILED(hr)) {
		LogWarning("Windows: Failed to enumerate audio devices. Reason: %s\n", PL_GetLastError());
		Context.Audio.DeviceEnumerator->lpVtbl->Release(Context.Audio.DeviceEnumerator);
		Context.Audio.DeviceEnumerator = nullptr;
		return;
	}

	PL_Media_EnumerateAudioDevices();

	if (Context.Config.Audio.Render) {
		if (!PL_AudioEndpoint_InitRender()) {
			PL_AudioEndpoint_Release(&Context.Audio.Endpoints[PL_AudioEndpoint_Render]);
			LogWarning("Windows: Failed to initialize audio device for rendering. Reason: %s\n", PL_GetLastError());
		}
	}

	if (Context.Config.Audio.Capture) {
		if (!PL_AudioEndpoint_InitCapture()) {
			PL_AudioEndpoint_Release(&Context.Audio.Endpoints[PL_AudioEndpoint_Capture]);
			LogWarning("Windows: Failed to initialize audio device for capturing. Reason: %s\n", PL_GetLastError());
		}
	}
}

static void PL_Media_Load(void) {
	u32 features       = Context.Config.Features;
	Context.UserVTable = Context.Config.User;
	Context.Flags      = Context.Config.Flags;

	if (features & PL_Feature_Window) {
		PL_Media_Window_InitVTable();
	}

	if (features & PL_Feature_Audio) {
		PL_Media_Audio_InitVTable();
	}
}

static void PL_Media_Window_Release(void) {
	if (Context.Features & PL_Feature_Window) {
		DestroyWindow(Context.Window.Handle);
	}
}

static void PL_Media_Audio_Release(void) {
	if (Context.Features & PL_Feature_Audio) {
		for (int i = 0; i < PL_AudioEndpoint_EnumCount; ++i) {
			PL_AudioEndpoint *endpoint = &Context.Audio.Endpoints[i];
			PL_AudioEndpoint_Release(endpoint);
		}

		if (Context.Audio.DeviceEnumerator)
			Context.Audio.DeviceEnumerator->lpVtbl->Release(Context.Audio.DeviceEnumerator);

		while (Context.Audio.FirstDevice.Next != &Context.Audio.FirstDevice) {
			PL_AudioDeviceNative *native   = Context.Audio.FirstDevice.Next;
			Context.Audio.FirstDevice.Next = native->Next;
			CoTaskMemFree((void *)native->Id);
			CoTaskMemFree(native);
		}
	}
}

static void PL_Media_Release(void) {
	PL_Media_Window_Release();
	PL_Media_Audio_Release();
	memset(&Context, 0, sizeof(Context));
}

static int PL_MessageLoop(void) {
	Context.UserVTable.OnEvent(&(PL_Event){ .Kind = PL_Event_Startup }, Context.UserVTable.Data);

	MSG msg;
	while (1) {
		for (int i = 0; i < PL_Key_EnumCount; ++i) {
			PL_KeyState *key = &Context.IoDevice.Keyboard.Keys[i];
			key->Pressed     = 0;
			key->Released    = 0;
			key->Transitions = 0;
		}

		for (int i = 0; i < PL_Button_EnumCount; ++i) {
			PL_MouseButtonState *button = &Context.IoDevice.Mouse.Buttons[i];
			button->Pressed             = 0;
			button->Released            = 0;
			button->Transitions         = 0;
			button->DoubleClicked       = 0;
		}

		Context.IoDevice.Mouse.DeltaCursor = (PL_Axis){ 0, 0 };
		Context.IoDevice.Mouse.Wheel       = (PL_Axis){ 0, 0 };

		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				return 0;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		Context.UserVTable.OnUpdate(&Context.IoDevice, Context.UserVTable.Data);
	}

	Context.UserVTable.OnEvent(&(PL_Event){ .Kind = PL_Event_Quit }, Context.UserVTable.Data);

	return 0;
}

//
// Main
//

static char **PL_CommandLineArguments(int *argc) {
	int argument_count = 0;
	LPWSTR *arguments  = CommandLineToArgvW(GetCommandLineW(), &argument_count);

	if (arguments && argument_count) {
		HANDLE heap = GetProcessHeap();
		char **argv = HeapAlloc(heap, 0, sizeof(char *) * argument_count);

		if (argv) {
			int index = 0;
			for (; index < argument_count; ++index) {
				int len = WideCharToMultiByte(CP_UTF8, 0, arguments[index], -1, nullptr, 0, 0, 0);
				argv[index] = HeapAlloc(heap, 0, len + 1);
				if (!argv[index]) break;
				WideCharToMultiByte(CP_UTF8, 0, arguments[index], -1, argv[index], len + 1, 0, 0);
			}
			*argc = index;
			return argv;
		}
	}

	*argc = 0;
	return nullptr;
}

static int PL_Main(void) {
	if (IsWindows10OrGreater()) {
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	} else {
		SetProcessDPIAware();
	}

#if defined(M_BUILD_DEBUG) || defined(M_BUILD_DEVELOPER)
	AllocConsole();
#endif

	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	Assert(hr != RPC_E_CHANGED_MODE);

	if (hr != S_OK && hr != S_FALSE) {
		FatalAppExitW(0, L"Windows: Failed to initialize COM Library");
	}

	int argc    = 0;
	char **argv = PL_CommandLineArguments(&argc);

	PL_InitThreadContext();

	int res = Main(argc, argv);

	if (res == 0) {
		PL_Media_Load();
		PL_MessageLoop();
		PL_Media_Release();
	}

	CoUninitialize();

	return res;
}

#if defined(M_BUILD_DEBUG) || defined(M_BUILD_DEVELOP)
#pragma comment( linker, "/subsystem:console" )
#else
#pragma comment( linker, "/subsystem:windows" )
#endif

// SUBSYSTEM:CONSOLE
int wmain() { return PL_Main(); }

// SUBSYSTEM:WINDOWS
int __stdcall wWinMain(HINSTANCE instance, HINSTANCE prev_instance, LPWSTR cmd, int n) { return PL_Main(); }
