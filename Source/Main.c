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


DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioClient, 0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioRenderClient, 0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);
DEFINE_GUID(IID_IMMNotificationClient, 0x7991eec9, 0x7e89, 0x4d85, 0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0);

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

u32 LoadAudioSample(r32 *samples, u32 count, u32 channels, void *user_data) {
	umem size = count * channels;

	for (uint i = 0; i < size; i += channels) {
		float l = (float)CurrentStream[CurrentSample + 0] / 32767.0f;
		float r = (float)CurrentStream[CurrentSample + 1] / 32767.0f;

		samples[i + 0] = Volume * l;
		samples[i + 1] = Volume * r;

		CurrentSample += 2;

		if (CurrentSample >= MaxSample)
			CurrentSample = 0;
	}

	return count;
}

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
	PL_AudioDevice_IsLost   = 0x1,
	PL_AudioDevice_IsNotPresent = 0x2
};

typedef struct PL_AudioDevice {
	u32                 flags;
	u32                 frames;
	IAudioClient *      client;
	IAudioRenderClient *render;
	WAVEFORMATEX *      format;
	char16_t *          id;
} PL_AudioDevice;

static IMMDeviceEnumerator *DeviceEnumerator = nullptr;
static volatile LONG        Initialized      = 0;
static PL_AudioDevice       Device;

static void PL_InitAudioThread(void) {
	DWORD task_index;
	AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (hr != S_OK && hr != S_FALSE && hr != RPC_E_CHANGED_MODE) {
		FatalAppExitW(0, L"Failed to initialize audio thread");
	}

	LONG val = InterlockedCompareExchange(&Initialized, 1, 0);

	if (val == 0) {
		hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &DeviceEnumerator);
		if (FAILED(hr)) {
			FatalAppExitW(0, L"Failed to initialize audio thread");
		}
	} else {
		IMMDeviceEnumerator_AddRef(DeviceEnumerator);
	}
}

static void PL_DeinitAudioThread(void) {
	if (IMMDeviceEnumerator_Release(DeviceEnumerator) == 0) {
		InterlockedCompareExchange(&Initialized, 0, 1);
	}
	CoUninitialize();
}

static void PL_CloseAudioDevice(void) {
	if (Device.client) IAudioClient_Release(Device.client);
	if (Device.render) IAudioRenderClient_Release(Device.render);
	if (Device.format) CoTaskMemFree(Device.format);
	if (Device.id)     CoTaskMemFree(Device.id);

	memset(&Device, 0, sizeof(Device));
}

static void PL_StartAudioDevice(void) {
	HRESULT hr = IAudioClient_Start(Device.client);
	if (PL_AudioFailed(hr)) {
		Device.flags |= PL_AudioDevice_IsLost;
	}
}

static void PL_StopAudioDevice(void) {
	HRESULT hr = IAudioClient_Stop(Device.client);
	if (PL_AudioFailed(hr)) {
		Device.flags |= PL_AudioDevice_IsLost;
	}
}

static void PL_ResetAudioDevice(void) {
	HRESULT hr = IAudioClient_Reset(Device.client);
	if (PL_AudioFailed(hr)) {
		Device.flags |= PL_AudioDevice_IsLost;
	}
}

static void PL_UpdateAudioDevice(void) {
	UINT32 padding = 0;
	HRESULT hr = IAudioClient_GetCurrentPadding(Device.client, &padding);
	if (PL_AudioFailed(hr)) goto failed;

	UINT32 frames = Device.frames - padding;
	BYTE *data    = nullptr;
	hr = IAudioRenderClient_GetBuffer(Device.render, frames, &data);
	if (PL_AudioFailed(hr)) goto failed;

	u32 written = LoadAudioSample((r32 *)data, frames, Device.format->nChannels, nullptr);

	hr = IAudioRenderClient_ReleaseBuffer(Device.render, written, 0);
	if (PL_AudioFailed(hr)) goto failed;

	return;

failed:
	Device.flags |= PL_AudioDevice_IsLost;
}

static void PL_OpenAudioDevice(char16_t *id, HANDLE event) {
	HRESULT hr;

	IMMDevice *endpoint = nullptr;

	if (id) {
		hr = IMMDeviceEnumerator_GetDevice(DeviceEnumerator, id, &endpoint);
		if (hr != E_NOTFOUND) {
			if (PL_Failed(hr)) {
				Device.flags |= PL_AudioDevice_IsLost;
				return;
			}
		}
	}

	if (!endpoint) {
		hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(DeviceEnumerator, eRender, eMultimedia, &endpoint);
		if (hr == E_NOTFOUND) {
			Device.flags |= PL_AudioDevice_IsNotPresent;
			return;
		}
	}

	if (PL_Failed(hr)) {
		Device.flags |= PL_AudioDevice_IsNotPresent;
		return;
	}

	Device.flags = 0;

	IMMDevice_GetId(endpoint, &Device.id);

	hr = IMMDevice_Activate(endpoint, &IID_IAudioClient, CLSCTX_ALL, nullptr, &Device.client);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_GetMixFormat(Device.client, &Device.format);
	if (PL_AudioFailed(hr)) goto failed;

	REFERENCE_TIME device_period;
	hr = IAudioClient_GetDevicePeriod(Device.client, nullptr, &device_period);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_Initialize(Device.client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		device_period, device_period, Device.format, nullptr);
	Assert(hr != AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_SetEventHandle(Device.client, event);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_GetBufferSize(Device.client, &Device.frames);
	if (PL_AudioFailed(hr)) goto failed;

	hr = IAudioClient_GetService(Device.client, &IID_IAudioRenderClient, &Device.render);
	if (PL_AudioFailed(hr)) goto failed;

	IMMDevice_Release(endpoint);

	PL_UpdateAudioDevice();
	PL_StartAudioDevice();

	return;

failed:
	PL_CloseAudioDevice();
	IMMDevice_Release(endpoint);

	Device.flags |= PL_AudioDevice_IsLost;

	return;
}

static void PL_ReloadAudioDeviceIfLost(HANDLE event) {
	if (Device.flags & PL_AudioDevice_IsLost) {
		PL_CloseAudioDevice();
		PL_OpenAudioDevice(nullptr, event);
	} else if (Device.flags & PL_AudioDevice_IsNotPresent) {
		PL_OpenAudioDevice(nullptr, event);
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

DWORD WINAPI ApRenderThreadProc(LPVOID param) {
	PL_InitAudioThread();

	HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (event == INVALID_HANDLE_VALUE) {
		PL_FatalError(L"Failed to start audio thread");
	}

	PL_OpenAudioDevice(nullptr, event);

	while (1) {
		DWORD wait = WaitForSingleObject(event, 200);

		if (wait == WAIT_OBJECT_0) {
			PL_UpdateAudioDevice();
		} else {
			PL_ReloadAudioDeviceIfLost(event);
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

int main(int argc, char *argv[]) {
	if (argc <= 1){
		Usage(argv[0]);
	}

	const char *path = argv[1];

	String buff = ReadEntireFile(path);
	Audio_Stream *audio = (Audio_Stream *)buff.data;

	CurrentStream = Serialize(audio, &MaxSample);

	HANDLE thread = CreateThread(nullptr, 0, ApRenderThreadProc, nullptr, 0, nullptr);

	WaitForSingleObject(thread, INFINITE);

	return 0;
}
