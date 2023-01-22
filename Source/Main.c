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
	volatile LONG       playing;
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

static struct {
	volatile LONG        Initialized;
	volatile LONG        Guard;
	HANDLE               Events[PL_AudioEvent_EnumMax];
	HANDLE               Thread;
	IMMDeviceEnumerator *DeviceEnumerator;
	PL_AudioUpdateProc   UpdateProc;
	void *               UserData;
	PL_AudioDevice       Device;
} G;

static void PL_InitAudioThread(void) {
	HANDLE thread = GetCurrentThread();
	SetThreadDescription(thread, L"PL Audio Thread");

	DWORD task_index;
	AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (hr != S_OK && hr != S_FALSE && hr != RPC_E_CHANGED_MODE) {
		FatalAppExitW(0, L"Failed to initialize audio thread");
	}

	hr = CoCreateInstance(&PL_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &PL_IID_IMMDeviceEnumerator, &G.DeviceEnumerator);
	if (FAILED(hr)) {
		FatalAppExitW(0, L"Failed to initialize audio thread");
	}

	G.Events[PL_AudioEvent_Update] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	G.Events[PL_AudioEvent_Play]   = CreateSemaphoreW(nullptr, 0, 255, nullptr);
	G.Events[PL_AudioEvent_Pause]  = CreateSemaphoreW(nullptr, 0, 255, nullptr);
	G.Events[PL_AudioEvent_Reset]  = CreateSemaphoreW(nullptr, 0, 255, nullptr);

	for (int i = 0; i < PL_AudioEvent_EnumMax; ++i) {
		if (!G.Events[i]) {
			PL_FatalError(L"Failed to start audio thread");
		}
	}
}

static void PL_CloseAudioDevice(void) {
	if (G.Device.client) IAudioClient_Release(G.Device.client);
	if (G.Device.render) IAudioRenderClient_Release(G.Device.render);
	if (G.Device.format) CoTaskMemFree(G.Device.format);
	if (G.Device.id)     CoTaskMemFree(G.Device.id);

	memset(&G.Device, 0, sizeof(G.Device));
}

static void PL_DeinitAudioThread(void) {
	PL_CloseAudioDevice();

	for (int i = 0; i < PL_AudioEvent_EnumMax; ++i) {
		CloseHandle(G.Events[i]);
		G.Events[i] = nullptr;
	}

	if (G.DeviceEnumerator)
		IMMDeviceEnumerator_Release(G.DeviceEnumerator);
	G.DeviceEnumerator = nullptr;
	CoUninitialize();
}

static void PL_StartAudioDevice(void) {
	InterlockedExchange(&G.Device.playing, 1);

	HRESULT hr  = IAudioClient_Start(G.Device.client);
	if (hr == AUDCLNT_E_NOT_STOPPED)
		return;
	if (PL_AudioFailed(hr)) {
		G.Device.flags |= PL_AudioDevice_IsLost;
	}
}

static void PL_StopAudioDevice(void) {
	InterlockedExchange(&G.Device.playing, 0);

	HRESULT hr = IAudioClient_Stop(G.Device.client);
	if (PL_AudioFailed(hr)) {
		G.Device.flags |= PL_AudioDevice_IsLost;
	}
}

static void PL_ResetAudioDevice(void) {
	InterlockedExchange(&G.Device.playing, 0);

	PL_StopAudioDevice();
	HRESULT hr = IAudioClient_Reset(G.Device.client);
	if (PL_AudioFailed(hr)) {
		G.Device.flags |= PL_AudioDevice_IsLost;
	}
}

static void PL_UpdateAudioDevice(void) {
	UINT32 padding = 0;
	HRESULT hr = IAudioClient_GetCurrentPadding(G.Device.client, &padding);
	if (PL_AudioFailed(hr)) goto failed;

	UINT32 frames = G.Device.frames - padding;
	BYTE *data    = nullptr;
	hr = IAudioRenderClient_GetBuffer(G.Device.render, frames, &data);
	if (PL_AudioFailed(hr)) goto failed;

	u32 written = G.UpdateProc(&G.Device.spec, (Buffer){ .count=frames, .data=data }, G.UserData);

	DWORD flags = 0;
	if (written < frames)
		flags |= AUDCLNT_BUFFERFLAGS_SILENT;

	hr = IAudioRenderClient_ReleaseBuffer(G.Device.render, written, flags);
	if (PL_AudioFailed(hr)) goto failed;

	return;

failed:
	G.Device.flags |= PL_AudioDevice_IsLost;
}

static void PL_TranslateSpec(PL_AudioSpec *dst, const WAVEFORMATEX *src) {
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

static void PL_OpenAudioDevice(char16_t *id) {
	HRESULT hr;

	IMMDevice *endpoint = nullptr;

	if (id) {
		hr = IMMDeviceEnumerator_GetDevice(G.DeviceEnumerator, id, &endpoint);
		if (hr != E_NOTFOUND) {
			if (PL_Failed(hr)) {
				G.Device.flags |= PL_AudioDevice_IsLost;
				return;
			}
		}
	}

	if (!endpoint) {
		hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(G.DeviceEnumerator, eRender, eMultimedia, &endpoint);
		if (hr == E_NOTFOUND) {
			G.Device.flags |= PL_AudioDevice_IsNotPresent;
			return;
		}
	}

	if (PL_Failed(hr)) {
		G.Device.flags |= PL_AudioDevice_IsNotPresent;
		return;
	}

	G.Device.flags = 0;

	IMMDevice_GetId(endpoint, &G.Device.id);

	hr = IMMDevice_Activate(endpoint, &PL_IID_IAudioClient, CLSCTX_ALL, nullptr, &G.Device.client);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_GetMixFormat(G.Device.client, &G.Device.format);
	if (PL_AudioFailed(hr)) goto failed;

	REFERENCE_TIME device_period;
	hr = IAudioClient_GetDevicePeriod(G.Device.client, nullptr, &device_period);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_Initialize(G.Device.client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		device_period, device_period, G.Device.format, nullptr);
	Assert(hr != AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_SetEventHandle(G.Device.client, G.Events[PL_AudioEvent_Update]);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_GetBufferSize(G.Device.client, &G.Device.frames);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_GetService(G.Device.client, &PL_IID_IAudioRenderClient, &G.Device.render);
	if (PL_AudioFailed(hr)) goto failed;

	IMMDevice_Release(endpoint);

	PL_TranslateSpec(&G.Device.spec, G.Device.format);

	PL_UpdateAudioDevice();
	PL_StartAudioDevice();

	return;

failed:
	PL_CloseAudioDevice();
	IMMDevice_Release(endpoint);

	G.Device.flags |= PL_AudioDevice_IsLost;

	return;
}

static void PL_ReloadAudioDeviceIfLost() {
	if (G.Device.flags & PL_AudioDevice_IsLost) {
		PL_CloseAudioDevice();
		PL_OpenAudioDevice(nullptr);
	} else if (G.Device.flags & PL_AudioDevice_IsNotPresent) {
		PL_OpenAudioDevice(nullptr);
	}
}

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

static DWORD WINAPI PL_AudioThreadThread(LPVOID param) {
	PL_InitAudioThread();

	PL_OpenAudioDevice(nullptr);

	while (1) {
		DWORD wait = WaitForMultipleObjects(PL_AudioEvent_EnumMax, G.Events, FALSE, 200);

		if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Update) {
			PL_UpdateAudioDevice();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Play) {
			PL_StartAudioDevice();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Pause) {
			PL_StopAudioDevice();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Reset) {
			PL_ResetAudioDevice();
		} else if (wait == WAIT_TIMEOUT) {
			PL_ReloadAudioDeviceIfLost();
		}
	}


	//ApBackend *backend = nullptr;
	//hr = ApBackend_Create(&backend);
	//if (FAILED(hr)) {
	//	FatalAppExitW(0, L"Failed to initialize audio device");
	//}

	//while (1) {
	//	static_assert(ApEvent_RequireUpdate == 0 && ApEvent_DeviceChanged == 1,
	//		"RequireUpdate and DeviceChanged are the only events the audio thread needs to wait for");

	//	DWORD event = WaitForMultipleObjects(2, backend->events, FALSE, INFINITE);

	//	if (event == WAIT_OBJECT_0 + ApEvent_RequireUpdate) {
	//		ApBackend_Update(backend);
	//	} else if (event == WAIT_OBJECT_0 + ApEvent_DeviceChanged) {
	//		ApDevice backup_device                 = backend->devices[ApDeviceKind_Backup];
	//		backend->devices[ApDeviceKind_Backup]  = backend->devices[ApDeviceKind_Current];
	//		backend->devices[ApDeviceKind_Current] = backup_device;
	//		SetEvent(backend->events[ApEvent_DeviceInstalled]);
	//		ApBackend_Start(backend);
	//	}
	//}

	PL_DeinitAudioThread();

	return 0;
}

//
//
//

void PL_SetAudioUpdateProc(PL_AudioUpdateProc proc, void *user) {
	G.UpdateProc = proc;
	G.UserData   = user;
}

void PL_SpawnAudioThread(PL_AudioUpdateProc proc, void *user) {
	Assert(proc != nullptr);

	if (InterlockedCompareExchange(&G.Initialized, 1, 0)) {
		PL_SetAudioUpdateProc(proc, user);
		return;
	}

	PL_SetAudioUpdateProc(proc, user);

	G.Thread = CreateThread(nullptr, 0, PL_AudioThreadThread, nullptr, 0, nullptr);
	if (!G.Thread) {
		PL_FatalError(L"Failed to create thread");
	}
}

void PL_KillAudioThread(void) {
	TerminateThread(G.Thread, 0);
	G.Thread = nullptr;

	PL_DeinitAudioThread();

	G.UpdateProc  = nullptr;
	G.UserData    = nullptr;
	G.Initialized = 0;
}

bool PL_IsAudioPlaying(void) {
	return G.Device.playing != 0;
}

void PL_PauseAudio(void) {
	ReleaseSemaphore(G.Events[PL_AudioEvent_Pause], 1, nullptr);
}

void PL_ResumeAudio(void) {
	ReleaseSemaphore(G.Events[PL_AudioEvent_Play], 1, nullptr);
}

void PL_ResetAudio(void) {
	ReleaseSemaphore(G.Events[PL_AudioEvent_Reset], 1, nullptr);
}

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

	PL_SpawnAudioThread(UpdateAudio, nullptr);

	char input[1024];

	for (;;) {
		printf("> ");
		int n = scanf_s("%s", input, (int)sizeof(input));

		if (strcmp(input, "resume") == 0) {
			PL_ResumeAudio();
		} else if (strcmp(input, "pause") == 0) {
			PL_PauseAudio();
		} else if (strcmp(input, "stop") == 0) {
			PL_ResetAudio();
			CurrentSample = 0;
		} else if (strcmp(input, "reset") == 0) {
			PL_ResetAudio();
			CurrentSample = 0;
			PL_ResumeAudio();
		}
	}

	return 0;
}
