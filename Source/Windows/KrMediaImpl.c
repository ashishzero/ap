#include "../KrMediaImpl.h"

#pragma warning(push)
#pragma warning(disable: 5105)

#include <windows.h>
#include <windowsx.h>
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
				event.Kind = KrEventKind_WindowActivated;
			else
				event.Kind = KrEventKind_WindowDeactivated;
			g_UserContext.OnEvent(&event, g_UserContext.Data);

			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;

		case WM_CREATE:
		{
			res = DefWindowProcW(wnd, msg, wparam, lparam);

			event.Kind = KrEventKind_WindowCreated;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_DESTROY:
		{
			event.Kind = KrEventKind_WindowDestroyed;
			g_UserContext.OnEvent(&event, g_UserContext.Data);

			res = DefWindowProcW(wnd, msg, wparam, lparam);
			PostQuitMessage(0);
		} break;

		case WM_SIZE:
		{
			int x = LOWORD(lparam);
			int y = HIWORD(lparam);

			event.Kind = KrEventKind_WindowResized;
			event.Window.Width  = x;
			event.Window.Height = y;
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_MOUSEMOVE: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEventKind_MouseMoved;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_LBUTTONUP:
		case WM_LBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = msg == WM_LBUTTONDOWN ? KrEventKind_ButtonPressed : KrEventKind_ButtonReleased;
			event.Mouse.Button = KrButton_Left;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_RBUTTONUP:
		case WM_RBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = msg == WM_LBUTTONDOWN ? KrEventKind_ButtonPressed : KrEventKind_ButtonReleased;
			event.Mouse.Button = KrButton_Right;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_MBUTTONUP:
		case WM_MBUTTONDOWN: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = msg == WM_LBUTTONDOWN ? KrEventKind_ButtonPressed : KrEventKind_ButtonReleased;
			event.Mouse.Button = KrButton_Middle;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_LBUTTONDBLCLK: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEventKind_DoubleClicked;
			event.Mouse.Button = KrButton_Left;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_RBUTTONDBLCLK: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEventKind_DoubleClicked;
			event.Mouse.Button = KrButton_Right;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_MBUTTONDBLCLK: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEventKind_DoubleClicked;
			event.Mouse.Button = KrButton_Middle;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Mouse.X, &event.Mouse.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_MOUSEWHEEL: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEventKind_WheelMoved;
			event.Wheel.Vert = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
			event.Wheel.Horz = 0.0f;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Wheel.X, &event.Wheel.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_MOUSEHWHEEL: {
			int x = GET_X_LPARAM(lparam);
			int y = GET_Y_LPARAM(lparam);

			event.Kind = KrEventKind_WheelMoved;
			event.Wheel.Vert = 0.0f;
			event.Wheel.Horz = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
			PL_NormalizeCursorPosition(wnd, x, y, &event.Wheel.X, &event.Wheel.Y);
			g_UserContext.OnEvent(&event, g_UserContext.Data);
		} break;

		case WM_KEYUP:
		case WM_KEYDOWN: {
			if (wparam < ArrayCount(VirtualKeyMap)) {
				event.Kind = msg == WM_KEYDOWN ? KrEventKind_KeyPressed : KrEventKind_KeyReleased;
				event.Key.Code   = VirtualKeyMap[wparam];
				event.Key.Repeat = HIWORD(lparam) & KF_REPEAT;
				g_UserContext.OnEvent(&event, g_UserContext.Data);
			}
		} break;

		case WM_SYSKEYUP:
		case WM_SYSKEYDOWN: {
			if (wparam == VK_F10) {
				event.Kind = msg == WM_KEYDOWN ? KrEventKind_KeyPressed : KrEventKind_KeyReleased;
				event.Key.Code = KrKey_F10;
				event.Key.Repeat = HIWORD(lparam) & KF_REPEAT;
				g_UserContext.OnEvent(&event, g_UserContext.Data);
			}
		} break;

		case WM_CHAR: {
			event.Kind = KrEventKind_TextInput;
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

		default:
		{
			res = DefWindowProcW(wnd, msg, wparam, lparam);
		} break;
	}

	return res;
}

inproc void PL_ReleaseWindow() {
	if (g_MainWindow) {
		DestroyWindow(g_MainWindow);
		g_MainWindow = nullptr;
	}
}

inproc void PL_PrepareWindow() {
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
}

inproc void PL_ToggleFullscreen() {
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
	volatile u32           Active;
	LPWSTR                 Id;
	PL_KrDeviceName        Names[2];
} PL_AudioDevice;

typedef enum PL_AudioEvent {
	PL_AudioEvent_Update,
	PL_AudioEvent_Resume,
	PL_AudioEvent_Pause,
	PL_AudioEvent_Reset,
	PL_AudioEvent_DeviceLost,
	PL_AudioEvent_DeviceRestart,
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
static PL_AudioDevice        g_AudioDeviceListHead = { .Name = "", .Flow = KrAudioDeviceFlow_EnumMax };
static volatile LONG         g_AudioDeviceListLock;

static struct {
	IAudioClient         *Client;
	IAudioRenderClient   *RenderClient;
	KrAudioSpec           Spec;
	UINT                  MaxFrames;
	HANDLE                Events[PL_AudioEvent_EnumMax];
	volatile LONG         DeviceIsLost;
	volatile LONG         DeviceIsResumed;
	PL_AudioDevice *      EffectiveAudioDevice;
	PL_AudioDevice *      DesiredAudioDevice;
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

inproc u32 PL_IMMDeviceActive(IMMDevice *immdevice) {
	u32 flags   = 0;
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

	device->Id       = id;
	device->Names[0] = PL_IMMDeviceName(immdevice);
	device->Flow     = PL_IMMDeviceFlow(immdevice);
	device->Active   = PL_IMMDeviceActive(immdevice);
	device->Name     = device->Names[0].Buffer;

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

inproc PL_AudioDevice *PL_FindAudioDeviceFromId(LPCWSTR id) {
	PL_AudioDevice *device = (PL_AudioDevice *)g_AudioDeviceListHead.Next;
	for (;device; device = (PL_AudioDevice *)device->Next) {
		if (wcscmp(id, device->Id) == 0)
			return device;
	}
	return nullptr;
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

	if (device) {
		InterlockedExchange(&device->Active, active);
	} else {
		// New device added
		IMMDevice *immdevice = PL_IMMDeviceFromId(deviceid);
		if (immdevice) {
			device = PL_UpdateAudioDeviceList(immdevice);
			immdevice->lpVtbl->Release(immdevice);
		}
	}

	if (device == InterlockedCompareExchangePointer(&g_AudioOut.DesiredAudioDevice, nullptr, nullptr)) {
		if (active) {
			ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_DeviceRestart], 1, 0);
		} else {
			ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_DeviceLost], 1, 0);
		}
	}

	KrAtomicUnlock(&g_AudioDeviceListLock);
	return S_OK;
}

// These events is handled in "OnDeviceStateChanged"
inproc HRESULT STDMETHODCALLTYPE PL_OnDeviceAdded(IMMNotificationClient *This, LPCWSTR deviceid)   { return S_OK; }
inproc HRESULT STDMETHODCALLTYPE PL_OnDeviceRemoved(IMMNotificationClient *This, LPCWSTR deviceid) { return S_OK; }

inproc HRESULT STDMETHODCALLTYPE PL_OnDefaultDeviceChanged(IMMNotificationClient *This, EDataFlow flow, ERole role, LPCWSTR deviceid) {
	KrAtomicLock(&g_AudioDeviceListLock);

	if (flow == eRender && role == eMultimedia) {
		if (InterlockedCompareExchangePointer(&g_AudioOut.DesiredAudioDevice, nullptr, nullptr) == nullptr) {
			ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_DeviceRestart], 1, 0);
		}
	}

	KrAtomicUnlock(&g_AudioDeviceListLock);
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
}

inproc bool PL_PrepareAudioRenderDevice(const PL_AudioDevice *device) {
	WAVEFORMATEX *wave_format = nullptr;

	IMMDevice *immdevice = device ? PL_IMMDeviceFromId(device->Id) : PL_GetDefaultIMMDevice(eRender);
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

	InterlockedExchangePointer(&g_AudioOut.EffectiveAudioDevice, (PL_AudioDevice *)device);
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
	BYTE *data = nullptr;
	u32 frames = 0;
	if (PL_GetAudioBuffer(&data, &frames)) {
		u32 written = g_UserContext.OnUploadAudio(&g_AudioOut.Spec, data, frames, g_UserContext.Data);
		PL_ReleaseAudioBuffer(written, frames);
	}
}

inproc void PL_ResumeAudio() {
	InterlockedExchange(&g_AudioOut.DeviceIsResumed, 1);

	HRESULT hr = g_AudioOut.Client->lpVtbl->Start(g_AudioOut.Client);
	if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED)
		return;
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
	Unimplemented();
}

inproc void PL_PauseAudio() {
	InterlockedExchange(&g_AudioOut.DeviceIsResumed, 0);

	HRESULT hr = g_AudioOut.Client->lpVtbl->Stop(g_AudioOut.Client);
	if (SUCCEEDED(hr))
		return;
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
	Unimplemented();
}

inproc void PL_ResetAudio() {
	PL_PauseAudio();
	HRESULT hr = g_AudioOut.Client->lpVtbl->Reset(g_AudioOut.Client);
	if (SUCCEEDED(hr))
		return;
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
}

inproc DWORD WINAPI PL_AudioThread(LPVOID param) {
	HANDLE thread = GetCurrentThread();
	SetThreadDescription(thread, L"PL Audio Thread");

	DWORD task_index;
	HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	if (PL_PrepareAudioRenderDevice(g_AudioOut.DesiredAudioDevice)) {
		// todo: event render device gained
	}

	while (1) {
		DWORD wait = WaitForMultipleObjects(PL_AudioEvent_EnumMax, g_AudioOut.Events, FALSE, INFINITE);

		HRESULT hr = S_OK;

		// todo: events
		if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Update) {
			PL_UpdateAudio();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Resume) {
			PL_ResumeAudio();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Pause) {
			PL_PauseAudio();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Reset) {
			PL_ResetAudio();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_DeviceLost) {
			PL_ReleaseAudioRenderDevice();
			if (PL_PrepareAudioRenderDevice(nullptr)) {
				// todo: swapping event
				PL_UpdateAudio();
				if (g_AudioOut.DeviceIsResumed) {
					PL_ResumeAudio();
				}
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_DeviceRestart) {
			PL_ReleaseAudioRenderDevice();
			if (PL_PrepareAudioRenderDevice(g_AudioOut.DesiredAudioDevice)) {
				// todo: swapping event
				PL_UpdateAudio();
				if (g_AudioOut.DeviceIsResumed) {
					PL_ResumeAudio();
				}
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Exit) {
			break;
		}

		// todo: check for default device changes, device lost and device gained and events
	}

	PL_ReleaseAudioRenderDevice();

	AvRevertMmThreadCharacteristics(avrt);

	return 0;
}

inproc void PL_ReleaseAudio() {
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
	PL_AudioDevice *prev = InterlockedExchangePointer(&g_AudioOut.DesiredAudioDevice, device);
	if (prev != device) {
		ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_DeviceRestart], 1, 0);
	}
	return true;
}

inproc uint PL_KrAudioGetDevices(KrAudioDeviceFlow flow, bool inactive, KrAudioDeviceInfo *output, uint cap) {
	uint count = 0;
	uint index = 0;
	for (PL_AudioDevice *device = g_AudioDeviceListHead.Next; device; device = device->Next) {
		if (device->Flow == flow && device->Active || inactive) {
			if (index < cap) {
				output[index++] = (KrAudioDeviceInfo){ .Id = device, .Name = device->Name };
			}
			count += 1;
		}
	}
	return output ? index : count;
}

inproc void PL_PrepareAudio() {
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

	g_Library.Audio      = (KrAudioLibrary) {
		.IsPlaying       = PL_KrAudioIsPlaying,
		.Update          = PL_KrAudioUpdate,
		.Resume          = PL_KrAudioResume,
		.Pause           = PL_KrAudioPause,
		.Reset           = PL_KrAudioReset,
		.SetRenderDevice = PL_KrAudioSetRenderDevice,
		.GetDevices      = PL_KrAudioGetDevices
	};
}

//
// Message Loop
//

inproc int PL_MessageLoop() {
	KrEvent event;
	event.Kind = KrEventKind_Startup;
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

		g_UserContext.OnUpdate(g_UserContext.Data);
	}

	event.Kind = KrEventKind_Quit;
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
		PL_PrepareWindow();
		PL_PrepareAudio();

		res = PL_MessageLoop();

		PL_ReleaseAudio();
		PL_ReleaseWindow();
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
