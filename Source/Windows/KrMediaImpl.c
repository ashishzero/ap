#include "../KrMediaImpl.h"

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
// [Atomics]
//

static_assert(sizeof(i32) == sizeof(LONG), "");

proc i32   KrAtomicAdd(volatile i32 *dst, i32 val)                               { return InterlockedAdd(dst, val); }
proc i32   KrAtomicCmpExg(volatile i32 *dst, i32 exchange, i32 compare)          { return InterlockedCompareExchange(dst, exchange, compare); }
proc void *KrAtomicCmpExgPtr(void *volatile *dst, void *exchange, void *compare) { return InterlockedCompareExchangePointer(dst, exchange, compare); }
proc i32   KrAtomicExg(volatile i32 *dst, i32 val)                               { return InterlockedExchange(dst, val); }
proc void  KrAtomicLock(volatile i32 *lock)                                      { while (InterlockedCompareExchange(lock, 1, 0) == 1); }
proc void  KrAtomicUnlock(volatile i32 *lock)                                    { InterlockedExchange(lock, 0); }

//
// [Unicode]
//

inproc int PL_UTF16ToUTF8(char *utf8_buff, int utf8_buff_len, const char16_t *utf16_string) {
	int bytes_written = WideCharToMultiByte(CP_UTF8, 0, utf16_string, -1, utf8_buff, utf8_buff_len, 0, 0);
	return bytes_written;
}

//
// [Window]
//

#define KR_WM_AUDIO_RESUMED                   (WM_USER + 1)
#define KR_WM_AUDIO_PAUSED                    (WM_USER + 2)
#define KR_WM_AUDIO_RESET                     (WM_USER + 3)
#define KR_WM_AUDIO_RENDER_DEVICE_CHANGED     (WM_USER + 4)
#define KR_WM_AUDIO_RENDER_DEVICE_LOST        (WM_USER + 5)
#define KR_WM_AUDIO_RENDER_DEVICE_ACTIVATED   (WM_USER + 6)
#define KR_WM_AUDIO_RENDER_DEVICE_DEACTIVATED (WM_USER + 7)

static WINDOWPLACEMENT g_MainWindowPlacement;
static HWND            g_MainWindow;

static const KrKey VirtualKeyMap[] = {
	['A'] = KrKey_A, ['B'] = KrKey_B,
	['C'] = KrKey_C, ['D'] = KrKey_D,
	['E'] = KrKey_E, ['F'] = KrKey_F,
	['G'] = KrKey_G, ['H'] = KrKey_H,
	['I'] = KrKey_I, ['J'] = KrKey_J,
	['K'] = KrKey_K, ['L'] = KrKey_L,
	['M'] = KrKey_M, ['N'] = KrKey_N,
	['O'] = KrKey_O, ['P'] = KrKey_P,
	['Q'] = KrKey_Q, ['R'] = KrKey_R,
	['S'] = KrKey_S, ['T'] = KrKey_T,
	['U'] = KrKey_U, ['V'] = KrKey_V,
	['W'] = KrKey_W, ['X'] = KrKey_X,
	['Y'] = KrKey_Y, ['Z'] = KrKey_Z,

	['0'] = KrKey_0, ['1'] = KrKey_1,
	['2'] = KrKey_2, ['3'] = KrKey_3,
	['4'] = KrKey_4, ['5'] = KrKey_5,
	['6'] = KrKey_6, ['7'] = KrKey_7,
	['8'] = KrKey_8, ['9'] = KrKey_9,

	[VK_NUMPAD0] = KrKey_0, [VK_NUMPAD1] = KrKey_1,
	[VK_NUMPAD2] = KrKey_2, [VK_NUMPAD3] = KrKey_3,
	[VK_NUMPAD4] = KrKey_4, [VK_NUMPAD5] = KrKey_5,
	[VK_NUMPAD6] = KrKey_6, [VK_NUMPAD7] = KrKey_7,
	[VK_NUMPAD8] = KrKey_8, [VK_NUMPAD9] = KrKey_9,

	[VK_F1]  = KrKey_F1,  [VK_F2]  = KrKey_F2,
	[VK_F3]  = KrKey_F3,  [VK_F4]  = KrKey_F4,
	[VK_F5]  = KrKey_F5,  [VK_F6]  = KrKey_F6,
	[VK_F7]  = KrKey_F7,  [VK_F8]  = KrKey_F8,
	[VK_F9]  = KrKey_F9,  [VK_F10] = KrKey_F10,
	[VK_F11] = KrKey_F11, [VK_F12] = KrKey_F12,

	[VK_SNAPSHOT] = KrKey_PrintScreen,  [VK_INSERT]   = KrKey_Insert,
	[VK_HOME]     = KrKey_Home,         [VK_PRIOR]    = KrKey_PageUp,
	[VK_NEXT]     = KrKey_PageDown,     [VK_DELETE]   = KrKey_Delete,
	[VK_END]      = KrKey_End,          [VK_RIGHT]    = KrKey_Right,
	[VK_LEFT]     = KrKey_Left,         [VK_DOWN]     = KrKey_Down,
	[VK_UP]       = KrKey_Up,           [VK_DIVIDE]   = KrKey_Divide,
	[VK_MULTIPLY] = KrKey_Multiply,     [VK_ADD]      = KrKey_Plus,
	[VK_SUBTRACT] = KrKey_Minus,        [VK_DECIMAL]  = KrKey_Period,
	[VK_OEM_3]    = KrKey_BackTick,     [VK_CONTROL]  = KrKey_Ctrl,
	[VK_RETURN]   = KrKey_Return,       [VK_ESCAPE]   = KrKey_Escape,
	[VK_BACK]     = KrKey_Backspace,    [VK_TAB]      = KrKey_Tab,
	[VK_SPACE]    = KrKey_Space,        [VK_SHIFT]    = KrKey_Shift,
};

static void PL_NormalizeCursorPosition(HWND hwnd, int x, int y, i32 *nx, i32 *ny) {
	RECT rc;
	GetClientRect(hwnd, &rc);
	int window_w = rc.right - rc.left;
	int window_h = rc.bottom - rc.top;

	*nx = x;
	*ny = window_h - y;
}

static LRESULT CALLBACK PL_HandleWindowsEvent(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	LRESULT res = 0;

	KrEvent event = {0};

	switch (msg) {
		case WM_ACTIVATE:
		{
			int low = LOWORD(wparam);

			if (low == WA_ACTIVE || low == WA_CLICKACTIVE)
				event.Kind = KrEvent_WindowActivated;
			else
				event.Kind = KrEvent_WindowDeactivated;
			g_UserContext.OnEvent(&event, g_UserContext.Data);

			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;

		case WM_CREATE:
		{
			res = DefWindowProcW(wnd, msg, wparam, lparam);

			event.Kind = KrEvent_WindowCreated;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_DESTROY:
		{
			event.Kind = KrEvent_WindowDestroyed;
			g_UserContext.OnEvent(&event, g_UserContext.Data);

			res = DefWindowProcW(wnd, msg, wparam, lparam);
			PostQuitMessage(0);
		} break;

		case WM_SIZE:
		{
			int x = LOWORD(lparam);
			int y = HIWORD(lparam);

			event.Kind = KrEvent_WindowResized;
			event.Window.Width  = x;
			event.Window.Height = y;
			g_UserContext.OnEvent(&event, g_UserContext.Data);

			inproc void PL_ResizeRenderTargets(u32 width, u32 height);

			PL_ResizeRenderTargets(x, y);
		} break;

		case WM_MOUSEMOVE: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEvent_MouseMoved;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_LBUTTONUP:
		case WM_LBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = msg == WM_LBUTTONDOWN ? KrEvent_ButtonPressed : KrEvent_ButtonReleased;
			event.Mouse.Button = KrButton_Left;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_RBUTTONUP:
		case WM_RBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = msg == WM_LBUTTONDOWN ? KrEvent_ButtonPressed : KrEvent_ButtonReleased;
			event.Mouse.Button = KrButton_Right;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_MBUTTONUP:
		case WM_MBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = msg == WM_LBUTTONDOWN ? KrEvent_ButtonPressed : KrEvent_ButtonReleased;
			event.Mouse.Button = KrButton_Middle;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_LBUTTONDBLCLK: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEvent_DoubleClicked;
			event.Mouse.Button = KrButton_Left;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_RBUTTONDBLCLK: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEvent_DoubleClicked;
			event.Mouse.Button = KrButton_Right;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_MBUTTONDBLCLK: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEvent_DoubleClicked;
			event.Mouse.Button = KrButton_Middle;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_MOUSEWHEEL: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEvent_WheelMoved;
			event.Wheel.Vert = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
			event.Wheel.Horz = 0.0f;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Wheel.X, &event.Wheel.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_MOUSEHWHEEL: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEvent_WheelMoved;
			event.Wheel.Vert = 0.0f;
			event.Wheel.Horz = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Wheel.X, &event.Wheel.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_KEYUP:
		case WM_KEYDOWN: {
			if (wparam < ArrayCount(VirtualKeyMap)) {
				event.Kind = msg == WM_KEYDOWN ? KrEvent_KeyPressed : KrEvent_KeyReleased;
				event.Key.Code   = VirtualKeyMap[wparam];
				event.Key.Repeat = HIWORD(lparam) & KF_REPEAT;
				g_UserContext.OnEvent(&event, g_UserContext.Data);
			}
		} break;

		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN: {
			if (wparam == VK_F10) {
				event.Kind = msg == WM_KEYDOWN ? KrEvent_KeyPressed : KrEvent_KeyReleased;
				event.Key.Code = KrKey_F10;
				event.Key.Repeat = HIWORD(lparam) & KF_REPEAT;
				g_UserContext.OnEvent(&event, g_UserContext.Data);
			}
		} break;

		case WM_CHAR: {
			event.Kind = KrEvent_TextInput;
			event.Text.Code = (u16)wparam;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_DPICHANGED:
		{
			RECT *rect   = (RECT *)lparam;
			LONG  left   = rect->left;
			LONG  top    = rect->top;
			LONG  width  = rect->right - rect->left;
			LONG  height = rect->bottom - rect->top;
			SetWindowPos(wnd, 0, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
		} break;

		case KR_WM_AUDIO_RESUMED:
		{
			event.Kind = KrEvent_AudioResumed;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case KR_WM_AUDIO_PAUSED:
		{
			event.Kind = KrEvent_AudioPaused;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case KR_WM_AUDIO_RESET:
		{
			event.Kind = KrEvent_AudioReset;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case KR_WM_AUDIO_RENDER_DEVICE_CHANGED:
		{
			event.Kind = KrEvent_AudioRenderDeviceChanged;
			event.AudioDevice.Id   = (void *)wparam;
			event.AudioDevice.Name = (char *)lparam;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case KR_WM_AUDIO_RENDER_DEVICE_LOST:
		{
			event.Kind = KrEvent_AudioRenderDeviceLost;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case KR_WM_AUDIO_RENDER_DEVICE_ACTIVATED:
		{
			event.Kind = KrEvent_AudioRenderDeviceActived;
			event.AudioDevice.Id   = (void *)wparam;
			event.AudioDevice.Name = (char *)lparam;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case KR_WM_AUDIO_RENDER_DEVICE_DEACTIVATED:
		{
			event.Kind = KrEvent_AudioRenderDeviceDeactived;
			event.AudioDevice.Id   = (void *)wparam;
			event.AudioDevice.Name = (char *)lparam;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		default:
		{
			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;
	}

	return res;
}

inproc bool PL_KrIsFullscreen() {
	HWND hwnd     = g_MainWindow;
	DWORD dwStyle = GetWindowLongW(hwnd, GWL_STYLE);
	if (dwStyle & WS_OVERLAPPEDWINDOW) {
		return false;
	}
	return true;
}

inproc void PL_KrToggleFullscreen() {
	HWND hwnd     = g_MainWindow;
	DWORD dwStyle = GetWindowLongW(hwnd, GWL_STYLE);
	if (dwStyle & WS_OVERLAPPEDWINDOW) {
		MONITORINFO mi = { sizeof(mi) };
		if (GetWindowPlacement(hwnd, &g_MainWindowPlacement) &&
			GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
			SetWindowLongW(hwnd, GWL_STYLE, dwStyle & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
				mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	} else {
		SetWindowLongW(hwnd, GWL_STYLE, dwStyle | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(hwnd, &g_MainWindowPlacement);
		SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

inproc void PL_UninitializeWindow() {
	g_Library.Window = LibraryFallback.Window;

	if (g_MainWindow) {
		DestroyWindow(g_MainWindow);
		g_MainWindow = nullptr;
	}
}

inproc void PL_InitializeWindow() {
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

	LPWSTR wnd_title = nullptr;
	if (LoadStringW(instance, 102, (LPWSTR)&wnd_title, 0) == 0) {
		wnd_title = L"Kr Window";
	}

	g_MainWindow = CreateWindowExW(WS_EX_APPWINDOW, wnd_class.lpszClassName, wnd_title, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, instance, nullptr);
	if (!g_MainWindow) {
		Unimplemented();
	}

	ShowWindow(g_MainWindow, SW_SHOWNORMAL);
	UpdateWindow(g_MainWindow);

	GetWindowPlacement(g_MainWindow, &g_MainWindowPlacement);

	g_Library.Window = (KrWindowLibrary){
		.IsFullscreen     = PL_KrIsFullscreen,
		.ToggleFullscreen = PL_KrToggleFullscreen,
	};
}

//
// [Audio]
//

DEFINE_GUID(PL_CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);

DEFINE_GUID(PL_IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(PL_IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(PL_IID_IMMNotificationClient, 0x7991eec9, 0x7e89, 0x4d85, 0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0);
DEFINE_GUID(PL_IID_IMMEndpoint, 0x1BE09788, 0x6894, 0x4089, 0x85, 0x86, 0x9A, 0x2A, 0x6C, 0x26, 0x5A, 0xC5);
DEFINE_GUID(PL_IID_IAudioClient, 0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(PL_IID_IAudioRenderClient, 0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);

DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX, 0x00000000L, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

inproc HRESULT STDMETHODCALLTYPE PL_QueryInterface(IMMNotificationClient *, REFIID, void **);
inproc ULONG   STDMETHODCALLTYPE PL_AddRef(IMMNotificationClient *);
inproc ULONG   STDMETHODCALLTYPE PL_Release(IMMNotificationClient *);
inproc HRESULT STDMETHODCALLTYPE PL_OnDeviceStateChanged(IMMNotificationClient *, LPCWSTR, DWORD);
inproc HRESULT STDMETHODCALLTYPE PL_OnDeviceAdded(IMMNotificationClient *, LPCWSTR);
inproc HRESULT STDMETHODCALLTYPE PL_OnDeviceRemoved(IMMNotificationClient *, LPCWSTR);
inproc HRESULT STDMETHODCALLTYPE PL_OnDefaultDeviceChanged(IMMNotificationClient *, EDataFlow, ERole, LPCWSTR);
inproc HRESULT STDMETHODCALLTYPE PL_OnPropertyValueChanged(IMMNotificationClient *, LPCWSTR, const PROPERTYKEY);

typedef struct PL_KrDeviceName {
	char Buffer[64];
} PL_KrDeviceName;

typedef struct PL_AudioDevice {
	struct PL_AudioDevice *Next;
	char *                 Name;
	KrAudioDeviceFlow      Flow;
	volatile LONG          IsActive;
	volatile LONG          Generation;
	LPWSTR                 Id;
	PL_KrDeviceName        Names[2];
} PL_AudioDevice;

typedef enum PL_AudioEvent {
	PL_AudioEvent_Update,
	PL_AudioEvent_Resume,
	PL_AudioEvent_Pause,
	PL_AudioEvent_Reset,
	PL_AudioEvent_DeviceLostLoadDefault,
	PL_AudioEvent_LoadDesiredDevice,
	PL_AudioEvent_Exit,
	PL_AudioEvent_EnumMax
} PL_AudioEvent;

static IMMNotificationClientVtbl g_NotificationClientVTable = {
	.QueryInterface         = PL_QueryInterface,
	.AddRef                 = PL_AddRef,
	.Release                = PL_Release,
	.OnDeviceStateChanged   = PL_OnDeviceStateChanged,
	.OnDeviceAdded          = PL_OnDeviceAdded,
	.OnDeviceRemoved        = PL_OnDeviceRemoved,
	.OnDefaultDeviceChanged = PL_OnDefaultDeviceChanged,
	.OnPropertyValueChanged = PL_OnPropertyValueChanged,
};

static IMMNotificationClient g_NotificationClient = {
	.lpVtbl = &g_NotificationClientVTable
};

static IMMDeviceEnumerator * g_AudioDeviceEnumerator;
static PL_AudioDevice        g_AudioDeviceListHead = { .Name = "Default", .Flow = KrAudioDeviceFlow_All };
static volatile LONG         g_AudioDeviceListLock;

static struct {
	IAudioClient         *Client;
	IAudioRenderClient   *RenderClient;
	KrAudioSpec           Spec;
	UINT                  MaxFrames;
	HANDLE                Events[PL_AudioEvent_EnumMax];
	volatile LONG         DeviceIsLost;
	volatile LONG         DeviceIsResumed;
	PL_AudioDevice *      DesiredDevice;
	PL_AudioDevice *      EffectiveDevice;
	LONG                  EffectiveGeneration;
	HANDLE                Thread;
} g_AudioOut;

inproc IMMDevice *PL_GetDefaultIMMDevice(EDataFlow flow) {
	IMMDevice *immdevice = nullptr;
	HRESULT hr = g_AudioDeviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(g_AudioDeviceEnumerator, flow, eMultimedia, &immdevice);
	if (SUCCEEDED(hr)) {
		return immdevice;
	}
	Unimplemented();
	return nullptr;
}

inproc IMMDevice *PL_IMMDeviceFromId(LPCWSTR id) {
	IMMDevice *immdevice = nullptr;
	HRESULT hr = g_AudioDeviceEnumerator->lpVtbl->GetDevice(g_AudioDeviceEnumerator, id, &immdevice);
	if (SUCCEEDED(hr)) {
		return immdevice;
	}
	Unimplemented();
	return nullptr;
}

inproc LPWSTR PL_IMMDeviceId(IMMDevice *immdevice) {
	LPWSTR  id = nullptr;
	HRESULT hr = immdevice->lpVtbl->GetId(immdevice, &id);
	if (FAILED(hr)) {
		Unimplemented();
	}
	return id;
}

inproc LONG PL_IMMDeviceIsActive(IMMDevice *immdevice) {
	DWORD state = 0;
	HRESULT hr  = immdevice->lpVtbl->GetState(immdevice, &state);
	if (FAILED(hr)) {
		Unimplemented();
	}
	return state == DEVICE_STATE_ACTIVE;
}

inproc KrAudioDeviceFlow PL_IMMDeviceFlow(IMMDevice *immdevice) {
	IMMEndpoint *endpoint = nullptr;
	HRESULT hr = immdevice->lpVtbl->QueryInterface(immdevice, &PL_IID_IMMEndpoint, &endpoint);
	if (FAILED(hr)) {
		Unimplemented();
	}

	EDataFlow flow = eRender;
	hr = endpoint->lpVtbl->GetDataFlow(endpoint, &flow);
	if (FAILED(hr)) {
		Unimplemented();
	}
	endpoint->lpVtbl->Release(endpoint);

	KrAudioDeviceFlow result = flow == eRender ? KrAudioDeviceFlow_Render : KrAudioDeviceFlow_Capture;

	return result;
}

inproc PL_KrDeviceName PL_IMMDeviceName(IMMDevice *immdevice) {
	PL_KrDeviceName name = {0};

	IPropertyStore *prop = nullptr;
	HRESULT hr = immdevice->lpVtbl->OpenPropertyStore(immdevice, STGM_READ, &prop);

	if (SUCCEEDED(hr)) {
		PROPVARIANT pv;
		hr = prop->lpVtbl->GetValue(prop, &PKEY_Device_FriendlyName, &pv);
		if (SUCCEEDED(hr)) {
			if (pv.vt == VT_LPWSTR) {
				PL_UTF16ToUTF8(name.Buffer, sizeof(name.Buffer), pv.pwszVal);
			}
		}
		prop->lpVtbl->Release(prop);
	}

	if (FAILED(hr)) {
		Unimplemented();
	}

	return name;
}

inproc PL_AudioDevice *PL_FindAudioDeviceFromId(LPCWSTR id) {
	PL_AudioDevice *device = g_AudioDeviceListHead.Next;
	for (; device; device = device->Next) {
		int res = wcscmp(id, device->Id);
		if (res == 0)
			return device;
	}
	return nullptr;
}

inproc PL_AudioDevice *PL_UpdateAudioDeviceList(IMMDevice *immdevice) {
	LPWSTR id = PL_IMMDeviceId(immdevice);
	if (!id) {
		return nullptr;
	}

	PL_AudioDevice *device = CoTaskMemAlloc(sizeof(*device));
	if (!device) {
		CoTaskMemFree(id);
		return nullptr;
	}

	memset(device, 0, sizeof(*device));

	device->Id         = id;
	device->Names[0]   = PL_IMMDeviceName(immdevice);
	device->Flow       = PL_IMMDeviceFlow(immdevice);
	device->IsActive   = PL_IMMDeviceIsActive(immdevice);
	device->Generation = 1;
	device->Name       = device->Names[0].Buffer;

	// Insert in alphabetical order
	PL_AudioDevice *prev = &g_AudioDeviceListHead;
	for (; prev->Next; prev = prev->Next) {
		if (strcmp(prev->Next->Name, device->Name) >= 0)
			break;
	}

	device->Next = prev->Next;
	InterlockedExchangePointer(&prev->Next, device);

	return device;
}

inproc PL_AudioDevice *PL_GetDefaultAudioDevice(KrAudioDeviceFlow flow) {
	PL_AudioDevice *device = nullptr;

	IMMDevice *immdevice = PL_GetDefaultIMMDevice(flow == KrAudioDeviceFlow_Render ? eRender : eConsole);
	if (immdevice ) {
		LPWSTR id = PL_IMMDeviceId(immdevice);
		if (id) {
			device = PL_FindAudioDeviceFromId(id);
			if (!device) {
				// This shouldn't really happen but oh well
				device = PL_UpdateAudioDeviceList(immdevice);
			}
			CoTaskMemFree(id);
		}
		immdevice ->lpVtbl->Release(immdevice );
	}

	return device;
}

inproc HRESULT STDMETHODCALLTYPE PL_QueryInterface(IMMNotificationClient *This, REFIID riid, void **ppv_object) {
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

inproc ULONG STDMETHODCALLTYPE PL_AddRef(IMMNotificationClient *This)  { return 1; }
inproc ULONG STDMETHODCALLTYPE PL_Release(IMMNotificationClient *This) { return 1; }

inproc HRESULT STDMETHODCALLTYPE PL_OnDeviceStateChanged(IMMNotificationClient *This, LPCWSTR deviceid, DWORD newstate) {
	KrAtomicLock(&g_AudioDeviceListLock);

	PL_AudioDevice *device = PL_FindAudioDeviceFromId(deviceid);
	u32 active = (newstate == DEVICE_STATE_ACTIVE);
	u32 prev   = 0;

	if (device) {
		prev = InterlockedExchange(&device->IsActive, active);
		InterlockedIncrement(&device->Generation);
	} else {
		// New device added
		IMMDevice *immdevice = PL_IMMDeviceFromId(deviceid);
		if (immdevice) {
			device = PL_UpdateAudioDeviceList(immdevice);
			immdevice->lpVtbl->Release(immdevice);
		}
	}

	KrAtomicUnlock(&g_AudioDeviceListLock);

	if (device && prev != active) {
		if (device->Flow == KrAudioDeviceFlow_Render) {
			UINT msg = active ? KR_WM_AUDIO_RENDER_DEVICE_ACTIVATED : KR_WM_AUDIO_RENDER_DEVICE_DEACTIVATED;
			PostMessageW(g_MainWindow, msg, (WPARAM)device, (LPARAM)device->Name);
		}
	}

	PL_AudioDevice *desired = InterlockedCompareExchangePointer(&g_AudioOut.DesiredDevice, nullptr, nullptr);

	// If desired is nullptr, the default notification will handle the device as required
	if (desired == nullptr)
		return S_OK;

	PL_AudioDevice *effective = InterlockedCompareExchangePointer(&g_AudioOut.EffectiveDevice, nullptr, nullptr);

	if (active) {
		if (device == desired) {
			ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_LoadDesiredDevice], 1, 0);
		} else if (!effective) {
			ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_DeviceLostLoadDefault], 1, 0);
		}
	} else {
		if (device == effective) {
			ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_DeviceLostLoadDefault], 1, 0);
		}
	}

	return S_OK;
}

// These events is handled in "OnDeviceStateChanged"
inproc HRESULT STDMETHODCALLTYPE PL_OnDeviceAdded(IMMNotificationClient *This, LPCWSTR deviceid)   { return S_OK; }
inproc HRESULT STDMETHODCALLTYPE PL_OnDeviceRemoved(IMMNotificationClient *This, LPCWSTR deviceid) { return S_OK; }

inproc HRESULT STDMETHODCALLTYPE PL_OnDefaultDeviceChanged(IMMNotificationClient *This, EDataFlow flow, ERole role, LPCWSTR deviceid) {
	if (flow == eRender && role == eMultimedia) {
		if (!deviceid) {
			// When there's no audio device in the system
			PostMessageW(g_MainWindow, KR_WM_AUDIO_RENDER_DEVICE_LOST, 0, 0);
			return S_OK;
		}

		PL_AudioDevice *desired = InterlockedCompareExchangePointer(&g_AudioOut.DesiredDevice, nullptr, nullptr);

		if (desired == nullptr) {
			ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_LoadDesiredDevice], 1, 0);
		}
	}

	return S_OK;
}

inproc HRESULT STDMETHODCALLTYPE PL_OnPropertyValueChanged(IMMNotificationClient *This, LPCWSTR deviceid, const PROPERTYKEY key) {
	if (memcmp(&key, &PKEY_Device_FriendlyName, sizeof(PROPERTYKEY)) == 0) {
		KrAtomicLock(&g_AudioDeviceListLock);

		IMMDevice *immdevice = PL_IMMDeviceFromId(deviceid);
		if (immdevice) {
			PL_AudioDevice *device = PL_FindAudioDeviceFromId(deviceid);
			if (device) {
				char *name_buff;
				if (device->Names[0].Buffer == device->Name) {
					device->Names[1] = PL_IMMDeviceName(immdevice);
					name_buff = device->Names[1].Buffer;
				} else {
					device->Names[0] = PL_IMMDeviceName(immdevice);
					name_buff = device->Names[0].Buffer;
				}
				InterlockedExchangePointer(&device->Name, name_buff);
			} else {
				PL_UpdateAudioDeviceList(immdevice);
			}
			immdevice->lpVtbl->Release(immdevice);
		}

		KrAtomicUnlock(&g_AudioDeviceListLock);
	}
	return S_OK;
}

inproc bool PL_SetAudioSpec(WAVEFORMATEX *wave_format) {
	const WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)wave_format;

	if (wave_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT &&
		wave_format->wBitsPerSample == 32) {
		g_AudioOut.Spec.Format = KrAudioFormat_R32;
	} else if (wave_format->wFormatTag == WAVE_FORMAT_PCM &&
		wave_format->wBitsPerSample == 32) {
		g_AudioOut.Spec.Format = KrAudioFormat_I32;
	} else if (wave_format->wFormatTag == WAVE_FORMAT_PCM &&
		wave_format->wBitsPerSample == 16) {
		g_AudioOut.Spec.Format = KrAudioFormat_I16;
	} else if (wave_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 32)) {
			g_AudioOut.Spec.Format = KrAudioFormat_R32;
		} else if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 32)) {
			g_AudioOut.Spec.Format = KrAudioFormat_I32;
		} else if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 16)) {
			g_AudioOut.Spec.Format = KrAudioFormat_I16;
		} else {
			Unimplemented();
			return false;
		}
	} else {
		Unimplemented();
		return false;
	}

	g_AudioOut.Spec.Channels  = wave_format->nChannels;
	g_AudioOut.Spec.Frequency = wave_format->nSamplesPerSec;

	return true;
}

inproc void PL_ReleaseAudioRenderDevice() {
	if (g_AudioOut.RenderClient) {
		g_AudioOut.RenderClient->lpVtbl->Release(g_AudioOut.RenderClient);
		g_AudioOut.RenderClient = nullptr;
	}
	if (g_AudioOut.Client) {
		g_AudioOut.Client->lpVtbl->Release(g_AudioOut.Client);
		g_AudioOut.Client = nullptr;
	}
	g_AudioOut.MaxFrames = 0;
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
	InterlockedExchange(&g_AudioOut.EffectiveGeneration, 0);
	InterlockedExchangePointer(&g_AudioOut.EffectiveDevice, nullptr);
}

inproc bool PL_PrepareAudioRenderDevice(PL_AudioDevice *device) {
	WAVEFORMATEX *wave_format = nullptr;

	IMMDevice *immdevice = PL_IMMDeviceFromId(device->Id);
	if (!immdevice) return false;

	HRESULT hr = immdevice->lpVtbl->Activate(immdevice, &PL_IID_IAudioClient, CLSCTX_ALL, nullptr, &g_AudioOut.Client);
	if (FAILED(hr)) goto failed;

	hr = g_AudioOut.Client->lpVtbl->GetMixFormat(g_AudioOut.Client, &wave_format);
	if (FAILED(hr)) goto failed;

	if (!PL_SetAudioSpec(wave_format)) {
		Unimplemented();
		CoTaskMemFree(wave_format);
		immdevice->lpVtbl->Release(immdevice);
		PL_ReleaseAudioRenderDevice();
	}

	REFERENCE_TIME device_period;
	hr = g_AudioOut.Client->lpVtbl->GetDevicePeriod(g_AudioOut.Client, nullptr, &device_period);
	if (FAILED(hr)) goto failed;

	hr = g_AudioOut.Client->lpVtbl->Initialize(g_AudioOut.Client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		device_period, device_period, wave_format, nullptr);
	if (FAILED(hr)) goto failed;

	hr = g_AudioOut.Client->lpVtbl->SetEventHandle(g_AudioOut.Client, g_AudioOut.Events[PL_AudioEvent_Update]);
	if (FAILED(hr)) goto failed;

	hr = g_AudioOut.Client->lpVtbl->GetBufferSize(g_AudioOut.Client, &g_AudioOut.MaxFrames);
	if (FAILED(hr)) goto failed;

	hr = g_AudioOut.Client->lpVtbl->GetService(g_AudioOut.Client, &PL_IID_IAudioRenderClient, &g_AudioOut.RenderClient);
	if (FAILED(hr)) goto failed;

	immdevice->lpVtbl->Release(immdevice);

	g_AudioOut.EffectiveGeneration = InterlockedCompareExchange(&device->Generation, 0, 0);
	InterlockedExchangePointer(&g_AudioOut.EffectiveDevice, (PL_AudioDevice *)device);
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 0);

	return true;

failed:
	if (immdevice)
		immdevice->lpVtbl->Release(immdevice);

	if (wave_format)
		CoTaskMemFree(wave_format);

	if (hr == E_NOTFOUND || hr == AUDCLNT_E_DEVICE_INVALIDATED)
		Unimplemented();
	Unimplemented();
	PL_ReleaseAudioRenderDevice(immdevice);
	return false;
}

inproc bool PL_GetAudioBuffer(BYTE **data, UINT *size) {
	UINT32 padding = 0;
	HRESULT hr = g_AudioOut.Client->lpVtbl->GetCurrentPadding(g_AudioOut.Client, &padding);
	if (FAILED(hr)) goto failed;

	*size = g_AudioOut.MaxFrames - padding;
	if (*size == 0) {
		hr = g_AudioOut.RenderClient->lpVtbl->ReleaseBuffer(g_AudioOut.RenderClient, 0, 0);
		if (FAILED(hr)) goto failed;
		return false;
	}

	hr = g_AudioOut.RenderClient->lpVtbl->GetBuffer(g_AudioOut.RenderClient, *size, data);
	if (FAILED(hr)) goto failed;

	return true;

failed:
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
	Unimplemented();
	return false;
}

inproc void PL_ReleaseAudioBuffer(u32 written, u32 frames) {
	DWORD flags = written < frames ? AUDCLNT_BUFFERFLAGS_SILENT : 0;
	HRESULT hr = g_AudioOut.RenderClient->lpVtbl->ReleaseBuffer(g_AudioOut.RenderClient, written, flags);
	if (SUCCEEDED(hr))
		return;

	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
	Unimplemented();
}

inproc void PL_UpdateAudio() {
	if (g_AudioOut.DeviceIsLost)
		return;

	BYTE *data = nullptr;
	u32 frames = 0;
	if (PL_GetAudioBuffer(&data, &frames)) {
		u32 written = g_UserContext.OnUploadAudio(&g_AudioOut.Spec, data, frames, g_UserContext.Data);
		PL_ReleaseAudioBuffer(written, frames);
	}
}

inproc void PL_ResumeAudio() {
	InterlockedExchange(&g_AudioOut.DeviceIsResumed, 1);

	if (g_AudioOut.DeviceIsLost)
		return;

	HRESULT hr = g_AudioOut.Client->lpVtbl->Start(g_AudioOut.Client);
	if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED) {
		PostMessageW(g_MainWindow, KR_WM_AUDIO_RESUMED, 0, 0);
		return;
	}
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
	Unimplemented();
}

inproc void PL_PauseAudio() {
	InterlockedExchange(&g_AudioOut.DeviceIsResumed, 0);

	if (g_AudioOut.DeviceIsLost)
		return;

	HRESULT hr = g_AudioOut.Client->lpVtbl->Stop(g_AudioOut.Client);
	if (SUCCEEDED(hr)) {
		PostMessageW(g_MainWindow, KR_WM_AUDIO_PAUSED, 0, 0);
		return;
	}
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
	Unimplemented();
}

inproc void PL_ResetAudio() {
	PL_PauseAudio();

	if (g_AudioOut.DeviceIsLost)
		return;

	HRESULT hr = g_AudioOut.Client->lpVtbl->Reset(g_AudioOut.Client);
	if (SUCCEEDED(hr)) {
		PostMessageW(g_MainWindow, KR_WM_AUDIO_RESET, 0, 0);
		return;
	}
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
}

inproc void PL_RestartAudioDevice(PL_AudioDevice **devices_list, int count) {
	PL_ReleaseAudioRenderDevice();
	for (int index = 0; index < count; ++index) {
		PL_AudioDevice *device = devices_list[index];
		if (PL_PrepareAudioRenderDevice(device)) {
			PostMessageW(g_MainWindow, KR_WM_AUDIO_RENDER_DEVICE_CHANGED,
				(WPARAM)g_AudioOut.EffectiveDevice, (LPARAM)g_AudioOut.EffectiveDevice->Name);
			PL_UpdateAudio();
			if (g_AudioOut.DeviceIsResumed) {
				PL_ResumeAudio();
			}
			return;
		}
	}
	InterlockedExchangePointer(&g_AudioOut.EffectiveDevice, nullptr);
}

inproc DWORD WINAPI PL_AudioThread(LPVOID param) {
	HANDLE thread = GetCurrentThread();
	SetThreadDescription(thread, L"PL Audio Thread");

	DWORD task_index;
	HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	{
		PL_AudioDevice *desired  =  g_AudioOut.DesiredDevice;
		PL_AudioDevice *default_ = PL_GetDefaultAudioDevice(KrAudioDeviceFlow_Render);

		int count = 0;
		PL_AudioDevice *devices_priority_list[2];
		if (desired)  devices_priority_list[count++] = desired;
		if (default_) devices_priority_list[count++] = default_;

		if (count) {
			PL_RestartAudioDevice(devices_priority_list, count);
		} else {
			PostMessageW(g_MainWindow, KR_WM_AUDIO_RENDER_DEVICE_LOST, 0, 0);
			g_AudioOut.DeviceIsLost = 1;
		}
	}

	while (1) {
		DWORD wait = WaitForMultipleObjects(PL_AudioEvent_EnumMax, g_AudioOut.Events, FALSE, INFINITE);

		HRESULT hr = S_OK;

		if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Update) {
			PL_UpdateAudio();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Resume) {
			PL_ResumeAudio();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Pause) {
			PL_PauseAudio();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Reset) {
			PL_ResetAudio();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_DeviceLostLoadDefault) {
			PL_ReleaseAudioRenderDevice();
			PL_AudioDevice *default_ = PL_GetDefaultAudioDevice(KrAudioDeviceFlow_Render);
			if (default_) {
				PL_RestartAudioDevice(&default_, 1);
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_LoadDesiredDevice) {
			PL_AudioDevice *default_  = PL_GetDefaultAudioDevice(KrAudioDeviceFlow_Render);
			PL_AudioDevice *desired   = InterlockedCompareExchangePointer(&g_AudioOut.DesiredDevice, nullptr, nullptr);
			PL_AudioDevice *effective = InterlockedCompareExchangePointer(&g_AudioOut.EffectiveDevice, nullptr, nullptr);

			int count = 0;
			PL_AudioDevice *devices_priority_list[2];
			if (desired)  devices_priority_list[count++] = desired;
			if (default_) devices_priority_list[count++] = default_;

			if (count) {
				if (desired) {
					if (effective != desired || effective->Generation > g_AudioOut.EffectiveGeneration) {
						PL_RestartAudioDevice(devices_priority_list, count);
					}
				} else {
					if (effective != default_ || effective->Generation > g_AudioOut.EffectiveGeneration) {
						PL_RestartAudioDevice(devices_priority_list, count);
					}
				}
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Exit) {
			break;
		}
	}

	PL_ReleaseAudioRenderDevice();

	AvRevertMmThreadCharacteristics(avrt);

	return 0;
}

inproc void PL_UninitializeAudio() {
	g_Library.Audio = LibraryFallback.Audio;

	if (g_AudioOut.Thread) {
		ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_Exit], 1, 0);

		WaitForSingleObject(g_AudioOut.Thread, INFINITE);
		CloseHandle(g_AudioOut.Thread);
	}

	memset(&g_AudioOut, 0, sizeof(g_AudioOut));

	if (g_AudioDeviceEnumerator) {
		g_AudioDeviceEnumerator->lpVtbl->UnregisterEndpointNotificationCallback(g_AudioDeviceEnumerator, &g_NotificationClient);
		g_AudioDeviceEnumerator->lpVtbl->Release(g_AudioDeviceEnumerator);
		g_AudioDeviceEnumerator = nullptr;
	}

	for (PL_AudioDevice *ptr = (PL_AudioDevice *)g_AudioDeviceListHead.Next; ptr; ) {
		PL_AudioDevice *next = (PL_AudioDevice *)ptr->Next;
		CoTaskMemFree((void *)ptr->Id);
		CoTaskMemFree(ptr);
		ptr = next;
	}

	g_AudioDeviceListHead.Next = nullptr;

	g_AudioDeviceListLock = 0;

	for (int i = 0; i < PL_AudioEvent_EnumMax; ++i)
		CloseHandle(g_AudioOut.Events[i]);
	memset(g_AudioOut.Events, 0, sizeof(g_AudioOut.Events));
}

inproc bool PL_KrAudioIsPlaying() {
	LONG lost = InterlockedOr(&g_AudioOut.DeviceIsLost, 0);
	if (lost) return false;
	return InterlockedOr(&g_AudioOut.DeviceIsResumed, 0);
}

inproc void PL_KrAudioUpdate() {
	if (!InterlockedOr(&g_AudioOut.DeviceIsResumed, 0)) {
		SetEvent(g_AudioOut.Events[PL_AudioEvent_Update]);
	}
}

inproc void PL_KrAudioResume() {
	ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_Resume], 1, 0);
}

inproc void PL_KrAudioPause() {
	ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_Pause], 1, 0);
}

inproc void PL_KrAudioReset() {
	ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_Reset], 1, 0);
}

inproc bool PL_KrAudioSetRenderDevice(KrAudioDeviceId id) {
	PL_AudioDevice *device = id;
	if (device && device->Flow == KrAudioDeviceFlow_Capture)
		return false;
	PL_AudioDevice *prev = InterlockedExchangePointer(&g_AudioOut.DesiredDevice, device);
	if (prev != device) {
		ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_LoadDesiredDevice], 1, 0);
	}
	return true;
}

inproc uint PL_KrAudioGetDeviceList(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap) {
	uint count = 0;
	uint index = 0;
	for (PL_AudioDevice *device = g_AudioDeviceListHead.Next; device; device = device->Next) {
		if (device->Flow == flow && (device->IsActive || inactive)) {
			if (index < cap) {
				output[index++] = (KrAudioDeviceInfo){ .Id = device, .Name = device->Name };
			}
			count += 1;
		}
	}

	return output ? index : count;
}

inproc bool PL_KrAudioGetEffectiveDevice(KrAudioDeviceInfo *output) {
	output->Id   = 0;
	output->Name = "Default";

	PL_AudioDevice *device = InterlockedCompareExchangePointer(&g_AudioOut.EffectiveDevice, nullptr, nullptr);
	if (device) {
		output->Id   = device->Id;
		output->Name = device->Name;
	}

	device = InterlockedCompareExchangePointer(&g_AudioOut.DesiredDevice, nullptr, nullptr);
	return device == nullptr;
}

inproc void PL_InitializeAudio() {
	HRESULT hr = CoCreateInstance(&PL_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &PL_IID_IMMDeviceEnumerator, &g_AudioDeviceEnumerator);
	if (FAILED(hr)) {
		Unimplemented();
	}

	g_AudioOut.Events[PL_AudioEvent_Update] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (!g_AudioOut.Events[PL_AudioEvent_Update]) {
		Unimplemented();
	}

	// Use semaphore for rest of the audio events
	for (int i = 0; i < PL_AudioEvent_EnumMax; ++i) {
		if (i == PL_AudioEvent_Update) continue;
		g_AudioOut.Events[i] = CreateSemaphoreW(nullptr, 0, 16, nullptr);
		if (!g_AudioOut.Events[i]) {
			Unimplemented();
		}
	}

	hr = g_AudioDeviceEnumerator->lpVtbl->RegisterEndpointNotificationCallback(g_AudioDeviceEnumerator, &g_NotificationClient);
	if (FAILED(hr)) {
		Unimplemented();
	}

	{
		// Enumerate all the audio devices

		IMMDeviceCollection *device_col = nullptr;
		hr = g_AudioDeviceEnumerator->lpVtbl->EnumAudioEndpoints(g_AudioDeviceEnumerator, eAll, DEVICE_STATEMASK_ALL, &device_col);
		if (FAILED(hr)) {
			Unimplemented();
		}

		UINT count;
		hr = device_col->lpVtbl->GetCount(device_col, &count);
		if (FAILED(hr)) {
			Unimplemented();
		}

		KrAtomicLock(&g_AudioDeviceListLock);

		IMMDevice *immdevice = nullptr;
		for (UINT index = 0; index < count; ++index) {
			hr = device_col->lpVtbl->Item(device_col, index, &immdevice);
			if (FAILED(hr)) {
				Unimplemented();
			}
			PL_UpdateAudioDeviceList(immdevice);
			immdevice->lpVtbl->Release(immdevice);
			immdevice = nullptr;
		}

		device_col->lpVtbl->Release(device_col);

		KrAtomicUnlock(&g_AudioDeviceListLock);
	}

	g_AudioOut.Thread = CreateThread(nullptr, 0, PL_AudioThread, nullptr, 0, 0);
	if (!g_AudioOut.Thread) {
		Unimplemented();
	}

	g_Library.Audio = (KrAudioLibrary) {
		.IsPlaying          = PL_KrAudioIsPlaying,
		.Update             = PL_KrAudioUpdate,
		.Resume             = PL_KrAudioResume,
		.Pause              = PL_KrAudioPause,
		.Reset              = PL_KrAudioReset,
		.SetRenderDevice    = PL_KrAudioSetRenderDevice,
		.GetDeviceList      = PL_KrAudioGetDeviceList,
		.GetEffectiveDevice = PL_KrAudioGetEffectiveDevice
	};
}


//
// Graphics
//

#include <d3d11_1.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>

#pragma comment(lib, "D3D11.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D3DCompiler.lib")

static const UINT BufferCount   = 2;

#if defined(BUILD_DEBUG) || defined(BUILD_DEVELOPER)
static const bool EnableDxDebug = true;
#else
static const bool EnableDxDebug = false;
#endif

static const uint PL_MAX_VERTICES = 4 * 100000;
static const uint PL_MAX_INDICES  = 6 * 100000;

typedef struct PL_Vertex2d {
	float Position[3];
	float TexCoord[2];
	float Color[4];
} PL_Vertex2d;

typedef struct PL_Render2d {
	ID3D11VertexShader      * VertexShader;
	ID3D11PixelShader       * PixelShader;
	ID3D11InputLayout       * InputLayout;
	ID3D11DepthStencilState * DepthStencilState;
	ID3D11RasterizerState1  * RasterizerState;
	ID3D11SamplerState      * SamplerState;
	ID3D11BlendState1       * BlendState;

	ID3D11ShaderResourceView *WhiteTexture;
	ID3D11Buffer *            VertexBuffer;
	ID3D11Buffer *            IndexBuffer;
	ID3D11Buffer *            ConstantBuffer;

	uint                      NextIndex;
	PL_Vertex2d *             MappedVertex;
	uint                      MappedVertexPos;
	u32 *                     MappedIndex;
	uint                      MappedIndexPos;
} PL_Render2d;

static const D3D_FEATURE_LEVEL FeatureLevels[] = {
	D3D_FEATURE_LEVEL_11_1,
	D3D_FEATURE_LEVEL_11_0,
	D3D_FEATURE_LEVEL_10_1,
	D3D_FEATURE_LEVEL_10_0,
	D3D_FEATURE_LEVEL_9_3,
	D3D_FEATURE_LEVEL_9_2,
	D3D_FEATURE_LEVEL_9_1,
};

static IDXGIFactory2          *g_Factory;
static ID3D11Device           *g_Device;
static ID3D11Device1          *g_Device1;
static ID3D11DeviceContext    *g_DeviceContext;
static ID3D11DeviceContext1   *g_DeviceContext1;

static IDXGISwapChain1        *g_MainSwapChain1;
static ID3D11RenderTargetView *g_MainRenderTarget;
static float                   g_MainRenderTargetWidth;
static float                   g_MainRenderTargetHeight;

static PL_Render2d             g_Render2d;

proc void PL_DrawQuad(float p0[2], float p1[2], float p2[2], float p3[2], float color0[4], float color1[4], float color2[4], float color3[4]) {
	if (g_Render2d.MappedVertexPos + 4 < PL_MAX_VERTICES &&
		g_Render2d.MappedIndexPos + 6 < PL_MAX_INDICES) {
		PL_Vertex2d *vtx = g_Render2d.MappedVertex + g_Render2d.MappedVertexPos;
		memcpy(vtx->Position, p0, sizeof(float) * 2);
		vtx->Position[2] = 0;
		vtx->TexCoord[0] = 0; vtx->TexCoord[1] = 0;
		memcpy(vtx->Color, color0, sizeof(float) * 4);

		vtx += 1;
		memcpy(vtx->Position, p1, sizeof(float) * 2);
		vtx->Position[2] = 0;
		vtx->TexCoord[0] = 0; vtx->TexCoord[1] = 0;
		memcpy(vtx->Color, color1, sizeof(float) * 4);

		vtx += 1;
		memcpy(vtx->Position, p2, sizeof(float) * 2);
		vtx->Position[2] = 0;
		vtx->TexCoord[0] = 0; vtx->TexCoord[1] = 0;
		memcpy(vtx->Color, color2, sizeof(float) * 4);

		vtx += 1;
		memcpy(vtx->Position, p3, sizeof(float) * 2);
		vtx->Position[2] = 0;
		vtx->TexCoord[0] = 0; vtx->TexCoord[1] = 0;
		memcpy(vtx->Color, color3, sizeof(float) * 4);
		g_Render2d.MappedVertexPos += 4;

		u32 *idx = g_Render2d.MappedIndex + g_Render2d.MappedIndexPos;
		idx[0] = g_Render2d.NextIndex + 0; idx[1] = g_Render2d.NextIndex + 1; idx[2] = g_Render2d.NextIndex + 2;
		idx[3] = g_Render2d.NextIndex + 0; idx[4] = g_Render2d.NextIndex + 2; idx[5] = g_Render2d.NextIndex + 3;
		g_Render2d.MappedIndexPos += 6;
		g_Render2d.NextIndex += 4;
	}
}

proc void PL_DrawRect(float x, float y, float w, float h, float color0[4], float color1[4]) {
	float p0[] = {x, y};
	float p1[] = {x+w, y};
	float p2[] = {x+w, y+h};
	float p3[] = {x, y+h};
	PL_DrawQuad(p0, p1, p2, p3, color0, color1, color1, color0);
}

inproc void PL_BeginRender2d() {
	HRESULT hr;
	D3D11_MAPPED_SUBRESOURCE mapped;

	hr = g_DeviceContext1->lpVtbl->Map(g_DeviceContext1, (ID3D11Resource *)g_Render2d.VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(hr)) {
		Unimplemented();
	}
	g_Render2d.MappedVertex    = mapped.pData;
	g_Render2d.MappedVertexPos = 0;

	hr = g_DeviceContext1->lpVtbl->Map(g_DeviceContext1, (ID3D11Resource *)g_Render2d.IndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(hr)) {
		Unimplemented();
	}
	g_Render2d.MappedIndex    = mapped.pData;
	g_Render2d.MappedIndexPos = 0;

	g_Render2d.NextIndex = 0;
}

inproc void PL_FlushRender2d(float width, float height) {
	g_DeviceContext1->lpVtbl->Unmap(g_DeviceContext1, (ID3D11Resource *)g_Render2d.VertexBuffer, 0);
	g_DeviceContext1->lpVtbl->Unmap(g_DeviceContext1, (ID3D11Resource *)g_Render2d.IndexBuffer, 0);

	if (!g_Render2d.MappedVertexPos || !g_Render2d.MappedIndexPos) {
		return;
	}

	float proj[4*4] = {
		2.0f / width, 0.0f, 0.0f, -1.0f,
		0.0f, 2.0f / height, 0.0f, -1.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	D3D11_MAPPED_SUBRESOURCE mapped;
	g_DeviceContext1->lpVtbl->Map(g_DeviceContext1, (ID3D11Resource *)g_Render2d.ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	if (mapped.pData) {
		memcpy(mapped.pData, proj, sizeof(proj));
	}

	g_DeviceContext1->lpVtbl->Unmap(g_DeviceContext1, (ID3D11Resource *)g_Render2d.ConstantBuffer, 0);

	UINT strides[] = { sizeof(PL_Vertex2d) };
	UINT offsets[] = { 0 };
	g_DeviceContext1->lpVtbl->IASetVertexBuffers(g_DeviceContext1, 0, 1, &g_Render2d.VertexBuffer, strides, offsets);
	g_DeviceContext1->lpVtbl->IASetIndexBuffer(g_DeviceContext1, g_Render2d.IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	g_DeviceContext1->lpVtbl->VSSetShader(g_DeviceContext1, g_Render2d.VertexShader, nullptr, 0);
	g_DeviceContext1->lpVtbl->PSSetShader(g_DeviceContext1, g_Render2d.PixelShader, nullptr, 0);

	g_DeviceContext1->lpVtbl->VSSetConstantBuffers(g_DeviceContext1, 0, 1, &g_Render2d.ConstantBuffer);
	g_DeviceContext1->lpVtbl->PSSetShaderResources(g_DeviceContext1, 0, 1, (ID3D11ShaderResourceView **)&g_Render2d.WhiteTexture);

	g_DeviceContext1->lpVtbl->RSSetState(g_DeviceContext1, (ID3D11RasterizerState *)g_Render2d.RasterizerState);
	g_DeviceContext1->lpVtbl->IASetInputLayout(g_DeviceContext1, g_Render2d.InputLayout);
	g_DeviceContext1->lpVtbl->PSSetSamplers(g_DeviceContext1, 0, 1, &g_Render2d.SamplerState);
	g_DeviceContext1->lpVtbl->OMSetBlendState(g_DeviceContext1, (ID3D11BlendState *)g_Render2d.BlendState, (float[4]){0.0f}, 0xffffffff);
	g_DeviceContext1->lpVtbl->OMSetDepthStencilState(g_DeviceContext1, g_Render2d.DepthStencilState, 0);
	g_DeviceContext1->lpVtbl->IASetPrimitiveTopology(g_DeviceContext1, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	g_DeviceContext1->lpVtbl->DrawIndexed(g_DeviceContext1, g_Render2d.MappedIndexPos, 0, 0);
}

inproc void PL_InitRender2d() {
	HRESULT hr;

	ID3DBlob *vsblob = nullptr;
	ID3DBlob *psblob = nullptr;

	hr = D3DCompileFromFile(L"Source/Quad.shader", nullptr, nullptr, "VertexMain", "vs_4_0", 0, 0, &vsblob, nullptr);
	if (FAILED(hr)) {
		Unimplemented();
	}

	hr = D3DCompileFromFile(L"Source/Quad.shader", nullptr, nullptr, "PixelMain", "ps_4_0", 0, 0, &psblob, nullptr);
	if (FAILED(hr)) {
		Unimplemented();
	}

	hr = g_Device1->lpVtbl->CreateVertexShader(g_Device1, vsblob->lpVtbl->GetBufferPointer(vsblob),
		vsblob->lpVtbl->GetBufferSize(vsblob), nullptr, &g_Render2d.VertexShader);
	if (FAILED(hr)) {
		Unimplemented();
	}

	hr = g_Device1->lpVtbl->CreatePixelShader(g_Device1, psblob->lpVtbl->GetBufferPointer(psblob),
		psblob->lpVtbl->GetBufferSize(psblob), nullptr, &g_Render2d.PixelShader);
	if (FAILED(hr)) {
		Unimplemented();
	}

	D3D11_INPUT_ELEMENT_DESC input_elements[] = {
		[0] = {
			.SemanticName         = "POSITION",
			.Format               = DXGI_FORMAT_R32G32B32_FLOAT,
			.AlignedByteOffset    = 0,
			.InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA,
		},
		[1] = {
			.SemanticName         = "TEXCOORD",
			.Format               = DXGI_FORMAT_R32G32_FLOAT,
			.AlignedByteOffset    = D3D11_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA,
		},
		[2] = {
			.SemanticName         = "COLOR",
			.Format               = DXGI_FORMAT_R32G32B32A32_FLOAT,
			.AlignedByteOffset    = D3D11_APPEND_ALIGNED_ELEMENT,
			.InputSlotClass       = D3D11_INPUT_PER_VERTEX_DATA,
		}
	};

	hr = g_Device1->lpVtbl->CreateInputLayout(g_Device1, input_elements, ArrayCount(input_elements),
		vsblob->lpVtbl->GetBufferPointer(vsblob), vsblob->lpVtbl->GetBufferSize(vsblob), &g_Render2d.InputLayout);
	if (FAILED(hr)) {
		Unimplemented();
	}

	vsblob->lpVtbl->Release(vsblob);
	psblob->lpVtbl->Release(psblob);

	D3D11_DEPTH_STENCIL_DESC depth_stencil_desc = {
		.DepthEnable    = TRUE,
		.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
		.DepthFunc      = D3D11_COMPARISON_LESS
	};

	hr = g_Device1->lpVtbl->CreateDepthStencilState(g_Device1, &depth_stencil_desc, &g_Render2d.DepthStencilState);
	if (FAILED(hr)) {
		Unimplemented();
	}

	D3D11_RASTERIZER_DESC1 rasterizer_desc = {
		.FillMode      = D3D11_FILL_SOLID,
		.CullMode      = D3D11_CULL_NONE,
		.ScissorEnable = TRUE,
	};

	hr = g_Device1->lpVtbl->CreateRasterizerState1(g_Device1, &rasterizer_desc, &g_Render2d.RasterizerState);
	if (FAILED(hr)) {
		Unimplemented();
	}

	D3D11_SAMPLER_DESC sampler_desc = {
		.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
		.AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
		.AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
		.AddressW = D3D11_TEXTURE_ADDRESS_WRAP
	};

	hr = g_Device1->lpVtbl->CreateSamplerState(g_Device1, &sampler_desc, &g_Render2d.SamplerState);
	if (FAILED(hr)) {
		Unimplemented();
	}

	D3D11_BLEND_DESC1 blend_desc = {
		.RenderTarget[0] = {
			.BlendEnable           = TRUE,
			.SrcBlend              = D3D11_BLEND_SRC_ALPHA,
			.DestBlend             = D3D11_BLEND_INV_SRC_ALPHA,
			.SrcBlendAlpha         = D3D11_BLEND_SRC_ALPHA,
			.DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA,
			.BlendOp               = D3D11_BLEND_OP_ADD,
			.BlendOpAlpha          = D3D11_BLEND_OP_ADD,
			.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
		},
	};

	hr = g_Device1->lpVtbl->CreateBlendState1(g_Device1, &blend_desc, &g_Render2d.BlendState);
	if (FAILED(hr)) {
		Unimplemented();
	}

	D3D11_BUFFER_DESC vertex_buffer = {
		.ByteWidth           = PL_MAX_VERTICES * sizeof(PL_Vertex2d),
		.Usage               = D3D11_USAGE_DYNAMIC,
		.BindFlags           = D3D11_BIND_VERTEX_BUFFER,
		.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE,
	};

	hr = g_Device1->lpVtbl->CreateBuffer(g_Device1, &vertex_buffer, nullptr, &g_Render2d.VertexBuffer);
	if (FAILED(hr)) {
		Unimplemented();
	}

	D3D11_BUFFER_DESC index_buffer = {
		.ByteWidth      = PL_MAX_INDICES * sizeof(u32),
		.Usage          = D3D11_USAGE_DYNAMIC,
		.BindFlags      = D3D11_BIND_INDEX_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
	};

	hr = g_Device1->lpVtbl->CreateBuffer(g_Device1, &index_buffer, nullptr, &g_Render2d.IndexBuffer);
	if (FAILED(hr)) {
		Unimplemented();
	}

	D3D11_BUFFER_DESC constant_buffer = {
		.ByteWidth      = sizeof(float[4 * 4]),
		.Usage          = D3D11_USAGE_DYNAMIC,
		.BindFlags      = D3D11_BIND_CONSTANT_BUFFER,
		.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
	};

	hr = g_Device1->lpVtbl->CreateBuffer(g_Device1, &constant_buffer, nullptr, &g_Render2d.ConstantBuffer);
	if (FAILED(hr)) {
		Unimplemented();
	}

	D3D11_TEXTURE2D_DESC texture_desc = {
		.Width              = 1,
		.Height             = 1,
		.MipLevels          = 0,
		.ArraySize          = 1,
		.SampleDesc.Count   = 1,
		.SampleDesc.Quality = 0,
		.Format             = DXGI_FORMAT_R8G8B8A8_UNORM,
		.Usage              = D3D11_USAGE_IMMUTABLE,
		.BindFlags          = D3D11_BIND_SHADER_RESOURCE,
		.CPUAccessFlags     = 0
	};

	D3D11_SUBRESOURCE_DATA texture_data = {
		.pSysMem          = (uint8_t[4]){0xff, 0xff, 0xff, 0xff},
		.SysMemPitch      = sizeof(u8) * 4,
		.SysMemSlicePitch = 0
	};

	ID3D11Texture2D *texture = nullptr;

	hr = g_Device1->lpVtbl->CreateTexture2D(g_Device1, &texture_desc, &texture_data, &texture);
	if (FAILED(hr)) {
		Unimplemented();
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC shader_res_desc = {
		.Format              = texture_desc.Format,
		.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D,
		.Texture2D.MipLevels = 1
	};

	hr = g_Device1->lpVtbl->CreateShaderResourceView(g_Device1, (ID3D11Resource *)texture, &shader_res_desc, &g_Render2d.WhiteTexture);
	if (FAILED(hr)) {
		Unimplemented();
	}
}

inproc void PL_ResizeRenderTargets(u32 width, u32 height) {
	if (width && height && g_MainSwapChain1) {
		g_DeviceContext1->lpVtbl->Flush(g_DeviceContext1);
		g_DeviceContext1->lpVtbl->ClearState(g_DeviceContext1);

		g_MainRenderTarget->lpVtbl->Release(g_MainRenderTarget);
		g_MainRenderTarget = nullptr;

		g_MainSwapChain1->lpVtbl->ResizeBuffers(g_MainSwapChain1, BufferCount, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

		ID3D11Texture2D *back_buffer = nullptr;
		HRESULT hr = g_MainSwapChain1->lpVtbl->GetBuffer(g_MainSwapChain1, 0, &IID_ID3D11Texture2D, &back_buffer);
		if (FAILED(hr)) {
			Unimplemented();
			return;
		}

		hr = g_Device->lpVtbl->CreateRenderTargetView(g_Device, (ID3D11Resource *)back_buffer, nullptr, &g_MainRenderTarget);
		if (FAILED(hr)) {
			back_buffer->lpVtbl->Release(back_buffer);
			Unimplemented();
			return;
		}

		D3D11_TEXTURE2D_DESC texture_desc;
		back_buffer->lpVtbl->GetDesc(back_buffer, &texture_desc);

		g_MainRenderTargetWidth  = (float)texture_desc.Width;
		g_MainRenderTargetHeight = (float)texture_desc.Height;

		back_buffer->lpVtbl->Release(back_buffer);
	}
}

inproc void PL_ReleaseRenderResources() {
	if (g_MainRenderTarget) {
		g_MainRenderTarget->lpVtbl->Release(g_MainRenderTarget);
		g_MainRenderTarget = nullptr;
	}

	if (g_MainSwapChain1) {
		g_MainSwapChain1->lpVtbl->Release(g_MainSwapChain1);
		g_MainSwapChain1 = nullptr;
	}
}

inproc bool PL_CreateRenderResources() {
	DXGI_SWAP_EFFECT swap_effect =
		IsWindows10OrGreater() ? DXGI_SWAP_EFFECT_FLIP_DISCARD :
		IsWindows8OrGreater() ? DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL :
		DXGI_SWAP_EFFECT_DISCARD;

	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {0};
	swap_chain_desc.Width              = 0;
	swap_chain_desc.Height             = 0;
	swap_chain_desc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.SampleDesc.Count   = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount        = BufferCount;
	swap_chain_desc.SwapEffect         = swap_effect;

	HRESULT hr = g_Factory->lpVtbl->CreateSwapChainForHwnd(g_Factory, (IUnknown *)g_Device,
		g_MainWindow, &swap_chain_desc, nullptr, nullptr, &g_MainSwapChain1);

	Assert(hr != DXGI_ERROR_INVALID_CALL);
	if (FAILED(hr)) {
		PL_ReleaseRenderResources();
		return false;
	}

	ID3D11Texture2D *back_buffer = nullptr;
	hr = g_MainSwapChain1->lpVtbl->GetBuffer(g_MainSwapChain1, 0, &IID_ID3D11Texture2D, &back_buffer);
	if (FAILED(hr)) {
		PL_ReleaseRenderResources();
		return false;
	}

	hr = g_Device->lpVtbl->CreateRenderTargetView(g_Device, (ID3D11Resource *)back_buffer, nullptr, &g_MainRenderTarget);
	if (FAILED(hr)) {
		back_buffer->lpVtbl->Release(back_buffer);
		PL_ReleaseRenderResources();
		return false;
	}

	D3D11_TEXTURE2D_DESC texture_desc;
	back_buffer->lpVtbl->GetDesc(back_buffer, &texture_desc);

	g_MainRenderTargetWidth  = (float)texture_desc.Width;
	g_MainRenderTargetHeight = (float)texture_desc.Height;

	back_buffer->lpVtbl->Release(back_buffer);

	return true;
}

inproc IDXGIAdapter1 *PL_FindAdapter(IDXGIFactory2 *factory, UINT flags) {
	IDXGIAdapter1 *adapter = nullptr;
	size_t         max_dedicated_memory = 0;
	IDXGIAdapter1 *adapter_it = 0;

	uint32_t it_index = 0;

	while (factory->lpVtbl->EnumAdapters1(factory, it_index, &adapter_it) != DXGI_ERROR_NOT_FOUND) {
		it_index += 1;

		DXGI_ADAPTER_DESC1 desc;
		adapter_it->lpVtbl->GetDesc1(adapter_it, &desc);

		HRESULT hr = D3D11CreateDevice((IDXGIAdapter *)adapter_it, D3D_DRIVER_TYPE_UNKNOWN, 0,
			flags, FeatureLevels, ArrayCount(FeatureLevels),
			D3D11_SDK_VERSION, NULL, NULL, NULL);

		if (SUCCEEDED(hr) && desc.DedicatedVideoMemory > max_dedicated_memory) {
			max_dedicated_memory = desc.DedicatedVideoMemory;
			adapter = adapter_it;
		} else {
			adapter_it->lpVtbl->Release(adapter_it);
		}
	}

	return adapter;
}

inproc void PL_UninitializeGraphics() {
	PL_ReleaseRenderResources();

	if (g_DeviceContext1) {
		g_DeviceContext1->lpVtbl->Release(g_DeviceContext1);
		g_DeviceContext1 = nullptr;
	}
	if (g_DeviceContext) {
		g_DeviceContext->lpVtbl->Release(g_DeviceContext);
		g_DeviceContext = nullptr;
	}
	if (g_Device1) {
		g_Device1->lpVtbl->Release(g_Device1);
		g_Device1 = nullptr;
	}
	if (g_Device) {
		g_Device->lpVtbl->Release(g_Device);
		g_Device = nullptr;
	}
	if (g_Factory) {
		g_Factory->lpVtbl->Release(g_Factory);
		g_Factory = nullptr;
	}
}

inproc void PL_InitializeGraphics() {
	UINT flags = 0;
	if (EnableDxDebug) {
		flags |= DXGI_CREATE_FACTORY_DEBUG;
	}

	HRESULT hr = CreateDXGIFactory2(flags, &IID_IDXGIFactory2, &g_Factory);
	if (FAILED(hr)) {
		Unimplemented();
		return;
	}

	IDXGIAdapter1 *adapter = PL_FindAdapter(g_Factory, flags);

	if (adapter == nullptr) {
		PL_UninitializeGraphics();
		return;
	}

	hr = D3D11CreateDevice((IDXGIAdapter *)adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, EnableDxDebug ? D3D11_CREATE_DEVICE_DEBUG : 0,
		FeatureLevels, ArrayCount(FeatureLevels), D3D11_SDK_VERSION, &g_Device, nullptr, &g_DeviceContext);
	Assert(hr != E_INVALIDARG);

	if (FAILED(hr)) {
		goto failed;
	}

	hr = g_Device->lpVtbl->QueryInterface(g_Device, &IID_ID3D11Device1, &g_Device1);
	if (FAILED(hr)) {
		Unimplemented();
	}

	if (g_Device1)
		g_Device1->lpVtbl->GetImmediateContext1(g_Device1, &g_DeviceContext1);

	if (EnableDxDebug) {
		ID3D11Debug *debug = 0;

		if (SUCCEEDED(g_Device1->lpVtbl->QueryInterface(g_Device1, &IID_ID3D11Debug, &debug))) {
			ID3D11InfoQueue *info_queue = nullptr;
			if (SUCCEEDED(g_Device1->lpVtbl->QueryInterface(g_Device1, &IID_ID3D11InfoQueue, &info_queue))) {
				info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
				info_queue->lpVtbl->SetBreakOnSeverity(info_queue, D3D11_MESSAGE_SEVERITY_ERROR, true);

				D3D11_MESSAGE_ID hide[] = { D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS };

				D3D11_INFO_QUEUE_FILTER filter = {{0}};
				filter.DenyList.NumIDs = ArrayCount(hide);
				filter.DenyList.pIDList = hide;
				info_queue->lpVtbl->AddStorageFilterEntries(info_queue, &filter);

				info_queue->lpVtbl->Release(info_queue);
			}
		}
		debug->lpVtbl->Release(debug);
	}

	adapter->lpVtbl->Release(adapter);

	if (!PL_CreateRenderResources()) {
		PL_UninitializeGraphics();
	}

	PL_InitRender2d();

	return;

failed:
	adapter->lpVtbl->Release(adapter);
	PL_UninitializeGraphics();
}

//
// Message Loop
//

inproc int PL_MessageLoop() {
	KrEvent event;
	event.Kind = KrEvent_Startup;
	g_UserContext.OnEvent(&event, g_UserContext.Data);

	bool running = true;

	while (running) {
		MSG msg;
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				running = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		g_DeviceContext1->lpVtbl->ClearRenderTargetView(g_DeviceContext1, g_MainRenderTarget, (float[4]){0.1f, 0.1f, 0.1f, 1.0f});

		PL_BeginRender2d();

		g_UserContext.OnUpdate(g_MainRenderTargetWidth, g_MainRenderTargetHeight, g_UserContext.Data);

		D3D11_VIEWPORT viewport = {
			.TopLeftX = 0,
			.TopLeftY = 0,
			.Width    = g_MainRenderTargetWidth,
			.Height   = g_MainRenderTargetHeight,
			.MinDepth = 0,
			.MaxDepth = 1
		};

		D3D11_RECT rect = {
			.left   = 0,
			.right  = (LONG)g_MainRenderTargetWidth,
			.top    = 0,
			.bottom = (LONG)g_MainRenderTargetHeight
		};

		g_DeviceContext1->lpVtbl->OMSetRenderTargets(g_DeviceContext1, 1, &g_MainRenderTarget, nullptr);
		g_DeviceContext1->lpVtbl->RSSetViewports(g_DeviceContext1, 1, &viewport);
		g_DeviceContext1->lpVtbl->RSSetScissorRects(g_DeviceContext1, 1, &rect);

		PL_FlushRender2d(g_MainRenderTargetWidth, g_MainRenderTargetHeight);

		DXGI_PRESENT_PARAMETERS present_params = {
			.DirtyRectsCount = 0
		};
		g_MainSwapChain1->lpVtbl->Present1(g_MainSwapChain1, 1, 0, &present_params);
	}

	event.Kind = KrEvent_Quit;
	g_UserContext.OnEvent(&event, g_UserContext.Data);
	
	return 0;
}

//
// Main
//

inproc char **PL_CommandLineArguments(int *argc) {
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

inproc int PL_Main() {
	if (IsWindows10OrGreater()) {
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
	} else {
		SetProcessDPIAware();
	}

#if defined(BUILD_DEBUG) || defined(BUILD_DEVELOPER)
	AllocConsole();
#endif

	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	Assert(hr != RPC_E_CHANGED_MODE);

	if (hr != S_OK && hr != S_FALSE) {
		Unimplemented(); // Handle error
	}

	int argc    = 0;
	char **argv = PL_CommandLineArguments(&argc);

	int res = Main(argc, argv, &g_UserContext);

	if (res == 0) {
		PL_InitializeWindow();
		PL_InitializeGraphics();
		PL_InitializeAudio();

		res = PL_MessageLoop();

		PL_UninitializeAudio();
		PL_UninitializeGraphics();
		PL_UninitializeWindow();
	}

	CoUninitialize();

	return res;
}

#if defined(BUILD_DEBUG) || defined(BUILD_DEVELOPER) || defined(BUILD_TEST)
#pragma comment( linker, "/subsystem:console" )
#else
#pragma comment( linker, "/subsystem:windows" )
#endif

// SUBSYSTEM:CONSOLE
int wmain() { return PL_Main(); }

// SUBSYSTEM:WINDOWS
int __stdcall wWinMain(HINSTANCE instance, HINSTANCE prev_instance, LPWSTR cmd, int n) { return PL_Main(); }
