#include "../KrMediaInternal.h"

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
#pragma comment(lib, "Avrt.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Dwmapi.lib")

#pragma warning(pop)

#include "KrWindowsCore.h"

typedef struct PL_Window {
	HWND            Handle;
	WINDOWPLACEMENT Placement;
	u16             LastText;
} PL_Window;

typedef enum PL_AudioCommand {
	PL_AudioCommand_Update,
	PL_AudioCommand_Resume,
	PL_AudioCommand_Pause,
	PL_AudioCommand_Reset,
	PL_AudioCommand_Switch,
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
	DWORD                  ParentThreadId;
} PL_Audio;

static PL_Window             Window;
static PL_Audio              Audio;

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

static LRESULT CALLBACK PL_HandleWindowsEvent(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	LRESULT res = 0;
	PL_Event event;

	switch (msg) {
		case WM_ACTIVATE:
		{
			int low    = LOWORD(wparam);
			event.Kind = (low == WA_ACTIVE || low == WA_CLICKACTIVE) ?
				PL_Event_WindowActivated : PL_Event_WindowDeactivated;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;

		case WM_CREATE:
		{
			res = DefWindowProcW(wnd, msg, wparam, lparam);
			event.Kind = PL_Event_WindowCreated;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_DESTROY:
		{
			event.Kind = PL_Event_WindowDestroyed;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			res = DefWindowProcW(wnd, msg, wparam, lparam);
			PostQuitMessage(0); // ??
		} break;

		case WM_SIZE:
		{
			int x               = LOWORD(lparam);
			int y               = HIWORD(lparam);
			event.Kind          = PL_Event_WindowResized;
			event.Window.Width  = x;
			event.Window.Height = y;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
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

				event.Kind        = PL_Event_MouseMoved;
				event.Cursor.PosX = c_x;
				event.Cursor.PosY = (rc.bottom - rc.top) - c_y;

				Media.IoDevice.Mouse.Cursor.X = (float)event.Cursor.PosX;
				Media.IoDevice.Mouse.Cursor.Y = (float)event.Cursor.PosY;

				Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			} else {
				event.Kind = PL_Event_MouseMoved;
				PL_NormalizeCursorPosition(wnd, x, y, &event.Cursor.PosX, &event.Cursor.PosY);

				Media.IoDevice.Mouse.Cursor.X = (float)event.Cursor.PosX;
				Media.IoDevice.Mouse.Cursor.Y = (float)event.Cursor.PosY;

				Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
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
							Media.IoDevice.Mouse.DeltaCursor.X = (float)xrel / (float)monitor_w;
							Media.IoDevice.Mouse.DeltaCursor.Y = (float)yrel / (float)monitor_h;
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

			PL_MouseButtonState *button = &Media.IoDevice.Mouse.Buttons[PL_Button_Left];
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
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_RBUTTONUP:
		case WM_RBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			PL_MouseButtonState *button = &Media.IoDevice.Mouse.Buttons[PL_Button_Right];
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
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_MBUTTONUP:
		case WM_MBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			PL_MouseButtonState *button = &Media.IoDevice.Mouse.Buttons[PL_Button_Middle];
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
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_LBUTTONDBLCLK: {
			Media.IoDevice.Mouse.Buttons[PL_Button_Left].DoubleClicked = 1;
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			event.Kind = PL_Event_DoubleClicked;
			event.Button.Sym = PL_Button_Left;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Button.CursorX, &event.Button.CursorY);
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_RBUTTONDBLCLK: {
			Media.IoDevice.Mouse.Buttons[PL_Button_Right].DoubleClicked = 1;
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			event.Kind = PL_Event_DoubleClicked;
			event.Button.Sym = PL_Button_Right;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Button.CursorX, &event.Button.CursorY);
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_MBUTTONDBLCLK: {
			Media.IoDevice.Mouse.Buttons[PL_Button_Middle].DoubleClicked = 1;
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			event.Kind = PL_Event_DoubleClicked;
			event.Button.Sym = PL_Button_Middle;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Button.CursorX, &event.Button.CursorY);
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_MOUSEWHEEL: {
			Media.IoDevice.Mouse.Wheel.Y = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			event.Kind = PL_Event_WheelMoved;
			event.Wheel.Vert = Media.IoDevice.Mouse.Wheel.Y;
			event.Wheel.Horz = 0.0f;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Wheel.CursorX, &event.Wheel.CursorY);
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_MOUSEHWHEEL: {
			Media.IoDevice.Mouse.Wheel.X = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);
			event.Kind = PL_Event_WheelMoved;
			event.Wheel.Vert = 0.0f;
			event.Wheel.Horz = Media.IoDevice.Mouse.Wheel.X;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Wheel.CursorX, &event.Wheel.CursorY);
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_KEYUP:
		case WM_KEYDOWN: {
			if (wparam < ArrayCount(VirtualKeyMap)) {
				PL_Key sym       = VirtualKeyMap[wparam];
				PL_KeyState *key = &Media.IoDevice.Keyboard.Keys[sym];

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
				Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);

				if (event.Kind == PL_Event_KeyPressed && !event.Key.Repeat) {
					if (Media.Flags & PL_Flag_ToggleFullscreenF11 && event.Key.Sym == PL_Key_F11) {
						PL_ToggleFullscreen();
					} else if (Media.Flags & PL_Flag_ExitEscape && event.Key.Sym == PL_Key_Escape) {
						ExitProcess(0);
					}
				}
			}
		} break;

		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN: {
			if (wparam == VK_F10) {
				PL_KeyState *key = &Media.IoDevice.Keyboard.Keys[PL_Key_F10];

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
				Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			}

			if ((Media.Flags & PL_Flag_ExitAltF4) && 
				msg == WM_SYSKEYDOWN && wparam == VK_F4 &&
				!(HIWORD(lparam) & KF_REPEAT)) {
				ExitProcess(0);
			}
		} break;

		case WM_CHAR: {
			u32 value = (u32)wparam;

			if (IS_HIGH_SURROGATE(value)) {
				Window.LastText = value;
			} else if (IS_LOW_SURROGATE(value)) {
				u32 high                = Window.LastText;
				u32 low                 = value;
				Window.LastText = 0;
				u32 codepoint           = 
				event.Kind              = PL_Event_TextInput;
				event.Text.Codepoint    = (((high - 0xd800) << 10) + (low - 0xdc00) + 0x10000);
				Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			} else {
				Window.LastText = 0;
				event.Kind              = PL_Event_TextInput;
				event.Text.Codepoint    = value;
				Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
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

		default: {
			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;
	}

	return res;
}

static bool PL_Media_Window_IsFullscreen(void) {
	DWORD dwStyle = GetWindowLongW(Window.Handle, GWL_STYLE);
	if (dwStyle & WS_OVERLAPPEDWINDOW) {
		return false;
	}
	return true;
}

static void PL_Media_Window_ToggleFullscreen(void) {
	HWND hwnd           = Window.Handle;
	DWORD dwStyle       = GetWindowLongW(hwnd, GWL_STYLE);
	if (dwStyle & WS_OVERLAPPEDWINDOW) {
		MONITORINFO mi  = { sizeof(mi) };
		if (GetWindowPlacement(hwnd, &Window.Placement) &&
			GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
			SetWindowLongW(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	} else {
		SetWindowLongW(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(hwnd, &Window.Placement);
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

	if (Media.Config.Window.Title[0]) {
		MultiByteToWideChar(CP_UTF8, 0, Media.Config.Window.Title, -1, title, ArrayCount(title));
	}

	int width = CW_USEDEFAULT, height = CW_USEDEFAULT;

	if (Media.Config.Window.Width)
		width = Media.Config.Window.Width;
	if (Media.Config.Window.Height)
		height = Media.Config.Window.Height;

	Window.Handle = CreateWindowExW(WS_EX_APPWINDOW, wnd_class.lpszClassName, title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, instance, nullptr);
	if (!Window.Handle) {
		LogError("Failed to create window. Reason: %s\n", PL_GetLastError());
		return;
	}

	RAWINPUTDEVICE device;
	device.usUsagePage = 0x1;
	device.usUsage     = 0x2;
	device.dwFlags     = 0;
	device.hwndTarget  = Window.Handle;

	RegisterRawInputDevices(&device, 1, sizeof(device));

	GetWindowPlacement(Window.Handle, &Window.Placement);

	ShowWindow(Window.Handle, SW_SHOWNORMAL);
	UpdateWindow(Window.Handle);

	Media.VTable.IsFullscreen     = PL_Media_Window_IsFullscreen;
	Media.VTable.ToggleFullscreen = PL_Media_Window_ToggleFullscreen;
	Media.Features |= PL_Feature_Window;
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
DEFINE_GUID(PL_IID_IAudioCaptureClient,0xc8adbd64,0xe71e,0x48a0,0xa4,0xde,0x18,0x5c,0x39,0x5c,0xd3,0x17);
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
	PL_AudioDeviceNative *device = Audio.FirstDevice.Next;
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

	native->Next      = Audio.FirstDevice.Next;
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
				native->Name = native->NameBufferA;
				PL_UTF16ToUTF8(native->NameBufferA, sizeof(native->NameBufferA), pv.pwszVal);
			}
		}
		prop->lpVtbl->Release(prop);
	}

	Audio.FirstDevice.Next = native;

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
	HRESULT hr = Audio.DeviceEnumerator->lpVtbl->GetDevice(Audio.DeviceEnumerator, id, &immdevice);
	if (FAILED(hr)) return nullptr;
	PL_AudioDeviceNative *native = PL_AddAudioDeviceNative(immdevice);
	immdevice->lpVtbl->Release(immdevice);
	return native;
}

HRESULT STDMETHODCALLTYPE PL_OnDeviceStateChanged(IMMNotificationClient *this_, LPCWSTR device_id, DWORD new_state) {
	PL_AtomicLock(&Audio.Lock);

	PL_AudioDeviceNative *native = PL_FindAudioDeviceNative(device_id);

	u32 active = (new_state == DEVICE_STATE_ACTIVE);
	u32 prev   = 0;

	if (native) {
		bool changed = (native->IsActive != active);
		InterlockedExchange(&native->IsActive, active);

		if (changed) {
			UINT msg = active ? PL_WM_AUDIO_DEVICE_ACTIVATED : PL_WM_AUDIO_DEVICE_DEACTIVATED;
			PostThreadMessageW(Audio.ParentThreadId, msg, (WPARAM)native, 0);
		}
	} else {
		native = PL_AddAudioDeviceNativeFromId(device_id);
		UINT msg = active ? PL_WM_AUDIO_DEVICE_ACTIVATED : PL_WM_AUDIO_DEVICE_DEACTIVATED;
		PostThreadMessageW(Audio.ParentThreadId, msg, (WPARAM)native, 0);
	}

	PL_AtomicUnlock(&Audio.Lock);

	PL_AudioEndpoint *endpoint = &Audio.Endpoints[native->IsCapture == 0 ?
		PL_AudioEndpoint_Render : PL_AudioEndpoint_Capture];

	PL_AudioDeviceNative *desired   = PL_AtomicCmpExgPtr(&endpoint->DesiredDevice, 0, 0);
	PL_AudioDeviceNative *effective = PL_AtomicCmpExgPtr(&endpoint->EffectiveDevice, 0, 0);
	if (desired == nullptr) return S_OK; // Handled by Default's notification

	if (native->IsActive) {
		if (native == desired || !effective) {
			SetEvent(endpoint->Commands[PL_AudioCommand_Switch]);
		}
	} else if (native == effective) {
		SetEvent(endpoint->Commands[PL_AudioCommand_Switch]);
	}

	return S_OK;
}

// These events is handled in "OnDeviceStateChanged"
HRESULT STDMETHODCALLTYPE PL_OnDeviceAdded(IMMNotificationClient *this_, LPCWSTR device_id)   { return S_OK; }
HRESULT STDMETHODCALLTYPE PL_OnDeviceRemoved(IMMNotificationClient *this_, LPCWSTR device_id) { return S_OK; }

HRESULT STDMETHODCALLTYPE PL_OnDefaultDeviceChanged(IMMNotificationClient *this_, EDataFlow flow, ERole role, LPCWSTR device_id) {
	if (role != eMultimedia) return S_OK;

	PL_AudioEndpointKind kind = flow == eRender ? PL_AudioEndpoint_Render : PL_AudioEndpoint_Capture;
	PL_AudioEndpoint *   endpoint = &Audio.Endpoints[kind];

	if (!device_id) {
		// No device for the endpoint
		PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_DEVICE_LOST, kind, 0);
		return S_OK;
	}

	PL_AudioDevice *desired = PL_AtomicCmpExgPtr(&endpoint->DesiredDevice, nullptr, nullptr);
	if (desired == nullptr) {
		SetEvent(endpoint->Commands[PL_AudioCommand_Switch]);
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE PL_OnPropertyValueChanged(IMMNotificationClient *this_, LPCWSTR device_id, const PROPERTYKEY key) {
	if (memcmp(&key, &PKEY_Device_FriendlyName, sizeof(PROPERTYKEY)) == 0) {
		PL_AtomicLock(&Audio.Lock);

		IMMDevice *immdevice = nullptr;
		HRESULT hr = Audio.DeviceEnumerator->lpVtbl->GetDevice(Audio.DeviceEnumerator, device_id, &immdevice);
		if (FAILED(hr)) {
			PL_AtomicUnlock(&Audio.Lock);
			return S_OK;
		}

		PL_AudioDeviceNative *native = PL_FindAudioDeviceNative(device_id);
		if (!native) {
			PL_AddAudioDeviceNativeFromId(device_id);
			PL_AtomicUnlock(&Audio.Lock);
			return S_OK;
		}

		IPropertyStore *prop = nullptr;
		hr = immdevice->lpVtbl->OpenPropertyStore(immdevice, STGM_READ, &prop);
		if (SUCCEEDED(hr)) {
			PROPVARIANT pv;
			hr = prop->lpVtbl->GetValue(prop, &PKEY_Device_FriendlyName, &pv);
			if (SUCCEEDED(hr)) {
				if (pv.vt == VT_LPWSTR) {
					native->Name = native->Name == native->NameBufferA ? native->NameBufferB : native->NameBufferA;
					PL_UTF16ToUTF8(native->Name, sizeof(native->NameBufferA), pv.pwszVal);
				}
			}
			prop->lpVtbl->Release(prop);
		}

		PL_AtomicUnlock(&Audio.Lock);
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
	hr = Audio.DeviceEnumerator->lpVtbl->EnumAudioEndpoints(Audio.DeviceEnumerator, eAll, DEVICE_STATEMASK_ALL, &device_col);
	if (FAILED(hr)) return;

	UINT count;
	hr = device_col->lpVtbl->GetCount(device_col, &count);
	if (FAILED(hr)) { device_col->lpVtbl->Release(device_col); return; }

	PL_AtomicLock(&Audio.Lock);

	LPWSTR id = nullptr;
	IMMDevice *immdevice = nullptr;
	for (UINT index = 0; index < count; ++index) {
		hr = device_col->lpVtbl->Item(device_col, index, &immdevice);
		if (FAILED(hr)) continue;

		hr = immdevice->lpVtbl->GetId(immdevice, &id);
		if (SUCCEEDED(hr)) {
			PL_AudioDeviceNative *native = PL_AddAudioDeviceNative(immdevice);
			if (native->IsActive) {
				PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_DEVICE_ACTIVATED, (WPARAM)native, 0);
			}
			CoTaskMemFree(id);
			id = nullptr;
		}

		immdevice->lpVtbl->Release(immdevice);
		immdevice = nullptr;
	}

	device_col->lpVtbl->Release(device_col);

	PL_AtomicUnlock(&Audio.Lock);
}

static void PL_AudioEndpoint_Release(PL_AudioEndpoint *endpoint) {
	if (endpoint->Thread) {
		InterlockedExchange(&endpoint->DeviceLost, 1);
		SetEvent(endpoint->Commands[PL_AudioCommand_Terminate]);
		WaitForSingleObject(endpoint->Thread, INFINITE);
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

static void PL_AudioEndpoint_RenderFrames(void) {
	PL_AudioEndpoint *endpoint = &Audio.Endpoints[PL_AudioEndpoint_Render];

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
				u32 written = Media.UserVTable.OnAudioRender(&endpoint->Spec, data, frames, Media.UserVTable.Data);
				hr = endpoint->Render->lpVtbl->ReleaseBuffer(endpoint->Render, written, 0);
			}
		} else {
			hr = endpoint->Render->lpVtbl->ReleaseBuffer(endpoint->Render, 0, 0);
		}
	}

	if (FAILED(hr)) {
		InterlockedExchange(&endpoint->DeviceLost, 1);
	}
}

static void PL_AudioEndpoint_CaptureFrames(void) {
	PL_AudioEndpoint *endpoint = &Audio.Endpoints[PL_AudioEndpoint_Capture];

	if (endpoint->DeviceLost)
		return;

	BYTE *data     = nullptr;
	UINT32 frames  = 0;
	DWORD flags    = 0;

	HRESULT hr = endpoint->Capture->lpVtbl->GetNextPacketSize(endpoint->Capture, &frames);

	while (frames) {
		if (FAILED(hr)) {
			InterlockedExchange(&endpoint->DeviceLost, 1);
			return;
		}
		hr = endpoint->Capture->lpVtbl->GetBuffer(endpoint->Capture, &data, &frames, &flags, nullptr, nullptr);
		if (SUCCEEDED(hr)) {
			if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
				data = nullptr; // Silence
			u32 read = Media.UserVTable.OnAudioCapture(&endpoint->Spec, data, frames, Media.UserVTable.Data);
			hr = endpoint->Capture->lpVtbl->ReleaseBuffer(endpoint->Capture, read);
			if (SUCCEEDED(hr)) {
				hr = endpoint->Capture->lpVtbl->GetNextPacketSize(endpoint->Capture, &frames);
			}
		}
	}
}

static void PL_AudioEndpoint_ReleaseDevice(PL_AudioEndpointKind kind) {
	PL_AudioEndpoint *endpoint = &Audio.Endpoints[kind];
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

static bool PL_AudioEndpoint_TryLoadDevice(PL_AudioEndpointKind kind, PL_AudioDeviceNative *native) {
	PL_AudioEndpoint *endpoint = &Audio.Endpoints[kind];
	IMMDevice *immdevice       = nullptr;
	WAVEFORMATEX *wave_format  = nullptr;

	HRESULT hr;

	if (!native) {
		EDataFlow flow = kind == PL_AudioEndpoint_Render ? eRender : eCapture;
		hr = Audio.DeviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(Audio.DeviceEnumerator, flow, eMultimedia, &immdevice);
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
		hr = Audio.DeviceEnumerator->lpVtbl->GetDevice(Audio.DeviceEnumerator, native->Id, &immdevice);
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

	hr = endpoint->Client->lpVtbl->Initialize(endpoint->Client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		0, 0, wave_format, nullptr);
	if (FAILED(hr)) goto failed;

	hr = endpoint->Client->lpVtbl->SetEventHandle(endpoint->Client, endpoint->Commands[PL_AudioCommand_Update]);
	if (FAILED(hr)) goto failed;

	hr = endpoint->Client->lpVtbl->GetBufferSize(endpoint->Client, &endpoint->MaxFrames);
	if (FAILED(hr)) goto failed;

	if (kind == PL_AudioEndpoint_Render) {
		hr = endpoint->Client->lpVtbl->GetService(endpoint->Client, &PL_IID_IAudioRenderClient, &endpoint->Render);
	} else {
		hr = endpoint->Client->lpVtbl->GetService(endpoint->Client, &PL_IID_IAudioCaptureClient, &endpoint->Capture);
	}
	if (FAILED(hr)) goto failed;

	CoTaskMemFree(wave_format);
	immdevice->lpVtbl->Release(immdevice);

	InterlockedExchangePointer(&endpoint->EffectiveDevice, native);
	InterlockedExchange(&endpoint->DeviceLost, 0);

	PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_DEVICE_CHANGED,
		(WPARAM)endpoint->EffectiveDevice, (WPARAM)endpoint->DesiredDevice);

	if (kind == PL_AudioEndpoint_Render) {
		PL_AudioEndpoint_RenderFrames();
	} else {
		PL_AudioEndpoint_CaptureFrames();
	}

	if (endpoint->Resumed) {
		IAudioClient *client = endpoint->Client;
		HRESULT hr = client->lpVtbl->Start(client);
		if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED) {
			PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_RESUMED, kind, 0);
		} else {
			InterlockedExchange(&endpoint->DeviceLost, 1);
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

static void PL_AudioEndpoint_RestartDevice(PL_AudioEndpointKind kind) {
	PL_AudioDeviceNative *native = PL_AtomicCmpExgPtr(&Audio.Endpoints[kind].DesiredDevice, nullptr, nullptr);
	PL_AudioEndpoint_ReleaseDevice(kind);
	if (!PL_AudioEndpoint_TryLoadDevice(kind, native)) {
		PL_AudioEndpoint_ReleaseDevice(kind);
		if (native) {
			if (!PL_AudioEndpoint_TryLoadDevice(kind, nullptr)) {
				PL_AudioEndpoint_ReleaseDevice(kind);
				PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_DEVICE_LOST, kind, 0);
			}
		} else {
			PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_DEVICE_LOST, kind, 0);
		}
	}
}

static DWORD WINAPI PL_AudioThread(LPVOID param) {
	PL_AudioEndpointKind kind = (PL_AudioEndpointKind)param;

	HANDLE thread = GetCurrentThread();
	SetThreadDescription(thread, kind == PL_AudioEndpoint_Render ? L"PL Audio Render Thread" : L"PL Audio Capture Thread");

	PL_InitThreadContext();

	DWORD task_index;
	HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	PL_AudioEndpoint *endpoint = &Audio.Endpoints[kind];

	PL_AudioEndpoint_RestartDevice(kind);

	HRESULT hr;

	while (1) {
		DWORD wait = WaitForMultipleObjects(PL_AudioCommand_EnumCount, endpoint->Commands, FALSE, INFINITE);

		if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Update) {
			if (kind == PL_AudioEndpoint_Render)
				PL_AudioEndpoint_RenderFrames();
			else
				PL_AudioEndpoint_CaptureFrames();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Resume) {
			InterlockedExchange(&endpoint->Resumed, 1);
			if (!endpoint->DeviceLost) {
				hr = endpoint->Client->lpVtbl->Start(endpoint->Client);
				if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED) {
					PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_RESUMED, PL_AudioEndpoint_Render, 0);
				} else {
					InterlockedExchange(&endpoint->DeviceLost, 1);
				}
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Pause) {
			InterlockedExchange(&endpoint->Resumed, 0);
			if (!endpoint->DeviceLost) {
				hr = endpoint->Client->lpVtbl->Stop(endpoint->Client);
				if (SUCCEEDED(hr)) {
					if (!PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_PAUSED, PL_AudioEndpoint_Render, 0)) {
						TriggerBreakpoint();
					}
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
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Switch) {
			PL_AudioEndpoint_RestartDevice(kind);
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Terminate) {
			break;
		}
	}

	AvRevertMmThreadCharacteristics(avrt);

	return 0;
}

static bool PL_Media_Audio_IsAudioRendering(void) {
	if (InterlockedOr(&Audio.Endpoints[PL_AudioEndpoint_Render].DeviceLost, 0))
		return false;
	return InterlockedOr(&Audio.Endpoints[PL_AudioEndpoint_Render].Resumed, 0);
}

static void PL_Media_Audio_UpdateAudioRender(void) {
	if (!InterlockedOr(&Audio.Endpoints[PL_AudioEndpoint_Render].Resumed, 0)) {
		SetEvent(Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Update]);
	}
}

static void PL_Media_Audio_PauseAudioRender(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Pause]);
}

static void PL_Media_Audio_ResumeAudioRender(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Resume]);
}

static void PL_Media_Audio_ResetAudioRender(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Reset]);
}

static bool PL_Media_Audio_IsAudioCapturing(void) {
	if (InterlockedOr(&Audio.Endpoints[PL_AudioEndpoint_Capture].DeviceLost, 0))
		return false;
	return InterlockedOr(&Audio.Endpoints[PL_AudioEndpoint_Capture].Resumed, 0);
}

static void PL_Media_Audio_PauseAudioCapture(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Capture].Commands[PL_AudioCommand_Pause]);
}

static void PL_Media_Audio_ResumeAudioCapture(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Capture].Commands[PL_AudioCommand_Resume]);
}

static void PL_Media_Audio_ResetAudioCapture(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Capture].Commands[PL_AudioCommand_Reset]);
}

static void PL_Media_Audio_SetAudioDevice(PL_AudioDevice const *device) {
	Assert((device >= Media.IoDevice.AudioRenderDevices.Data && 
			device <= Media.IoDevice.AudioRenderDevices.Data + Media.IoDevice.AudioRenderDevices.Count) ||
			(device >= Media.IoDevice.AudioCaptureDevices.Data &&
			device <= Media.IoDevice.AudioCaptureDevices.Data + Media.IoDevice.AudioCaptureDevices.Count));

	PL_AudioDeviceNative * native   = device->Handle;
	PL_AudioEndpointKind   endpoint = native->IsCapture == 0 ? PL_AudioEndpoint_Render : PL_AudioEndpoint_Capture;

	PL_AudioDeviceNative *prev = InterlockedExchangePointer(&Audio.Endpoints[endpoint].DesiredDevice, native);
	if (prev != native) {
		SetEvent(Audio.Endpoints[endpoint].Commands[PL_AudioCommand_Switch]);
	}

	if (endpoint == PL_AudioEndpoint_Render) {
		Media.IoDevice.AudioRenderDevices.Current  = (PL_AudioDevice *)device;
	} else {
		Media.IoDevice.AudioCaptureDevices.Current = (PL_AudioDevice *)device;
	}
}

static bool PL_AudioEndpoint_Init(PL_AudioEndpointKind kind) {
	PL_AudioEndpoint *endpoint = &Audio.Endpoints[kind];

	for (int i = 0; i < PL_AudioCommand_EnumCount; ++i) {
		endpoint->Commands[i] = CreateEventExW(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
		if (!endpoint->Commands[i]) {
			return false;
		}
	}

	endpoint->Thread = CreateThread(nullptr, 0, PL_AudioThread, (LPVOID)kind, 0, 0);
	if (!endpoint->Thread) {
		return false;
	}

	if (kind == PL_AudioEndpoint_Render) {
		Media.VTable.IsAudioRendering   = PL_Media_Audio_IsAudioRendering;
		Media.VTable.UpdateAudioRender  = PL_Media_Audio_UpdateAudioRender;
		Media.VTable.PauseAudioRender   = PL_Media_Audio_PauseAudioRender;
		Media.VTable.ResumeAudioRender  = PL_Media_Audio_ResumeAudioRender;
		Media.VTable.ResetAudioRender   = PL_Media_Audio_ResetAudioRender;
	} else {
		Media.VTable.IsAudioCapturing   = PL_Media_Audio_IsAudioCapturing;
		Media.VTable.PauseAudioCapture  = PL_Media_Audio_PauseAudioCapture;
		Media.VTable.ResumeAudioCapture = PL_Media_Audio_ResumeAudioCapture;
		Media.VTable.ResetAudioCapture  = PL_Media_Audio_ResetAudioCapture;
	}

	return true;
}

static void PL_Media_Audio_InitVTable(void) {
	MSG msg;
	PeekMessageW(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	Audio.ParentThreadId = GetCurrentThreadId();

	Media.IoDevice.AudioRenderDevices.Data  = Media.AudioDeviceBuffer[PL_AudioEndpoint_Render].Data;
	Media.IoDevice.AudioCaptureDevices.Data = Media.AudioDeviceBuffer[PL_AudioEndpoint_Capture].Data;

	HRESULT hr = CoCreateInstance(&PL_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &PL_IID_IMMDeviceEnumerator, &Audio.DeviceEnumerator);
	if (FAILED(hr)) {
		LogWarning("Windows: Failed to create audio instance. Reason: %s\n", PL_GetLastError());
		return;
	}

	hr = Audio.DeviceEnumerator->lpVtbl->RegisterEndpointNotificationCallback(Audio.DeviceEnumerator, &NotificationClient);

	if (FAILED(hr)) {
		LogWarning("Windows: Failed to enumerate audio devices. Reason: %s\n", PL_GetLastError());
		Audio.DeviceEnumerator->lpVtbl->Release(Audio.DeviceEnumerator);
		Audio.DeviceEnumerator = nullptr;
		return;
	}

	PL_Media_EnumerateAudioDevices();

	Media.VTable.SetAudioDevice = PL_Media_Audio_SetAudioDevice;

	if (Media.Config.Audio.Render) {
		if (!PL_AudioEndpoint_Init(PL_AudioEndpoint_Render)) {
			PL_AudioEndpoint_Release(&Audio.Endpoints[PL_AudioEndpoint_Render]);
			LogWarning("Windows: Failed to initialize audio device for rendering. Reason: %s\n", PL_GetLastError());
		}
	}

	if (Media.Config.Audio.Capture) {
		if (!PL_AudioEndpoint_Init(PL_AudioEndpoint_Capture)) {
			PL_AudioEndpoint_Release(&Audio.Endpoints[PL_AudioEndpoint_Capture]);
			LogWarning("Windows: Failed to initialize audio device for capturing. Reason: %s\n", PL_GetLastError());
		}
	}

	Media.Features |= PL_Feature_Audio;
}

void PL_Media_Load(void) {
	u32 features       = Media.Config.Features;
	Media.UserVTable = Media.Config.User;
	Media.Flags      = Media.Config.Flags;

	if (features & PL_Feature_Window) {
		PL_Media_Window_InitVTable();
	}

	if (features & PL_Feature_Audio) {
		PL_Media_Audio_InitVTable();
	}
}

static void PL_Media_Window_Release(void) {
	if (Media.Features & PL_Feature_Window) {
		DestroyWindow(Window.Handle);
	}
}

static void PL_Media_Audio_Release(void) {
	if (Media.Features & PL_Feature_Audio) {
		for (int i = 0; i < PL_AudioEndpoint_EnumCount; ++i) {
			PL_AudioEndpoint *endpoint = &Audio.Endpoints[i];
			PL_AudioEndpoint_Release(endpoint);
		}

		if (Audio.DeviceEnumerator)
			Audio.DeviceEnumerator->lpVtbl->Release(Audio.DeviceEnumerator);

		while (Audio.FirstDevice.Next) {
			PL_AudioDeviceNative *native   = Audio.FirstDevice.Next;
			Audio.FirstDevice.Next = native->Next;
			CoTaskMemFree((void *)native->Id);
			CoTaskMemFree(native);
		}
	}
}

void PL_Media_Release(void) {
	PL_Media_Window_Release();
	PL_Media_Audio_Release();
	memset(&Media, 0, sizeof(Media));
}
