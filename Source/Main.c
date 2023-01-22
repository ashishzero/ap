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

bool ApBackend_Failed(HRESULT hr) {
	if (SUCCEEDED(hr))
		return false;

	if (hr == AUDCLNT_E_DEVICE_INVALIDATED)
		return true;

	DWORD error    = GetLastError();
	LPWSTR message = L"Fatal Backend Error";

	if (error) {
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&message, 0, NULL);
	}

	FatalAppExitW(0, message);
	return true;
}

typedef enum ApEvent {
	ApEvent_RequireUpdate,
	ApEvent_DeviceChanged,
	ApEvent_DeviceInstalled,
	ApEvent_Max,
} ApEvent;

typedef enum ApDeviceKind {
	ApDeviceKind_Current,
	ApDeviceKind_Backup,
	ApDeviceKind_Max
} ApDeviceKind;

typedef struct ApDevice {
	IAudioClient *            client;
	IAudioRenderClient *      render;
	UINT32                    frames;
	WAVEFORMATEX *            format;
	IMMDevice *               handle;
} ApDevice;

typedef struct ApBackend {
	IMMNotificationClient     notification;
	LONG volatile             refcount;
	HANDLE                    events[ApEvent_Max];
	ApDevice                  devices[ApDeviceKind_Max];
	IMMDeviceEnumerator *     enumerator;
	IMMNotificationClientVtbl vtable;
} ApBackend;

void ApDevice_Release(ApDevice *device) {
	if (device->client) IAudioClient_Release(device->client);
	if (device->render) IAudioRenderClient_Release(device->render);
	if (device->format) CoTaskMemFree(device->format);
	if (device->handle) IMMDevice_Release(device->handle);
	memset(device, 0, sizeof(*device));
}

bool ApBackend_LoadDevice(ApBackend *backend) {
	ApDevice_Release(&backend->devices[ApDeviceKind_Backup]);

	IMMDeviceEnumerator *enumerator = backend->enumerator;
	ApDevice *          device      = &backend->devices[ApDeviceKind_Backup];

	HRESULT hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eMultimedia, &device->handle);
	if (hr == E_NOTFOUND) {
		// no audio output devices available
		return false;
	}
	if (ApBackend_Failed(hr)) return false;

	hr = IMMDevice_Activate(device->handle, &IID_IAudioClient, CLSCTX_ALL, nullptr, &device->client);
	if (ApBackend_Failed(hr)) return false;

	hr = IAudioClient_GetMixFormat(device->client, &device->format);
	if (ApBackend_Failed(hr)) return false;

	REFERENCE_TIME device_period;
	hr = IAudioClient_GetDevicePeriod(device->client, nullptr, &device_period);
	if (ApBackend_Failed(hr)) return false;

	hr = IAudioClient_Initialize(device->client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		device_period, device_period, device->format, nullptr);
	Assert(hr != AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED);
	if (ApBackend_Failed(hr)) return false;

	return true;
}

void ApBackend_Update(ApBackend *backend) {
	ApDevice *device = &backend->devices[ApDeviceKind_Current];

	UINT32 padding = 0;
	HRESULT hr = IAudioClient_GetCurrentPadding(device->client, &padding);
	if (ApBackend_Failed(hr)) return;

	UINT32 frames = device->frames - padding;
	BYTE *data    = nullptr;
	hr = IAudioRenderClient_GetBuffer(device->render, frames, &data);
	if (ApBackend_Failed(hr)) return;

	u32 written = LoadAudioSample((r32 *)data, frames, device->format->nChannels, nullptr);

	hr = IAudioRenderClient_ReleaseBuffer(device->render, written, 0);
	if (ApBackend_Failed(hr)) return;
}

void ApBackend_Start(ApBackend *backend) {
	ResetEvent(backend->events[ApEvent_RequireUpdate]);

	HRESULT hr;
	ApDevice *device = &backend->devices[ApDeviceKind_Current];

	if (!device->client) return;

	hr = IAudioClient_SetEventHandle(device->client, backend->events[ApEvent_RequireUpdate]);
	if (ApBackend_Failed(hr)) return;

	hr = IAudioClient_GetBufferSize(device->client, &device->frames);
	if (ApBackend_Failed(hr)) return;

	hr = IAudioClient_GetService(device->client, &IID_IAudioRenderClient, &device->render);
	if (ApBackend_Failed(hr)) return;

	ApBackend_Update(backend);

	hr = IAudioClient_Start(device->client);
	if (ApBackend_Failed(hr)) return;
}

HRESULT ApBackend_QueryInterface(IMMNotificationClient *_this, REFIID riid, void **ppvObject) {
	if (memcmp(&IID_IUnknown, riid, sizeof(IID_IUnknown)) == 0) {
		*ppvObject = (IUnknown *)_this;
	} else if (memcmp(&IID_IMMNotificationClient, riid, sizeof(IID_IMMNotificationClient)) == 0) {
		*ppvObject = (IMMNotificationClient *)_this;
	} else {
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	return S_OK;
}

ULONG ApBackend_AddRef(IMMNotificationClient *_this)  { 
	ApBackend *backend = (ApBackend *)_this; 
	return InterlockedIncrement(&backend->refcount);
}

ULONG ApBackend_Release(IMMNotificationClient *_this) { 
	ApBackend *backend = (ApBackend *)_this; 
	ULONG res = InterlockedDecrement(&backend->refcount);
	if (res == 0) {
		for (int i = 0; i < ApEvent_Max; ++i) {
			CloseHandle(backend->events[i]);
		}
		for (int i = 0; i < ApDeviceKind_Max; ++i) {
			ApDevice_Release(&backend->devices[i]);
		}
		IMMDeviceEnumerator_Release(backend->enumerator);
		CoTaskMemFree(_this);
	}
	return res;
}

HRESULT ApBackend_OnDeviceStateChanged(IMMNotificationClient *_this, LPCWSTR dev_id, DWORD state) {
	ApBackend *backend = (ApBackend *)_this;
	printf("%S changed: %u\n", dev_id, state);
	return S_OK;
}

HRESULT ApBackend_OnDeviceAdded(IMMNotificationClient *_this, LPCWSTR dev_id) {
	printf("%S added\n", dev_id);
	return S_OK;
}

HRESULT ApBackend_OnDeviceRemoved(IMMNotificationClient *_this, LPCWSTR dev_id) {
	printf("%S removed\n", dev_id);
	return S_OK;
}

HRESULT ApBackend_OnDefaultDeviceChanged(IMMNotificationClient *_this, EDataFlow flow, ERole role, LPCWSTR dev_id) {
	const char *flow_str = "";
	if (flow == eRender) flow_str = "render";
	else if (flow == eCapture) flow_str = "capture";
	else if (flow == eAll) flow_str = "all";

	const char *role_str = "";
	if (role == eConsole) role_str = "console";
	else if (role == eMultimedia) role_str = "multimedia";
	else if (role == eCommunications) role_str = "communications";

	printf("%S default changed. Flow: %s, Role: %s\n", dev_id, flow_str, role_str);

	return S_OK;
}

HRESULT ApBackend_OnPropertyValueChanged(IMMNotificationClient *_this, LPCWSTR dev_id, const PROPERTYKEY key) {
	return S_OK;
}

HRESULT ApBackend_Create(ApBackend **out) {
	ApBackend *backend = CoTaskMemAlloc(sizeof(*backend));
	if (!backend) {
		return E_OUTOFMEMORY;
	}

	memset(backend, 0, sizeof(*backend));

	backend->notification.lpVtbl = &backend->vtable;
	backend->vtable = (IMMNotificationClientVtbl) {
		.AddRef                 = ApBackend_AddRef,
		.Release                = ApBackend_Release,
		.QueryInterface         = ApBackend_QueryInterface,
		.OnDeviceStateChanged   = ApBackend_OnDeviceStateChanged,
		.OnDeviceAdded          = ApBackend_OnDeviceAdded,
		.OnDeviceRemoved        = ApBackend_OnDeviceRemoved,
		.OnDefaultDeviceChanged = ApBackend_OnDefaultDeviceChanged,
		.OnPropertyValueChanged = ApBackend_OnPropertyValueChanged
	};

	backend->refcount = 1;

	HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &backend->enumerator);
	if (FAILED(hr)) {
		CoTaskMemFree(backend);
		return hr;
	}

	hr = IMMDeviceEnumerator_RegisterEndpointNotificationCallback(backend->enumerator, &backend->notification);
	if (FAILED(hr)) {
		IMMDeviceEnumerator_Release(backend->enumerator);
		CoTaskMemFree(backend);
		return hr;
	}

	for (int i = 0; i < ApEvent_Max; ++i) {
		backend->events[i] = CreateEventW(nullptr, FALSE, FALSE, nullptr);
		if (backend->events[i] == INVALID_HANDLE_VALUE) {
			for (int j = i - 1; j >= 0; ++j)
				CloseHandle(backend->events[i]);
			IMMDeviceEnumerator_Release(backend->enumerator);
			CoTaskMemFree(backend);
		}
	}

	if (ApBackend_LoadDevice(backend)) {
		backend->devices[ApDeviceKind_Current] = backend->devices[ApDeviceKind_Backup];
		backend->devices[ApDeviceKind_Backup]  = (ApDevice){0};
		ApBackend_Start(backend);
	}

	*out = backend;

	return S_OK;
}

DWORD WINAPI AudioThreadProc(LPVOID param) {
	HRESULT hr;

	DWORD task_index;
	AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (hr != S_OK && hr != S_FALSE && hr != RPC_E_CHANGED_MODE) {
		FatalAppExitW(0, L"Failed to initialize audio device");
	}

	ApBackend *backend = nullptr;
	hr = ApBackend_Create(&backend);
	if (FAILED(hr)) {
		FatalAppExitW(0, L"Failed to initialize audio device");
	}

	while (1) {
		static_assert(ApEvent_RequireUpdate == 0 && ApEvent_DeviceChanged == 1,
			"RequireUpdate and DeviceChanged are the only events the audio thread needs to wait for");

		DWORD event = WaitForMultipleObjects(2, backend->events, FALSE, INFINITE);

		if (event == WAIT_OBJECT_0 + ApEvent_RequireUpdate) {
			ApBackend_Update(backend);
		} else if (event == WAIT_OBJECT_0 + ApEvent_DeviceChanged) {
			ApDevice backup_device                 = backend->devices[ApDeviceKind_Backup];
			backend->devices[ApDeviceKind_Backup]  = backend->devices[ApDeviceKind_Current];
			backend->devices[ApDeviceKind_Current] = backup_device;
			SetEvent(backend->events[ApEvent_DeviceInstalled]);
			ApBackend_Start(backend);
		}
	}

	ApBackend_Release(&backend->notification);
	CoUninitialize();

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

	HANDLE thread = CreateThread(nullptr, 0, AudioThreadProc, nullptr, 0, nullptr);

	WaitForSingleObject(thread, INFINITE);

	return 0;
}
