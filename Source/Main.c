#pragma warning(push)
#pragma warning(disable: 5105)
#define COBJMACROS

#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>

#pragma warning(pop)

#pragma comment(lib, "Avrt.lib")

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Platform.h"

DEFINE_GUID(PL_CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(PL_IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(PL_IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(PL_IID_IAudioClient, 0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(PL_IID_IAudioRenderClient, 0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);
DEFINE_GUID(PL_IID_IMMNotificationClient, 0x7991eec9, 0x7e89, 0x4d85, 0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX, 0x00000000L, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

typedef enum PL_AudioFormat {
	PL_AudioFormat_ANY,
	PL_AudioFormat_R32,
	PL_AudioFormat_I32,
	PL_AudioFormat_I16,
	PL_AudioFormat_EnumMax
} PL_AudioFormat;

typedef enum PL_AudioChannelMasks {
	PL_AudioChannel_FrontLeft          = 0x00001,
	PL_AudioChannel_FrontRight         = 0x00002,
	PL_AudioChannel_FrontCenter        = 0x00004,
	PL_AudioChannel_LowFrequency       = 0x00008,
	PL_AudioChannel_BackLeft           = 0x00010,
	PL_AudioChannel_BackRight          = 0x00020,
	PL_AudioChannel_FrontLeftOfCenter  = 0x00040,
	PL_AudioChannel_FrontRightOfCenter = 0x00080,
	PL_AudioChannel_BackCenter         = 0x00100,
	PL_AudioChannel_SideLeft           = 0x00200,
	PL_AudioChannel_SideRight          = 0x00400,
	PL_AudioChannel_TopCenter          = 0x00800,
	PL_AudioChannel_TopFrontLeft       = 0x01000,
	PL_AudioChannel_TopFrontCenter     = 0x02000,
	PL_AudioChannel_TopFrontRight      = 0x04000,
	PL_AudioChannel_TopBackLeft        = 0x08000,
	PL_AudioChannel_TopBackCenter      = 0x10000,
	PL_AudioChannel_TopBackRight       = 0x20000
} PL_AudioChannelMasks;

typedef struct PL_AudioChannel {
	u32 count;
	u32 mask;
} PL_AudioChannel;

typedef struct PL_AudioSampleRate {
	u32 secs;
} PL_AudioSampleRate;

typedef struct PL_AudioSpec {
	PL_AudioFormat     format;
	PL_AudioChannel    channels;
	PL_AudioSampleRate rate;
} PL_AudioSpec;

typedef u32(*PL_AudioUpdateProc)(struct PL_AudioSpec const *spec, Buffer samples, void *user);

static void PL_FatalError(LPWSTR message) {
	DWORD error    = GetLastError();

	if (error) {
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&message, 0, NULL);
	}

	FatalAppExitW(0, message);
}

static bool PL_Failed(HRESULT hr) {
	if (SUCCEEDED(hr))
		return false;
	PL_FatalError(L"Fatal Internal Error");
	return true;
}

static bool PL_AudioFailed(HRESULT hr) {
	if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
		return true;
	return PL_Failed(hr);
}

enum PL_AudioDeviceFlags {
	PL_AudioDevice_IsLost       = 0x1,
	PL_AudioDevice_IsNotPresent = 0x2
};

typedef struct PL_AudioDevice {
	u32                 flags;
	u32                 frames;
	PL_AudioSpec        spec;
	IAudioClient *      client;
	IAudioRenderClient *render;
	WAVEFORMATEX *      format;
	char16_t *          id;
} PL_AudioDevice;

typedef enum PL_AudioEvent {
	PL_AudioEvent_Update,
	PL_AudioEvent_Play,
	PL_AudioEvent_Pause,
	PL_AudioEvent_Reset,
	PL_AudioEvent_EnumMax
} PL_AudioEvent;

typedef struct PL_AudioEndpoint {
	PL_AudioDevice        device;
	HANDLE                events[PL_AudioEvent_EnumMax];
	PL_AudioUpdateProc    update;
	void *                user;
	volatile LONG         playing;
	HANDLE                thread;
	WAVEFORMATEXTENSIBLE *want;
} PL_AudioEndpoint;

static struct {
	volatile LONG        Initialized;
	IMMDeviceEnumerator *DeviceEnumerator;
} Instance;

void PL_InitAudioInstance(void) {
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (hr != S_OK && hr != S_FALSE && hr != RPC_E_CHANGED_MODE) {
		FatalAppExitW(0, L"Failed to initialize audio instance");
	}

	if (InterlockedCompareExchange(&Instance.Initialized, 1, 0) == 1) {
		IMMDeviceEnumerator_AddRef(Instance.DeviceEnumerator);
		return;
	}

	hr = CoCreateInstance(&PL_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &PL_IID_IMMDeviceEnumerator, &Instance.DeviceEnumerator);
	if (FAILED(hr)) {
		FatalAppExitW(0, L"Failed to initialize audio instance");
	}
}

void PL_DeinitAudioInstance(void) {
	if (IMMDeviceEnumerator_Release(Instance.DeviceEnumerator) == 0) {
		InterlockedExchange(&Instance.Initialized, 0);
		CoUninitialize();
	}
}

static void PL_TranslateAudioSpecToWaveFormat(WAVEFORMATEXTENSIBLE *dst, const PL_AudioSpec *src) {
	memset(dst, 0, sizeof(*dst));

	dst->Format.nSamplesPerSec = src->rate.secs;

	if (src->channels.count == 2) {
		if (src->format == PL_AudioFormat_R32) {
			dst->Format.wFormatTag     = WAVE_FORMAT_IEEE_FLOAT;
			dst->Format.wBitsPerSample = 32;
		} else if (src->format == PL_AudioFormat_I32) {
			dst->Format.wFormatTag     = WAVE_FORMAT_PCM;
			dst->Format.wBitsPerSample = 32;
		} else if (src->format == PL_AudioFormat_I16) {
			dst->Format.wFormatTag     = WAVE_FORMAT_PCM;
			dst->Format.wBitsPerSample = 16;
		}
		dst->Format.nChannels = 2;
		dst->Format.cbSize    = sizeof(WAVEFORMATEX);
	} else {
		dst->Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		if (src->format == PL_AudioFormat_R32) {
			memcpy(&dst->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID));
			dst->Format.wBitsPerSample = 32;
		} else if (src->format == PL_AudioFormat_I32) {
			memcpy(&dst->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID));
			dst->Format.wBitsPerSample = 32;
		} else if (src->format == PL_AudioFormat_I16) {
			memcpy(&dst->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID));
			dst->Format.wBitsPerSample = 16;
		}
		dst->Format.nChannels = src->channels.count;

		if (src->channels.mask & PL_AudioChannel_FrontLeft)          dst->dwChannelMask |=  SPEAKER_FRONT_LEFT;
		if (src->channels.mask & PL_AudioChannel_FrontRight)         dst->dwChannelMask |=  SPEAKER_FRONT_RIGHT;
		if (src->channels.mask & PL_AudioChannel_FrontCenter)        dst->dwChannelMask |=  SPEAKER_FRONT_CENTER;
		if (src->channels.mask & PL_AudioChannel_LowFrequency)       dst->dwChannelMask |=  SPEAKER_LOW_FREQUENCY;
		if (src->channels.mask & PL_AudioChannel_BackLeft)           dst->dwChannelMask |=  SPEAKER_BACK_LEFT;
		if (src->channels.mask & PL_AudioChannel_BackRight)          dst->dwChannelMask |=  SPEAKER_BACK_RIGHT;
		if (src->channels.mask & PL_AudioChannel_FrontLeftOfCenter)  dst->dwChannelMask |=  SPEAKER_FRONT_LEFT_OF_CENTER;
		if (src->channels.mask & PL_AudioChannel_FrontRightOfCenter) dst->dwChannelMask |= SPEAKER_FRONT_RIGHT_OF_CENTER;
		if (src->channels.mask & PL_AudioChannel_BackCenter)         dst->dwChannelMask |=  SPEAKER_BACK_CENTER;
		if (src->channels.mask & PL_AudioChannel_SideLeft)           dst->dwChannelMask |=  SPEAKER_SIDE_LEFT;
		if (src->channels.mask & PL_AudioChannel_SideRight)          dst->dwChannelMask |=  SPEAKER_SIDE_RIGHT;
		if (src->channels.mask & PL_AudioChannel_TopCenter)          dst->dwChannelMask |=  SPEAKER_TOP_CENTER;
		if (src->channels.mask & PL_AudioChannel_TopFrontLeft)       dst->dwChannelMask |=  SPEAKER_TOP_FRONT_LEFT;
		if (src->channels.mask & PL_AudioChannel_TopFrontCenter)     dst->dwChannelMask |=  SPEAKER_TOP_FRONT_CENTER;
		if (src->channels.mask & PL_AudioChannel_TopFrontRight)      dst->dwChannelMask |=  SPEAKER_TOP_FRONT_RIGHT;
		if (src->channels.mask & PL_AudioChannel_TopBackLeft)        dst->dwChannelMask |=  SPEAKER_TOP_BACK_LEFT;
		if (src->channels.mask & PL_AudioChannel_TopBackCenter)      dst->dwChannelMask |=  SPEAKER_TOP_BACK_CENTER;
		if (src->channels.mask & PL_AudioChannel_TopBackRight)       dst->dwChannelMask |=  SPEAKER_TOP_BACK_RIGHT;

		dst->Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE);
	}

	dst->Format.nBlockAlign     = (dst->Format.nChannels * dst->Format.wBitsPerSample) / 8;
	dst->Format.nAvgBytesPerSec = dst->Format.nSamplesPerSec * dst->Format.nBlockAlign;
}

static void PL_TranslateWaveFormatToAudioSpec(PL_AudioSpec *dst, const WAVEFORMATEX *src) {
	const WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)src;

	if (src->wFormatTag == WAVE_FORMAT_IEEE_FLOAT && src->wBitsPerSample == 32) {
		dst->format = PL_AudioFormat_R32;
	} else if (src->wFormatTag == WAVE_FORMAT_PCM && src->wBitsPerSample == 32) {
		dst->format = PL_AudioFormat_I32;
	} else if (src->wFormatTag == WAVE_FORMAT_PCM && src->wBitsPerSample == 16) {
		dst->format = PL_AudioFormat_I16;
	} else if (src->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0) && (src->wBitsPerSample == 32)) {
			dst->format = PL_AudioFormat_R32;
		} else if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) && (src->wBitsPerSample == 32)) {
			dst->format = PL_AudioFormat_I32;
		} else if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) && (src->wBitsPerSample == 16)) {
			dst->format = PL_AudioFormat_I16;
		} else {
			dst->format = PL_AudioFormat_ANY;
		}
	} else {
		dst->format = PL_AudioFormat_ANY;
	}

	dst->channels.mask  = 0;

	if (src->nChannels == 2) {
		dst->channels.mask |= PL_AudioChannel_FrontLeft;
		dst->channels.mask |= PL_AudioChannel_FrontRight;
	} else if (src->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		if (ext->dwChannelMask & SPEAKER_FRONT_LEFT)            dst->channels.mask |= PL_AudioChannel_FrontLeft;
		if (ext->dwChannelMask & SPEAKER_FRONT_RIGHT)           dst->channels.mask |= PL_AudioChannel_FrontRight;
		if (ext->dwChannelMask & SPEAKER_FRONT_CENTER)          dst->channels.mask |= PL_AudioChannel_FrontCenter;
		if (ext->dwChannelMask & SPEAKER_LOW_FREQUENCY)         dst->channels.mask |= PL_AudioChannel_LowFrequency;
		if (ext->dwChannelMask & SPEAKER_BACK_LEFT)             dst->channels.mask |= PL_AudioChannel_BackLeft;
		if (ext->dwChannelMask & SPEAKER_BACK_RIGHT)            dst->channels.mask |= PL_AudioChannel_BackRight;
		if (ext->dwChannelMask & SPEAKER_FRONT_LEFT_OF_CENTER)  dst->channels.mask |= PL_AudioChannel_FrontLeftOfCenter;
		if (ext->dwChannelMask & SPEAKER_FRONT_RIGHT_OF_CENTER) dst->channels.mask |= PL_AudioChannel_FrontRightOfCenter;
		if (ext->dwChannelMask & SPEAKER_BACK_CENTER)           dst->channels.mask |= PL_AudioChannel_BackCenter;
		if (ext->dwChannelMask & SPEAKER_SIDE_LEFT)             dst->channels.mask |= PL_AudioChannel_SideLeft;
		if (ext->dwChannelMask & SPEAKER_SIDE_RIGHT)            dst->channels.mask |= PL_AudioChannel_SideRight;
		if (ext->dwChannelMask & SPEAKER_TOP_CENTER)            dst->channels.mask |= PL_AudioChannel_TopCenter;
		if (ext->dwChannelMask & SPEAKER_TOP_FRONT_LEFT)        dst->channels.mask |= PL_AudioChannel_TopFrontLeft;
		if (ext->dwChannelMask & SPEAKER_TOP_FRONT_CENTER)      dst->channels.mask |= PL_AudioChannel_TopFrontCenter;
		if (ext->dwChannelMask & SPEAKER_TOP_FRONT_RIGHT)       dst->channels.mask |= PL_AudioChannel_TopFrontRight;
		if (ext->dwChannelMask & SPEAKER_TOP_BACK_LEFT)         dst->channels.mask |= PL_AudioChannel_TopBackLeft;
		if (ext->dwChannelMask & SPEAKER_TOP_BACK_CENTER)       dst->channels.mask |= PL_AudioChannel_TopBackCenter;
		if (ext->dwChannelMask & SPEAKER_TOP_BACK_RIGHT)        dst->channels.mask |= PL_AudioChannel_TopBackRight;
	}

	dst->channels.count = src->nChannels;
	dst->rate.secs      = src->nSamplesPerSec;
}

static void PL_CloseAudioDevice(PL_AudioDevice *device) {
	if (device->client) IAudioClient_Release(device->client);
	if (device->render) IAudioRenderClient_Release(device->render);
	if (device->format) CoTaskMemFree(device->format);
	if (device->id)     CoTaskMemFree(device->id);
	memset(device, 0, sizeof(*device));
}

static void PL_OpenAudioDevice(PL_AudioDevice *device, const char16_t *id, WAVEFORMATEXTENSIBLE *want, HANDLE event) {
	HRESULT hr;

	IMMDevice *endpoint = nullptr;

	if (id) {
		hr = IMMDeviceEnumerator_GetDevice(Instance.DeviceEnumerator, id, &endpoint);
		if (hr != E_NOTFOUND) {
			if (PL_Failed(hr)) {
				device->flags |= PL_AudioDevice_IsLost;
				return;
			}
		}
	}

	if (!endpoint) {
		hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(Instance.DeviceEnumerator, eRender, eMultimedia, &endpoint);
		if (hr == E_NOTFOUND) {
			device->flags |= PL_AudioDevice_IsNotPresent;
			return;
		}
	}

	if (PL_Failed(hr)) {
		device->flags |= PL_AudioDevice_IsNotPresent;
		return;
	}

	device->flags = 0;

	IMMDevice_GetId(endpoint, &device->id);

	hr = IMMDevice_Activate(endpoint, &PL_IID_IAudioClient, CLSCTX_ALL, nullptr, &device->client);
	if (PL_AudioFailed(hr)) goto failed;

	if (want) {
		hr = IAudioClient_IsFormatSupported(device->client, AUDCLNT_SHAREMODE_SHARED, (WAVEFORMATEX *)want, &device->format);
		if (hr != S_OK && hr != S_FALSE) {
			hr = IAudioClient_GetMixFormat(device->client, &device->format);
			if (PL_AudioFailed(hr)) goto failed;
		}
	} else {
		hr = IAudioClient_GetMixFormat(device->client, &device->format);
		if (PL_AudioFailed(hr)) goto failed;
	}

	REFERENCE_TIME device_period;
	hr = IAudioClient_GetDevicePeriod(device->client, nullptr, &device_period);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_Initialize(device->client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		device_period, device_period, device->format, nullptr);
	Assert(hr != AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED);
	if (PL_AudioFailed(hr)) goto failed;

	PL_TranslateWaveFormatToAudioSpec(&device->spec, device->format);

	hr = IAudioClient_SetEventHandle(device->client, event);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_GetBufferSize(device->client, &device->frames);
	if (PL_AudioFailed(hr)) goto failed;
	
	hr = IAudioClient_GetService(device->client, &PL_IID_IAudioRenderClient, &device->render);
	if (PL_AudioFailed(hr)) goto failed;

	IMMDevice_Release(endpoint);

	return;

failed:
	PL_CloseAudioDevice(device);
	IMMDevice_Release(endpoint);

	device->flags |= PL_AudioDevice_IsLost;
}

static void PL_ImUpdateAudioEndpoint(PL_AudioEndpoint *endpoint) {
	PL_AudioDevice *device = &endpoint->device;

	UINT32 padding = 0;
	HRESULT hr = IAudioClient_GetCurrentPadding(device->client, &padding);
	if (PL_AudioFailed(hr)) goto failed;

	UINT32 frames = device->frames - padding;
	if (frames == 0) return;

	BYTE *data    = nullptr;
	hr = IAudioRenderClient_GetBuffer(device->render, frames, &data);
	if (PL_AudioFailed(hr)) goto failed;

	u32 written = endpoint->update(&device->spec, (Buffer){ .count=frames, .data=data }, endpoint->user);

	DWORD flags = 0;
	if (written < frames)
		flags |= AUDCLNT_BUFFERFLAGS_SILENT;

	hr = IAudioRenderClient_ReleaseBuffer(device->render, written, flags);
	if (PL_AudioFailed(hr)) goto failed;

	return;

failed:
	device->flags |= PL_AudioDevice_IsLost;
}

static bool PL_ImRecoverAudioDeviceIfLost(PL_AudioEndpoint *endpoint) {
	PL_AudioDevice *device = &endpoint->device;

	if (device->flags & PL_AudioDevice_IsLost) {
		PL_CloseAudioDevice(device);
		PL_OpenAudioDevice(device, nullptr, endpoint->want, endpoint->events[PL_AudioEvent_Update]);
		return true;
	} else if (device->flags & PL_AudioDevice_IsNotPresent) {
		PL_OpenAudioDevice(device, nullptr, endpoint->want, endpoint->events[PL_AudioEvent_Update]);
		return true;
	}
	return false;
}

static DWORD WINAPI PL_AudioThread(LPVOID param) {
	HANDLE thread = GetCurrentThread();
	SetThreadDescription(thread, L"PL Audio Thread");

	DWORD task_index;
	HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	PL_AudioEndpoint *endpoint = param;

	while (1) {
		DWORD wait = WaitForMultipleObjects(PL_AudioEvent_EnumMax, endpoint->events, FALSE, 200);

		switch (wait) {
		case WAIT_OBJECT_0 + PL_AudioEvent_Update:
		{
			PL_ImUpdateAudioEndpoint(endpoint);
		} break;

		case WAIT_OBJECT_0 + PL_AudioEvent_Play:
		{
			InterlockedExchange(&endpoint->playing, 1);
			HRESULT hr = IAudioClient_Start(endpoint->device.client);
			if (hr != AUDCLNT_E_NOT_STOPPED) {
				if (PL_AudioFailed(hr)) {
					endpoint->device.flags |= PL_AudioDevice_IsLost;
				}
			}
		} break;

		case WAIT_OBJECT_0 + PL_AudioEvent_Pause:
		{
			InterlockedExchange(&endpoint->playing, 0);
			HRESULT hr = IAudioClient_Stop(endpoint->device.client);
			if (PL_AudioFailed(hr)) {
				endpoint->device.flags |= PL_AudioDevice_IsLost;
			}
		} break;

		case WAIT_OBJECT_0 + PL_AudioEvent_Reset:
		{
			InterlockedExchange(&endpoint->playing, 0);
			IAudioClient_Stop(endpoint->device.client);
			HRESULT hr = IAudioClient_Reset(endpoint->device.client);
			if (PL_AudioFailed(hr)) {
				endpoint->device.flags |= PL_AudioDevice_IsLost;
			}
		} break;

		case WAIT_TIMEOUT:
		{
			if (PL_ImRecoverAudioDeviceIfLost(endpoint)) {
				if (endpoint->playing) {
					HRESULT hr = IAudioClient_Start(endpoint->device.client);
					if (PL_AudioFailed(hr)) {
						endpoint->device.flags |= PL_AudioDevice_IsLost;
					}
				}
			}
		} break;
		}
	}

	AvRevertMmThreadCharacteristics(&avrt);

	return 0;
}

void PL_CloseAudioEndpoint(PL_AudioEndpoint *endpoint) {
	if (endpoint) {
		if (endpoint->thread)
			TerminateThread(endpoint->thread, 0);

		PL_CloseAudioDevice(&endpoint->device);

		for (int i = 0; i < PL_AudioEvent_EnumMax; ++i) {
			if (endpoint->events[i])
				CloseHandle(endpoint->events[i]);
		}

		CoTaskMemFree(endpoint);
	}

	PL_DeinitAudioInstance();
}

PL_AudioEndpoint *PL_OpenAudioEndpoint(const char16_t *device_id, const PL_AudioSpec *spec, PL_AudioUpdateProc update, void *user) {
	PL_InitAudioInstance();

	PL_AudioEndpoint *endpoint = CoTaskMemAlloc(sizeof(PL_AudioEndpoint) + sizeof(WAVEFORMATEXTENSIBLE));
	if (!endpoint) goto failed;

	memset(endpoint, 0, sizeof(*endpoint));

	const int MAX_EVENTS = 16;

	endpoint->events[PL_AudioEvent_Update] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	endpoint->events[PL_AudioEvent_Play]   = CreateSemaphoreW(nullptr, 0, MAX_EVENTS, nullptr);
	endpoint->events[PL_AudioEvent_Pause]  = CreateSemaphoreW(nullptr, 0, MAX_EVENTS, nullptr);
	endpoint->events[PL_AudioEvent_Reset]  = CreateSemaphoreW(nullptr, 0, MAX_EVENTS, nullptr);
	
	for (int i = 0; i < PL_AudioEvent_EnumMax; ++i) {
		if (!endpoint->events[i]) {
			PL_FatalError(L"Failed to open audio endpoint");
		}
	}

	endpoint->update = update;
	endpoint->user   = user;

	if (spec) {
		endpoint->want = (WAVEFORMATEXTENSIBLE *)(endpoint + 1);
		PL_TranslateAudioSpecToWaveFormat(endpoint->want, spec);
	}

	PL_OpenAudioDevice(&endpoint->device, device_id, endpoint->want, endpoint->events[PL_AudioEvent_Update]);

	endpoint->thread = CreateThread(nullptr, 0, PL_AudioThread, endpoint, 0, nullptr);
	if (!endpoint->thread) {
		PL_FatalError(L"Failed to create thread");
	}

	return endpoint;

failed:
	PL_CloseAudioEndpoint(endpoint);
	return nullptr;
}

bool PL_IsAudioEndpointPlaying(PL_AudioEndpoint *endpoint) {
	if (endpoint->device.flags == 0) {
		return endpoint->playing != 0;
	}
	return false;
}

void PL_UpdateAudioEndpoint(PL_AudioEndpoint *endpoint) {
	if (!PL_IsAudioEndpointPlaying(endpoint)) {
		SetEvent(endpoint->events[PL_AudioEvent_Update]);
	}
}

void PL_PauseAudioEndpoint(PL_AudioEndpoint *endpoint) {
	ReleaseSemaphore(endpoint->events[PL_AudioEvent_Pause], 1, nullptr);
}

void PL_ResumeAudioEndpoint(PL_AudioEndpoint *endpoint) {
	ReleaseSemaphore(endpoint->events[PL_AudioEvent_Play], 1, nullptr);
}

void PL_ResetAudioEndpoint(PL_AudioEndpoint *endpoint) {
	ReleaseSemaphore(endpoint->events[PL_AudioEvent_Reset], 1, nullptr);
}

//void PL_EnumerateAudioDevices(void) {
//	Assert(Instance.Initialized);
//
//	IMMDeviceCollection *devices = nullptr;
//	HRESULT hr = IMMDeviceEnumerator_EnumAudioEndpoints(Instance.DeviceEnumerator, eRender, DEVICE_STATE_ACTIVE, &devices);
//	if (FAILED(hr)) return;
//
//
//
//	IMMDeviceCollection_Release(devices);
//}

//HRESULT ApBackend_QueryInterface(IMMNotificationClient *_this, REFIID riid, void **ppvObject) {
//	if (memcmp(&IID_IUnknown, riid, sizeof(IID_IUnknown)) == 0) {
//		*ppvObject = (IUnknown *)_this;
//	} else if (memcmp(&IID_IMMNotificationClient, riid, sizeof(IID_IMMNotificationClient)) == 0) {
//		*ppvObject = (IMMNotificationClient *)_this;
//	} else {
//		*ppvObject = NULL;
//		return E_NOINTERFACE;
//	}
//	return S_OK;
//}
//
//ULONG ApBackend_AddRef(IMMNotificationClient *_this)  { 
//	ApBackend *backend = (ApBackend *)_this; 
//	return InterlockedIncrement(&backend->refcount);
//}
//
//ULONG ApBackend_Release(IMMNotificationClient *_this) { 
//	ApBackend *backend = (ApBackend *)_this; 
//	ULONG res = InterlockedDecrement(&backend->refcount);
//	if (res == 0) {
//		for (int i = 0; i < ApEvent_Max; ++i) {
//			CloseHandle(backend->events[i]);
//		}
//		for (int i = 0; i < ApDeviceKind_Max; ++i) {
//			ApDevice_Release(&backend->devices[i]);
//		}
//		IMMDeviceEnumerator_Release(backend->enumerator);
//		CoTaskMemFree(_this);
//	}
//	return res;
//}
//
//HRESULT ApBackend_OnDeviceStateChanged(IMMNotificationClient *_this, LPCWSTR dev_id, DWORD state) {	
//	ApBackend *backend = (ApBackend *)_this;
//	ApDevice * current = &backend->devices[ApDeviceKind_Current];
//
//	if (state == DEVICE_STATE_ACTIVE || state == DEVICE_STATE_DISABLED)
//		return S_OK;
//
//	if (lstrcmpW(dev_id, current->id) == 0) {
//		// The current device is disconnected, load a new device
//		if (ApBackend_LoadDevice(backend)) {
//			SetEvent(backend->events[ApEvent_DeviceChanged]);
//			DWORD wait = WaitForSingleObject(backend->events[ApEvent_DeviceInstalled], INFINITE);
//			if (wait == WAIT_OBJECT_0) {
//				ApDevice_Release(&backend->devices[ApDeviceKind_Backup]);
//			}
//			printf("state changed: loaded new device");
//		}
//	}
//
//	return S_OK;
//}
//
//HRESULT ApBackend_OnDeviceAdded(IMMNotificationClient *_this, LPCWSTR dev_id) {
//	return S_OK;
//}
//
//HRESULT ApBackend_OnDeviceRemoved(IMMNotificationClient *_this, LPCWSTR dev_id) {
//	return S_OK;
//}
//
//HRESULT ApBackend_OnDefaultDeviceChanged(IMMNotificationClient *_this, EDataFlow flow, ERole role, LPCWSTR dev_id) {
//	ApBackend *backend = (ApBackend *)_this;
//	ApDevice * current = &backend->devices[ApDeviceKind_Current];
//
//	printf("default changed: %u %u\n", flow, role);
//
//	if (flow != eRender || role != eMultimedia)
//		return S_OK;
//
//	if (lstrcmpW(dev_id, current->id) != 0) {
//		// Load the new default device
//		if (ApBackend_LoadDevice(backend)) {
//			SetEvent(backend->events[ApEvent_DeviceChanged]);
//			DWORD wait = WaitForSingleObject(backend->events[ApEvent_DeviceInstalled], INFINITE);
//			if (wait == WAIT_OBJECT_0) {
//				ApDevice_Release(&backend->devices[ApDeviceKind_Backup]);
//				printf("default changed: loaded new device\n");
//			}
//		}
//	}
//
//	return S_OK;
//}
//
//HRESULT ApBackend_OnPropertyValueChanged(IMMNotificationClient *_this, LPCWSTR dev_id, const PROPERTYKEY key) {
//	return S_OK;
//}
//
//HRESULT ApBackend_Create(ApBackend **out) {
//	ApBackend *backend = CoTaskMemAlloc(sizeof(*backend));
//	if (!backend) {
//		return E_OUTOFMEMORY;
//	}
//
//	memset(backend, 0, sizeof(*backend));
//
//	backend->notification.lpVtbl = &backend->vtable;
//	backend->vtable = (IMMNotificationClientVtbl) {
//		.AddRef                 = ApBackend_AddRef,
//		.Release                = ApBackend_Release,
//		.QueryInterface         = ApBackend_QueryInterface,
//		.OnDeviceStateChanged   = ApBackend_OnDeviceStateChanged,
//		.OnDeviceAdded          = ApBackend_OnDeviceAdded,
//		.OnDeviceRemoved        = ApBackend_OnDeviceRemoved,
//		.OnDefaultDeviceChanged = ApBackend_OnDefaultDeviceChanged,
//		.OnPropertyValueChanged = ApBackend_OnPropertyValueChanged
//	};
//
//	backend->refcount = 1;
//
//	HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &backend->enumerator);
//	if (FAILED(hr)) {
//		CoTaskMemFree(backend);
//		return hr;
//	}
//
//	hr = IMMDeviceEnumerator_RegisterEndpointNotificationCallback(backend->enumerator, &backend->notification);
//	if (FAILED(hr)) {
//		IMMDeviceEnumerator_Release(backend->enumerator);
//		CoTaskMemFree(backend);
//		return hr;
//	}
//
//	for (int i = 0; i < ApEvent_Max; ++i) {
//		backend->events[i] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
//		if (backend->events[i] == INVALID_HANDLE_VALUE) {
//			for (int j = i - 1; j >= 0; ++j)
//				CloseHandle(backend->events[i]);
//			IMMDeviceEnumerator_Release(backend->enumerator);
//			CoTaskMemFree(backend);
//		}
//	}
//
//	if (ApBackend_LoadDevice(backend)) {
//		backend->devices[ApDeviceKind_Current] = backend->devices[ApDeviceKind_Backup];
//		backend->devices[ApDeviceKind_Backup]  = (ApDevice){0};
//		ApBackend_Start(backend);
//	}
//
//	*out = backend;
//
//	return S_OK;
//}

//
//
//

void Usage(const char *path) {
	const char *program = path + strlen(path) - 1;

	for (; program != path; --program) {
		if (*program == '/' || *program == '\\') {
			program += 1;
			break;
		}
	}

	fprintf(stderr, "Usage: %s audio_file\n", program);
	exit(0);
}

String ReadEntireFile(const char *path) {
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "Failed to open file: %s. Reason: %s\n", path, strerror(errno));
		exit(1);
	}
	fseek(fp, 0, SEEK_END);
	umem size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	u8 *data = malloc(size);
	fread(data, size, 1, fp);
	fclose(fp);
	return (String){ .count=size, .data=data };
}

#pragma pack(push, 1)

typedef struct Riff_Header {
	u8	id[4];
	u32 size;
	u8	format[4];
} Riff_Header ;

typedef struct Wave_Fmt {
	u8	id[4];
	u32 size;
	u16 audio_format;
	u16 channels_count;
	u32 sample_rate;
	u32 byte_rate;
	u16 block_align;
	u16 bits_per_sample;
} Wave_Fmt ;

typedef struct Wave_Data {
	u8 id[4];
	u32 size;
} Wave_Data;

typedef struct Audio_Stream {
	Riff_Header header;
	Wave_Fmt	fmt;
	Wave_Data	data;
	i16			samples[1];
} Audio_Stream ;

#pragma pack(pop)

i16 *Serialize(Audio_Stream *audio, uint *count) {
	Wave_Data *ptr = &audio->data;
	for (; ptr->id[0] == 'L';) {
		uint sz = ptr->size;
		ptr += 1;
		ptr = (Wave_Data *)((u8 *)ptr + sz);
	}

	*count = ptr->size / sizeof(i16);

	return (i16 *)(ptr + 1);
}

static uint MaxSample     = 0;
static uint CurrentSample = 0;
static i16 *CurrentStream = 0;
static real Volume        = 0.5f;

u32 UpdateAudio(struct PL_AudioSpec const *spec, Buffer samples, void *user) {
	Assert(spec->format == PL_AudioFormat_R32 && spec->channels.count == 2);

	umem size = samples.count * spec->channels.count;

	r32 *dst  = (r32 *)samples.data;
	for (uint i = 0; i < size; i += spec->channels.count) {
		float l = (float)CurrentStream[CurrentSample + 0] / 32767.0f;
		float r = (float)CurrentStream[CurrentSample + 1] / 32767.0f;

		dst[i + 0] = Volume * l;
		dst[i + 1] = Volume * r;

		CurrentSample += 2;

		if (CurrentSample >= MaxSample)
			CurrentSample = 0;
	}

	return (u32)samples.count;
}

int main(int argc, char *argv[]) {
	if (argc <= 1){
		Usage(argv[0]);
	}

	const char *path = argv[1];

	String buff = ReadEntireFile(path);
	Audio_Stream *audio = (Audio_Stream *)buff.data;

	CurrentStream = Serialize(audio, &MaxSample);

	PL_InitAudioInstance();

	PL_AudioSpec spec = {
		.format         = PL_AudioFormat_R32,
		.channels.count = 2,
		.rate.secs      = 48000
	};

	PL_AudioEndpoint *endpoint = PL_OpenAudioEndpoint(nullptr, &spec, UpdateAudio, nullptr);

	PL_UpdateAudioEndpoint(endpoint);
	PL_ResumeAudioEndpoint(endpoint);

	char input[1024];

	for (;;) {
		printf("> ");
		int n = scanf_s("%s", input, (int)sizeof(input));

		if (strcmp(input, "resume") == 0) {
			PL_ResumeAudioEndpoint(endpoint);
		} else if (strcmp(input, "pause") == 0) {
			PL_PauseAudioEndpoint(endpoint);
		} else if (strcmp(input, "stop") == 0) {
			PL_ResetAudioEndpoint(endpoint);
			CurrentSample = 0;
		} else if (strcmp(input, "reset") == 0) {
			PL_ResetAudioEndpoint(endpoint);
			CurrentSample = 0;
			PL_UpdateAudioEndpoint(endpoint);
			PL_ResumeAudioEndpoint(endpoint);
		} else if (strcmp(input, "quit") == 0) {
			break;
		}
	}

	PL_CloseAudioEndpoint(endpoint);

	PL_DeinitAudioInstance();

	return 0;
}
