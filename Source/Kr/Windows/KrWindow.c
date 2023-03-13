#include "KrMain.h"

#pragma warning(push)
#pragma warning(disable: 5105)
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#pragma comment(lib, "Shell32.lib")
#pragma warning(pop)

typedef struct PL_WindowExtra {
	UINT            LastChar;
	WINDOWPLACEMENT Placement;
} PL_WindowExtra;

static struct {
	LONG volatile Count;
} Window;

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

	event.Target = (PL_Window *)wnd;

	switch (msg) {
		case WM_ACTIVATE: {
			int low    = LOWORD(wparam);
			event.Kind = (low == WA_ACTIVE || low == WA_CLICKACTIVE) ?
				PL_Event_WindowActivated : PL_Event_WindowDeactivated;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;

		case WM_CREATE: {
			res = DefWindowProcW(wnd, msg, wparam, lparam);
			PL_WindowExtra *extra = HeapAlloc(GetProcessHeap(), 0, sizeof(PL_WindowExtra));
			if (extra) {
				extra->LastChar = 0;
				GetWindowPlacement(wnd, &extra->Placement);
			}
			SetWindowLongPtrW(wnd, GWLP_USERDATA, (LONG_PTR)extra);
			event.Kind = PL_Event_WindowCreated;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_DESTROY: {
			event.Kind = PL_Event_WindowDestroyed;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			PL_WindowExtra *extra = (PL_WindowExtra *)GetWindowLongPtrW(wnd, GWLP_USERDATA);
			if (extra) HeapFree(GetProcessHeap(), 0, extra);
			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;

		case WM_CLOSE: {
			event.Kind = PL_Event_WindowClosed;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case WM_SIZE: {
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

			if (msg == WM_SYSKEYDOWN && wparam == VK_F4 && !(HIWORD(lparam) & KF_REPEAT)) {
				PostQuitMessage(0);
			}
		} break;

		case WM_CHAR: {
			PL_WindowExtra *ex  = (PL_WindowExtra *)GetWindowLongPtrW(wnd, GWLP_USERDATA);

			if (ex) {
				u32 value = (u32)wparam;

				if (IS_HIGH_SURROGATE(value)) {
					ex->LastChar = value;
				} else if (IS_LOW_SURROGATE(value)) {
					u32 high     = ex->LastChar;
					u32 low      = value;
					ex->LastChar = 0;
					u32 codepoint           = 
					event.Kind              = PL_Event_TextInput;
					event.Text.Codepoint    = (((high - 0xd800) << 10) + (low - 0xdc00) + 0x10000);
					Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
				} else {
					ex->LastChar            = 0;
					event.Kind              = PL_Event_TextInput;
					event.Text.Codepoint    = value;
					Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
				}
			} else {
				res = DefWindowProcW(wnd, msg, wparam, lparam);
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

static bool PL_Window_IsFullscreen(PL_Window *window) {
	if (!window) return false;

	HWND wnd       = (HWND)window;
	DWORD dw_style = (DWORD)GetWindowLongPtrW(wnd, GWL_STYLE);
	if (dw_style & WS_OVERLAPPEDWINDOW) {
		return false;
	}
	return true;
}

static void PL_Window_ToggleFullscreen(PL_Window *window) {
	if (!window) return;

	HWND hwnd           = (HWND)window;
	DWORD dw_style      = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
	PL_WindowExtra *ex  = (PL_WindowExtra *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

	if (!ex) return;

	if (dw_style & WS_OVERLAPPEDWINDOW) {
		MONITORINFO mi  = { sizeof(mi) };
		if (GetWindowPlacement(hwnd, &ex->Placement) &&
			GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
			SetWindowLongPtrW(hwnd, GWL_STYLE, dw_style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	} else {
		SetWindowLongPtrW(hwnd, GWL_STYLE, dw_style | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(hwnd, &ex->Placement);
		SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

//
// Public API
//

PL_Window *PL_CreateWindow(const char *mb_title, uint w, uint h, bool fullscreen) {
	if (InterlockedIncrement(&Window.Count) == 1) {
		Media.VTable.IsFullscreen     = PL_Window_IsFullscreen;
		Media.VTable.ToggleFullscreen = PL_Window_ToggleFullscreen;
	}

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

	if (mb_title) {
		MultiByteToWideChar(CP_UTF8, 0, mb_title, -1, title, ArrayCount(title));
	}

	int width  = w ? w : CW_USEDEFAULT;
	int height = h ? h : CW_USEDEFAULT;

	HWND wnd = CreateWindowExW(WS_EX_APPWINDOW, wnd_class.lpszClassName, title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, instance, nullptr);
	if (!wnd) {
		LogError("Failed to create window. Reason: %s\n", PL_GetLastError());
		return 0;
	}

	RAWINPUTDEVICE device;
	device.usUsagePage = 0x1;
	device.usUsage     = 0x2;
	device.dwFlags     = 0;
	device.hwndTarget  = wnd;

	RegisterRawInputDevices(&device, 1, sizeof(device));

	ShowWindow(wnd, SW_SHOWNORMAL);
	UpdateWindow(wnd);

	return (PL_Window *)wnd;
}

void PL_DestroyWindow(PL_Window *window) {
	HWND wnd = (HWND)window;
	DestroyWindow(wnd);

	if (InterlockedCompareExchange(&Window.Count, 0, 0) > 0) {
		if (InterlockedDecrement(&Window.Count)) {
			Media.VTable.IsFullscreen     = PL_Media_Fallback_IsFullscreen;
			Media.VTable.ToggleFullscreen = PL_Media_Fallback_ToggleFullscreen;
		}
	}
}
