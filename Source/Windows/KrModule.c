#include "../KrModule.h"

#pragma warning(push)
#pragma warning(disable: 5105)
#define COBJMACROS

#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>

#pragma warning(pop)

DEFINE_GUID(KR_CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(KR_IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(KR_IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(KR_IID_IAudioClient, 0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(KR_IID_IAudioRenderClient, 0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);
DEFINE_GUID(KR_IID_IMMNotificationClient, 0x7991eec9, 0x7e89, 0x4d85, 0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0);
DEFINE_GUID(KR_KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(KR_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(KR_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX, 0x00000000L, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Avrt.lib")


//
//
//

static __declspec(thread) char16_t ErrBuffer[16384];

const char16_t *KrGetLastError() {
	DWORD err = GetLastError();
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			ErrBuffer, ArrayCount(ErrBuffer), NULL);
	return ErrBuffer;
}

static void KrInternalError() {
	const char16_t *err = KrGetLastError();
	MessageBoxW(nullptr, err, L"Internal Fatal Error", MB_ICONERROR | MB_OK);
	TriggerBreakpoint();
	ExitProcess(1);
}

//
// Audio
//

static struct {
	volatile LONG        Initialized;
	IMMDeviceEnumerator *DeviceEnumerator;
} Audio;

void KrInitAudioInstance(void) {
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (hr != S_OK && hr != S_FALSE && hr != RPC_E_CHANGED_MODE) {
		FatalAppExitW(0, L"Failed to initialize audio instance");
	}

	if (InterlockedCompareExchange(&Audio.Initialized, 1, 0) == 1) {
		IMMDeviceEnumerator_AddRef(Audio.DeviceEnumerator);
		return;
	}

	hr = CoCreateInstance(&KR_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &KR_IID_IMMDeviceEnumerator, &Audio.DeviceEnumerator);
	if (FAILED(hr)) {
		FatalAppExitW(0, L"Failed to initialize audio instance");
	}
}

void KrDeinitAudioInstance(void) {
	if (IMMDeviceEnumerator_Release(Audio.DeviceEnumerator) == 0) {
		InterlockedExchange(&Audio.Initialized, 0);
		CoUninitialize();
	}
}

static void KrAudioSpecToWaveFormat(WAVEFORMATEXTENSIBLE *dst, const KrAudioSpec *src) {
	memset(dst, 0, sizeof(*dst));

	dst->Format.nSamplesPerSec = src->SamplesPerSecond;

	if (src->Channels == 2) {
		if (src->Format == KrAudioFormat_R32) {
			dst->Format.wFormatTag     = WAVE_FORMAT_IEEE_FLOAT;
			dst->Format.wBitsPerSample = 32;
		} else if (src->Format == KrAudioFormat_I32) {
			dst->Format.wFormatTag     = WAVE_FORMAT_PCM;
			dst->Format.wBitsPerSample = 32;
		} else if (src->Format == KrAudioFormat_I16) {
			dst->Format.wFormatTag     = WAVE_FORMAT_PCM;
			dst->Format.wBitsPerSample = 16;
		}
		dst->Format.nChannels = 2;
		dst->Format.cbSize    = sizeof(WAVEFORMATEX);
	} else {
		dst->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		if (src->Format == KrAudioFormat_R32) {
			memcpy(&dst->SubFormat, &KR_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID));
			dst->Format.wBitsPerSample = 32;
		} else if (src->Format == KrAudioFormat_I32) {
			memcpy(&dst->SubFormat, &KR_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID));
			dst->Format.wBitsPerSample = 32;
		} else if (src->Format == KrAudioFormat_I16) {
			memcpy(&dst->SubFormat, &KR_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID));
			dst->Format.wBitsPerSample = 16;
		}
		dst->Format.nChannels = src->Channels;

		if (src->ChannelMask & KrAudioChannel_FrontLeft)          dst->dwChannelMask |=  SPEAKER_FRONT_LEFT;
		if (src->ChannelMask & KrAudioChannel_FrontRight)         dst->dwChannelMask |=  SPEAKER_FRONT_RIGHT;
		if (src->ChannelMask & KrAudioChannel_FrontCenter)        dst->dwChannelMask |=  SPEAKER_FRONT_CENTER;
		if (src->ChannelMask & KrAudioChannel_LowFrequency)       dst->dwChannelMask |=  SPEAKER_LOW_FREQUENCY;
		if (src->ChannelMask & KrAudioChannel_BackLeft)           dst->dwChannelMask |=  SPEAKER_BACK_LEFT;
		if (src->ChannelMask & KrAudioChannel_BackRight)          dst->dwChannelMask |=  SPEAKER_BACK_RIGHT;
		if (src->ChannelMask & KrAudioChannel_FrontLeftOfCenter)  dst->dwChannelMask |=  SPEAKER_FRONT_LEFT_OF_CENTER;
		if (src->ChannelMask & KrAudioChannel_FrontRightOfCenter) dst->dwChannelMask |= SPEAKER_FRONT_RIGHT_OF_CENTER;
		if (src->ChannelMask & KrAudioChannel_BackCenter)         dst->dwChannelMask |=  SPEAKER_BACK_CENTER;
		if (src->ChannelMask & KrAudioChannel_SideLeft)           dst->dwChannelMask |=  SPEAKER_SIDE_LEFT;
		if (src->ChannelMask & KrAudioChannel_SideRight)          dst->dwChannelMask |=  SPEAKER_SIDE_RIGHT;
		if (src->ChannelMask & KrAudioChannel_TopCenter)          dst->dwChannelMask |=  SPEAKER_TOP_CENTER;
		if (src->ChannelMask & KrAudioChannel_TopFrontLeft)       dst->dwChannelMask |=  SPEAKER_TOP_FRONT_LEFT;
		if (src->ChannelMask & KrAudioChannel_TopFrontCenter)     dst->dwChannelMask |=  SPEAKER_TOP_FRONT_CENTER;
		if (src->ChannelMask & KrAudioChannel_TopFrontRight)      dst->dwChannelMask |=  SPEAKER_TOP_FRONT_RIGHT;
		if (src->ChannelMask & KrAudioChannel_TopBackLeft)        dst->dwChannelMask |=  SPEAKER_TOP_BACK_LEFT;
		if (src->ChannelMask & KrAudioChannel_TopBackCenter)      dst->dwChannelMask |=  SPEAKER_TOP_BACK_CENTER;
		if (src->ChannelMask & KrAudioChannel_TopBackRight)       dst->dwChannelMask |=  SPEAKER_TOP_BACK_RIGHT;

		dst->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE);
	}

	dst->Format.nBlockAlign     = (dst->Format.nChannels * dst->Format.wBitsPerSample) / 8;
	dst->Format.nAvgBytesPerSec = dst->Format.nSamplesPerSec * dst->Format.nBlockAlign;
}

static void KrWaveFormatToAudioSpec(KrAudioSpec *dst, const WAVEFORMATEX *src) {
	const WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)src;

	if (src->wFormatTag == WAVE_FORMAT_IEEE_FLOAT && src->wBitsPerSample == 32) {
		dst->Format = KrAudioFormat_R32;
	} else if (src->wFormatTag == WAVE_FORMAT_PCM && src->wBitsPerSample == 32) {
		dst->Format = KrAudioFormat_I32;
	} else if (src->wFormatTag == WAVE_FORMAT_PCM && src->wBitsPerSample == 16) {
		dst->Format = KrAudioFormat_I16;
	} else if (src->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		if ((memcmp(&ext->SubFormat, &KR_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0) && (src->wBitsPerSample == 32)) {
			dst->Format = KrAudioFormat_R32;
		} else if ((memcmp(&ext->SubFormat, &KR_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) && (src->wBitsPerSample == 32)) {
			dst->Format = KrAudioFormat_I32;
		} else if ((memcmp(&ext->SubFormat, &KR_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) && (src->wBitsPerSample == 16)) {
			dst->Format = KrAudioFormat_I16;
		} else {
			FatalAppExitW(0, L"Fatal Error: Unsupported WAVE Format");
		}
	} else {
		FatalAppExitW(0, L"Fatal Error: Unsupported WAVE Format");
	}

	dst->ChannelMask  = 0;

	if (src->nChannels == 2) {
		dst->ChannelMask |= KrAudioChannel_FrontLeft;
		dst->ChannelMask |= KrAudioChannel_FrontRight;
	} else if (src->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		if (ext->dwChannelMask & SPEAKER_FRONT_LEFT)            dst->ChannelMask |= KrAudioChannel_FrontLeft;
		if (ext->dwChannelMask & SPEAKER_FRONT_RIGHT)           dst->ChannelMask |= KrAudioChannel_FrontRight;
		if (ext->dwChannelMask & SPEAKER_FRONT_CENTER)          dst->ChannelMask |= KrAudioChannel_FrontCenter;
		if (ext->dwChannelMask & SPEAKER_LOW_FREQUENCY)         dst->ChannelMask |= KrAudioChannel_LowFrequency;
		if (ext->dwChannelMask & SPEAKER_BACK_LEFT)             dst->ChannelMask |= KrAudioChannel_BackLeft;
		if (ext->dwChannelMask & SPEAKER_BACK_RIGHT)            dst->ChannelMask |= KrAudioChannel_BackRight;
		if (ext->dwChannelMask & SPEAKER_FRONT_LEFT_OF_CENTER)  dst->ChannelMask |= KrAudioChannel_FrontLeftOfCenter;
		if (ext->dwChannelMask & SPEAKER_FRONT_RIGHT_OF_CENTER) dst->ChannelMask |= KrAudioChannel_FrontRightOfCenter;
		if (ext->dwChannelMask & SPEAKER_BACK_CENTER)           dst->ChannelMask |= KrAudioChannel_BackCenter;
		if (ext->dwChannelMask & SPEAKER_SIDE_LEFT)             dst->ChannelMask |= KrAudioChannel_SideLeft;
		if (ext->dwChannelMask & SPEAKER_SIDE_RIGHT)            dst->ChannelMask |= KrAudioChannel_SideRight;
		if (ext->dwChannelMask & SPEAKER_TOP_CENTER)            dst->ChannelMask |= KrAudioChannel_TopCenter;
		if (ext->dwChannelMask & SPEAKER_TOP_FRONT_LEFT)        dst->ChannelMask |= KrAudioChannel_TopFrontLeft;
		if (ext->dwChannelMask & SPEAKER_TOP_FRONT_CENTER)      dst->ChannelMask |= KrAudioChannel_TopFrontCenter;
		if (ext->dwChannelMask & SPEAKER_TOP_FRONT_RIGHT)       dst->ChannelMask |= KrAudioChannel_TopFrontRight;
		if (ext->dwChannelMask & SPEAKER_TOP_BACK_LEFT)         dst->ChannelMask |= KrAudioChannel_TopBackLeft;
		if (ext->dwChannelMask & SPEAKER_TOP_BACK_CENTER)       dst->ChannelMask |= KrAudioChannel_TopBackCenter;
		if (ext->dwChannelMask & SPEAKER_TOP_BACK_RIGHT)        dst->ChannelMask |= KrAudioChannel_TopBackRight;
	}

	dst->Channels         = src->nChannels;
	dst->SamplesPerSecond = src->nSamplesPerSec;
}

typedef struct KrAudioDeviceId {
	char16_t *Identifier;
} KrAudioDeviceId;

typedef enum KrAudioEvent {
	KrAudioEvent_Update,
	KrAudioEvent_Resume,
	KrAudioEvent_Pause,
	KrAudioEvent_Reset,
	KrAudioEvent_EnumMax
} KrAudioEvent;

struct KrAudioDevice {
	IAudioClient *        Client;
	IAudioRenderClient *  Render;
	KrAudioContext        Context;
	KrAudioSpec           TranslatedSpec;
	u32                   Frames;
	HANDLE                Events[KrAudioEvent_EnumMax];
	volatile LONG         Playing;
	KrAudioDeviceId       DeviceId;
	HANDLE                Thread;
	WAVEFORMATEXTENSIBLE  RequestedFormat;
};

static void KrAudioDeviceDetach(KrAudioDevice *device) {
	if (device->Render)
		IAudioRenderClient_Release(device->Render);
	if (device->Client)
		IAudioClient_Release(device->Client);
	device->Render = nullptr;
	device->Client = nullptr;
}

static bool KrAudioDeviceAttach(KrAudioDevice *device) {
	HRESULT hr;

	IMMDevice *endpoint  = nullptr;
	WAVEFORMATEX *format = nullptr;

	if (device->DeviceId.Identifier) {
		hr = IMMDeviceEnumerator_GetDevice(Audio.DeviceEnumerator, device->DeviceId.Identifier, &endpoint);
		if (hr == E_NOTFOUND || hr == E_OUTOFMEMORY) goto failed;
		if (FAILED(hr)) KrInternalError();
	} else {
		hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(Audio.DeviceEnumerator, eRender, eMultimedia, &endpoint);
		if (hr == E_NOTFOUND || hr == E_OUTOFMEMORY) goto failed;
		if (FAILED(hr)) KrInternalError();

		IMMDevice_GetId(endpoint, &device->DeviceId.Identifier);
	}

	hr = IMMDevice_Activate(endpoint, &KR_IID_IAudioClient, CLSCTX_ALL, nullptr, &device->Client);
	if (hr == E_NOTFOUND || hr == AUDCLNT_E_DEVICE_INVALIDATED) goto failed;
	if (FAILED(hr)) KrInternalError();

	hr = IAudioClient_IsFormatSupported(device->Client, AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX *)&device->RequestedFormat, &format);
	if (hr != S_OK && hr != S_FALSE) {
		hr = IAudioClient_GetMixFormat(device->Client, &format);
		if (hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == E_OUTOFMEMORY) goto failed;
		if (FAILED(hr)) KrInternalError();
	}

	KrWaveFormatToAudioSpec(&device->TranslatedSpec, format);

	REFERENCE_TIME device_period;
	hr = IAudioClient_GetDevicePeriod(device->Client, nullptr, &device_period);
	if (hr == AUDCLNT_E_DEVICE_INVALIDATED) goto failed;
	if (FAILED(hr)) KrInternalError();

	hr = IAudioClient_Initialize(device->Client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		device_period, device_period, format, nullptr);
	if (hr == AUDCLNT_E_CPUUSAGE_EXCEEDED || hr == AUDCLNT_E_DEVICE_INVALIDATED || hr == AUDCLNT_E_DEVICE_IN_USE ||
		hr == AUDCLNT_E_ENDPOINT_CREATE_FAILED || hr == E_OUTOFMEMORY) goto failed;
	if (FAILED(hr)) KrInternalError();

	hr = IAudioClient_SetEventHandle(device->Client, device->Events[KrAudioEvent_Update]);
	if (hr == AUDCLNT_E_DEVICE_INVALIDATED) goto failed;
	if (FAILED(hr)) KrInternalError();

	hr = IAudioClient_GetBufferSize(device->Client, &device->Frames);
	if (hr == AUDCLNT_E_DEVICE_INVALIDATED) goto failed;
	if (FAILED(hr)) KrInternalError();
	
	hr = IAudioClient_GetService(device->Client, &KR_IID_IAudioRenderClient, &device->Render);
	if (hr == AUDCLNT_E_DEVICE_INVALIDATED) goto failed;
	if (FAILED(hr)) KrInternalError();

	CoTaskMemFree(format);
	IMMDeviceEnumerator_Release(endpoint);

	return true;

failed:
	if (endpoint)
		IMMDeviceEnumerator_Release(endpoint);
	if (format)
		CoTaskMemFree(format);
	KrAudioDeviceDetach(device);

	return false;
}

static DWORD WINAPI KrAudioThread(LPVOID param) {
	HANDLE thread = GetCurrentThread();
	SetThreadDescription(thread, L"PL Audio Thread");

	DWORD task_index;
	HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	KrAudioDevice *device = param;

	if (KrAudioDeviceAttach(device)) {
		device->Context.DeviceGained(device, &device->DeviceId);
	} else {
		device->Context.DeviceLost(device, &device->DeviceId);
	}

	while (1) {
		DWORD wait = WaitForMultipleObjects(KrAudioEvent_EnumMax, device->Events, FALSE, INFINITE);

		HRESULT hr = S_OK;

		if (wait == WAIT_OBJECT_0 + KrAudioEvent_Update) {
			UINT32 padding = 0;
			hr = IAudioClient_GetCurrentPadding(device->Client, &padding);

			if (SUCCEEDED(hr)) {
				UINT32 frames = device->Frames - padding;
				if (frames == 0) continue;

				BYTE *data    = nullptr;
				hr = IAudioRenderClient_GetBuffer(device->Render, frames, &data);

				if (SUCCEEDED(hr)) {
					u32 written = device->Context.Update(device, &device->TranslatedSpec, data, frames, device->Context.UserData);

					DWORD flags = 0;
					if (written < frames)
						flags |= AUDCLNT_BUFFERFLAGS_SILENT;

					hr = IAudioRenderClient_ReleaseBuffer(device->Render, written, flags);

					if (SUCCEEDED(hr))
						continue;
				}
			}
		}
		
		else if (wait == WAIT_OBJECT_0 + KrAudioEvent_Resume) {
			InterlockedExchange(&device->Playing, 1);
			hr = IAudioClient_Start(device->Client);

			device->Context.Resumed(device);

			if (hr == AUDCLNT_E_NOT_STOPPED)
				continue;
		}
		
		else if (wait == WAIT_OBJECT_0 + KrAudioEvent_Pause) {
			InterlockedExchange(&device->Playing, 0);
			hr = IAudioClient_Stop(device->Client);
			device->Context.Paused(device);
		}

		else if (wait == WAIT_OBJECT_0 + KrAudioEvent_Reset) {
			InterlockedExchange(&device->Playing, 0);
			IAudioClient_Stop(device->Client);

			hr = IAudioClient_Reset(device->Client);
			device->Context.Reset(device);
		}

		if (hr == AUDCLNT_E_DEVICE_INVALIDATED) {
			InterlockedExchange(&device->Playing, 0);
			device->Context.Paused(device);
			KrAudioDeviceDetach(device);
			device->Context.DeviceLost(device, &device->DeviceId);
		} else if (FAILED(hr)) {
			KrInternalError();
		}
	}

	AvRevertMmThreadCharacteristics(&avrt);

	return 0;
}

static u32  KrAudioUpdateFallback(KrAudioDevice *device, KrAudioSpec *spec, u8 *dst, u32 count, void *user) { return count; }
static void KrAudioResumedFallback(KrAudioDevice *device) {}
static void KrAudioPausedFallback(KrAudioDevice *device) {}
static void KrAudioResetFallback(KrAudioDevice *device) {}
static void KrAudioDeviceLostFallback(KrAudioDevice *device, KrAudioDeviceId *id) {}
static void KrAudioDeviceGainedFallback(KrAudioDevice *device, KrAudioDeviceId *id) {}

KrAudioDevice *KrOpenAudioDevice(const KrAudioContext *ctx, const KrAudioSpec *spec, const KrAudioDeviceId *id) {
	KrInitAudioInstance();

	KrAudioDevice *device = CoTaskMemAlloc(sizeof(KrAudioDevice) + sizeof(WAVEFORMATEXTENSIBLE));
	if (!device) {
		SetLastError(E_OUTOFMEMORY);
		goto failed;
	}

	memset(device, 0, sizeof(*device));

	const int MAX_EVENTS = 16;

	device->Events[KrAudioEvent_Update] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	device->Events[KrAudioEvent_Resume] = CreateSemaphoreW(nullptr, 0, MAX_EVENTS, nullptr);
	device->Events[KrAudioEvent_Pause]  = CreateSemaphoreW(nullptr, 0, MAX_EVENTS, nullptr);
	device->Events[KrAudioEvent_Reset]  = CreateSemaphoreW(nullptr, 0, MAX_EVENTS, nullptr);

	for (int i = 0; i < KrAudioEvent_EnumMax; ++i) {
		if (!device->Events[i]) {
			goto failed;
		}
	}

	static const KrAudioContext FallbackContext = {
		.UserData     = nullptr,
		.Update       = KrAudioUpdateFallback,
		.Resumed      = KrAudioResumedFallback,
		.Paused       = KrAudioPausedFallback,
		.Reset        = KrAudioResetFallback,
		.DeviceLost   = KrAudioDeviceLostFallback,
		.DeviceGained = KrAudioDeviceGainedFallback,
	};

	if (!ctx) {
		ctx = &FallbackContext;
	}

	memcpy(&device->Context, ctx, sizeof(device->Context));

	if (!device->Context.Update)       device->Context.Update       = FallbackContext.Update;
	if (!device->Context.Resumed)      device->Context.Resumed      = FallbackContext.Resumed;
	if (!device->Context.Paused)       device->Context.Paused       = FallbackContext.Paused;
	if (!device->Context.Reset)        device->Context.Reset        = FallbackContext.Reset;
	if (!device->Context.DeviceLost)   device->Context.DeviceLost   = FallbackContext.DeviceLost;
	if (!device->Context.DeviceGained) device->Context.DeviceGained = FallbackContext.DeviceGained;

	static const KrAudioSpec DefaultSpec = {
		.Format           = KrAudioFormat_R32,
		.Channels         = 2,
		.ChannelMask      = KrAudioChannel_FrontLeft | KrAudioChannel_FrontRight,
		.SamplesPerSecond = 48000
	};

	if (!spec) spec = &DefaultSpec;

	if (id) {
		umem  id_length = 0;
		for (const char16_t *first = id->Identifier; *first; ++first)
			id_length += 1;

		device->DeviceId.Identifier = CoTaskMemAlloc(id_length + 1);
		if (device->DeviceId.Identifier) {
			memcpy(device->DeviceId.Identifier, id->Identifier, id_length * sizeof(char16_t));
			device->DeviceId.Identifier[id_length] = 0;
		}
	}

	KrAudioSpecToWaveFormat(&device->RequestedFormat, spec);

	device->Thread = CreateThread(nullptr, 0, KrAudioThread, device, 0, nullptr);
	if (!device->Thread) goto failed;

	return device;

failed:
	KrCloseAudioDevice(device);
	return nullptr;
}

void KrCloseAudioDevice(KrAudioDevice *device) {
	if (device) {
		if (device->Thread)
			TerminateThread(device->Thread, 0);

		KrAudioDeviceDetach(device);

		for (int i = 0; i < KrAudioEvent_EnumMax; ++i) {
			if (device->Events[i])
				CloseHandle(device->Events[i]);
		}

		if (device->DeviceId.Identifier) {
			CoTaskMemFree(device->DeviceId.Identifier);
		}

		CoTaskMemFree(device);
	}

	KrDeinitAudioInstance();
}

bool KrIsAudioDevicePlaying(KrAudioDevice *device) {
	return device->Playing;
}

void KrResumeAudioDevice(KrAudioDevice *device) {
	ReleaseSemaphore(device->Events[KrAudioEvent_Resume], 1, 0);
}

void KrPauseAudioDevice(KrAudioDevice *device) {
	ReleaseSemaphore(device->Events[KrAudioEvent_Pause], 1, 0);
}

void KrResetAudioDevice(KrAudioDevice *device) {
	ReleaseSemaphore(device->Events[KrAudioEvent_Reset], 1, 0);
}

void KrUpdateAudioDevice(KrAudioDevice *device) {
	if (!KrIsAudioDevicePlaying(device))
		SetEvent(device->Events[KrAudioEvent_Update]);
}
