#include "KrWindowsCore.h"

#pragma warning(push)
#pragma warning(disable: 5105)

#include <windowsx.h>
#include <VersionHelpers.h>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Avrt.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Shell32.lib")

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

int PL_UTF16ToUTF8(char *utf8_buff, int utf8_buff_len, const char16_t *utf16_string) {
	int bytes_written = WideCharToMultiByte(CP_UTF8, 0, utf16_string, -1, utf8_buff, utf8_buff_len, 0, 0);
	return bytes_written;
}

//
// Error
//

static thread_local char LastErrorBuff[4096];

const char *PL_GetLastError(void) {
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

void PL_InitThreadContext() {
	Thread.Logger.Handle = PL_LogEx;
	Thread.OnAssertion   = PL_HandleAssertion;
	Thread.OnFatalError  = PL_FatalError;
}

//
// Main
//

static void PL_ActivateAudioDevice(PL_AudioDeviceNative *native) {
	PL_AudioDeviceList *devices = native->IsCapture == 0 ? &Media.IoDevice.AudioRenderDevices : &Media.IoDevice.AudioCaptureDevices;

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
	PL_AudioDeviceList *devices = native->IsCapture == 0 ? &Media.IoDevice.AudioRenderDevices : &Media.IoDevice.AudioCaptureDevices;

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
	PL_AudioDeviceList *devices = endpoint == PL_AudioEndpoint_Render ? &Media.IoDevice.AudioRenderDevices : &Media.IoDevice.AudioCaptureDevices;
	devices->Default = false;
	devices->Current = 0;
}

static void PL_SetCurrentAudioDevice(PL_AudioDeviceNative *effective, PL_AudioDeviceNative *desired) {
	PL_AudioDeviceList *devices = effective->IsCapture == 0 ? &Media.IoDevice.AudioRenderDevices : &Media.IoDevice.AudioCaptureDevices;

	for (int i = 0; i < devices->Count; ++i) {
		if (devices->Data[i].Handle == effective) {
			devices->Current = &devices->Data[i];
			break;
		}
	}

	devices->Default = desired == nullptr;
}

static void HandleThreadMessage(MSG *msg) {
	PL_Event event;
	switch (msg->message) {
		case PL_WM_AUDIO_DEVICE_ACTIVATED: {
			PL_AudioDeviceNative *native  = (PL_AudioDeviceNative *)msg->wParam;
			PL_AudioEndpointKind endpoint = native->IsCapture ? PL_AudioEndpoint_Capture : PL_AudioEndpoint_Render;
			event.Kind                    = PL_Event_AudioDeviceActived;
			event.Audio.Endpoint          = endpoint;
			event.Audio.NativeHandle      = native;
			event.Audio.Name              = native->Name;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			PL_ActivateAudioDevice(native);
		} break;

		case PL_WM_AUDIO_DEVICE_DEACTIVATED: {
			PL_AudioDeviceNative *native  = (PL_AudioDeviceNative *)msg->wParam;
			PL_AudioEndpointKind endpoint = native->IsCapture ? PL_AudioEndpoint_Capture : PL_AudioEndpoint_Render;
			event.Kind                    = PL_Event_AudioDeviceDeactived;
			event.Audio.Endpoint          = endpoint;
			event.Audio.NativeHandle      = native;
			event.Audio.Name              = native->Name;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			PL_DeactivateAudioDevice(native);
		} break;

		case PL_WM_AUDIO_PAUSED: {
			event.Kind               = PL_Event_AudioPaused;
			event.Audio.Endpoint     = (PL_AudioEndpointKind)msg->wParam;
			event.Audio.NativeHandle = 0;
			event.Audio.Name         = nullptr;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case PL_WM_AUDIO_RESUMED: {
			event.Kind               = PL_Event_AudioResumed;
			event.Audio.Endpoint     = (PL_AudioEndpointKind)msg->wParam;
			event.Audio.NativeHandle = 0;
			event.Audio.Name         = nullptr;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
		} break;

		case PL_WM_AUDIO_DEVICE_LOST: {
			event.Kind               = PL_Event_AudioDeviceLost;
			event.Audio.Endpoint     = (PL_AudioEndpointKind)msg->wParam;
			event.Audio.NativeHandle = 0;
			event.Audio.Name         = nullptr;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			PL_UnsetCurrentAudioDevice(event.Audio.Endpoint);
		} break;

		case PL_WM_AUDIO_DEVICE_CHANGED: {
			PL_AudioDeviceNative *effective = (PL_AudioDeviceNative *)msg->wParam;
			PL_AudioDeviceNative *desired   = (PL_AudioDeviceNative *)msg->lParam;
			PL_AudioEndpointKind endpoint   = effective->IsCapture ? PL_AudioEndpoint_Capture : PL_AudioEndpoint_Render;
			event.Kind                      = PL_Event_AudioDeviceChanged;
			event.Audio.Endpoint            = endpoint;
			event.Audio.NativeHandle        = effective;
			event.Audio.Name                = effective->Name;
			Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);
			PL_SetCurrentAudioDevice(effective, desired);
		} break;
	}
}

static int PL_MessageLoop(void) {
	PL_Event event;

	event = (PL_Event){ .Kind = PL_Event_Startup };
	Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);

	MSG msg;
	while (1) {
		for (int i = 0; i < PL_Key_EnumCount; ++i) {
			PL_KeyState *key = &Media.IoDevice.Keyboard.Keys[i];
			key->Pressed     = 0;
			key->Released    = 0;
			key->Transitions = 0;
		}

		for (int i = 0; i < PL_Button_EnumCount; ++i) {
			PL_MouseButtonState *button = &Media.IoDevice.Mouse.Buttons[i];
			button->Pressed             = 0;
			button->Released            = 0;
			button->Transitions         = 0;
			button->DoubleClicked       = 0;
		}

		Media.IoDevice.Mouse.DeltaCursor = (PL_Axis){ 0, 0 };
		Media.IoDevice.Mouse.Wheel       = (PL_Axis){ 0, 0 };

		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				return 0;
			}
			HandleThreadMessage(&msg);
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}

		Media.UserVTable.OnUpdate(&Media.IoDevice, Media.UserVTable.Data);
	}

	event = (PL_Event){ .Kind = PL_Event_Quit };
	Media.UserVTable.OnEvent(&event, Media.UserVTable.Data);

	return 0;
}

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

// TODO: cleanup
extern void PL_Media_Load();
extern void PL_Media_Release();

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
		res = PL_MessageLoop();
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
