#include <stdio.h>

#include "KrPlatform.h"

typedef enum KrAudioFormat {
	KrAudioFormat_R32,
	KrAudioFormat_I32,
	KrAudioFormat_I16,
	KrAudioFormat_EnumMax
} KrAudioFormat;

typedef struct KrAudioSpec {
	KrAudioFormat Format;
	u32           Channels;
	u32           Frequency;
} KrAudioSpec;


#pragma warning(push)
#pragma warning(disable: 5105)

#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Avrt.lib")

#pragma warning(pop)

DEFINE_GUID(KR_CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);

DEFINE_GUID(KR_IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(KR_IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(KR_IID_IMMNotificationClient, 0x7991eec9, 0x7e89, 0x4d85, 0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0);
DEFINE_GUID(KR_IID_IMMEndpoint, 0x1BE09788, 0x6894, 0x4089, 0x85, 0x86, 0x9A, 0x2A, 0x6C, 0x26, 0x5A, 0xC5);
DEFINE_GUID(KR_IID_IAudioClient, 0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(KR_IID_IAudioRenderClient, 0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);
DEFINE_GUID(KR_KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(KR_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(KR_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX, 0x00000000L, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);

//
// [[Atomic]]
//

static void PL_AtomicLock(volatile LONG *lock) {
	while (InterlockedCompareExchange(lock, 1, 0) == 1) {
	}
}

static void PL_AtomicUnlock(volatile LONG *lock) {
	InterlockedExchange(lock, 0);
}

//
// [Unicode]
//

int PL_UTF16ToUTF8(char *utf8_buff, int utf8_buff_len, const char16_t *utf16_string) {
	int bytes_written = WideCharToMultiByte(CP_UTF8, 0, utf16_string, -1, utf8_buff, utf8_buff_len, 0, 0);
	return bytes_written;
}

//
// [[Audio]]
// 

static HRESULT STDMETHODCALLTYPE PL_QueryInterface(IMMNotificationClient *This, REFIID riid, void **ppv_object);
static ULONG   STDMETHODCALLTYPE PL_AddRef(IMMNotificationClient *This);
static ULONG   STDMETHODCALLTYPE PL_Release(IMMNotificationClient *This);
static HRESULT STDMETHODCALLTYPE PL_OnDeviceStateChanged(IMMNotificationClient *This, LPCWSTR deviceid, DWORD newstate);
static HRESULT STDMETHODCALLTYPE PL_OnDeviceAdded(IMMNotificationClient *This, LPCWSTR deviceid);
static HRESULT STDMETHODCALLTYPE PL_OnDeviceRemoved(IMMNotificationClient *This, LPCWSTR deviceid);
static HRESULT STDMETHODCALLTYPE PL_OnDefaultDeviceChanged(IMMNotificationClient *This, EDataFlow flow, ERole role, LPCWSTR deviceid);
static HRESULT STDMETHODCALLTYPE PL_OnPropertyValueChanged(IMMNotificationClient *This, LPCWSTR deviceid, const PROPERTYKEY key);

typedef enum KrAudioDeviceFlow {
	KrAudioDeviceFlow_Render,
	KrAudioDeviceFlow_Capture,
	KrAudioDeviceFlow_EnumMax
} KrAudioDeviceFlow;

typedef struct PL_KrDeviceName {
	char Buffer[64];
} PL_KrDeviceName;

typedef struct PL_AudioDevice {
	struct PL_AudioDevice *Next;
	char *                Name;
	KrAudioDeviceFlow     Flow;
	volatile u32          Active;
	LPWSTR                Id;
	PL_KrDeviceName       Names[2];
} PL_AudioDevice;

typedef PL_AudioDevice KrAudioDevice;

typedef enum PL_AudioEvent {
	PL_AudioEvent_Update,
	PL_AudioEvent_Resume,
	PL_AudioEvent_Pause,
	PL_AudioEvent_Reset,
	PL_AudioEvent_Restart,
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
static PL_AudioDevice        g_AudioDeviceListHead = { .Name = "Default", .Flow = KrAudioDeviceFlow_EnumMax };
static volatile LONG         g_AudioDeviceListLock;

static struct {
	IAudioClient        * Client;
	IAudioRenderClient  * RenderClient;
	KrAudioSpec           Spec;
	UINT                  MaxFrames;
	HANDLE                Events[PL_AudioEvent_EnumMax];
	volatile LONG         DeviceIsLost;
	volatile LONG         DeviceIsResumed;
	PL_AudioDevice      * EffectiveAudioDevice;
	PL_AudioDevice      * DesiredAudioDevice;
	HANDLE                Thread;
} g_AudioOut;

//
//
//

static IMMDevice *PL_GetDefaultIMMDeviceForRendering() {
	IMMDevice *immdevice = nullptr;
	HRESULT hr = g_AudioDeviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(g_AudioDeviceEnumerator, eRender, eMultimedia, &immdevice);
	if (SUCCEEDED(hr)) {
		return immdevice;
	}
	Unimplemented();
	return nullptr;
}

static IMMDevice *PL_IMMDeviceFromId(LPCWSTR id) {
	IMMDevice *immdevice = nullptr;
	HRESULT hr = g_AudioDeviceEnumerator->lpVtbl->GetDevice(g_AudioDeviceEnumerator, id, &immdevice);
	if (SUCCEEDED(hr)) {
		return immdevice;
	}
	Unimplemented();
	return nullptr;
}

static LPWSTR PL_IMMDeviceId(IMMDevice *immdevice) {
	LPWSTR  id = nullptr;
	HRESULT hr = immdevice->lpVtbl->GetId(immdevice, &id);
	if (FAILED(hr)) {
		Unimplemented();
	}
	return id;
}

static u32 PL_IMMDeviceActive(IMMDevice *immdevice) {
	u32 flags   = 0;
	DWORD state = 0;
	HRESULT hr  = immdevice->lpVtbl->GetState(immdevice, &state);
	if (FAILED(hr)) {
		Unimplemented();
	}
	return state == DEVICE_STATE_ACTIVE;
}

static KrAudioDeviceFlow PL_IMMDeviceFlow(IMMDevice *immdevice) {
	IMMEndpoint *endpoint = nullptr;
	HRESULT hr = immdevice->lpVtbl->QueryInterface(immdevice, &KR_IID_IMMEndpoint, &endpoint);
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

static PL_KrDeviceName PL_IMMDeviceName(IMMDevice *immdevice) {
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

static PL_AudioDevice *PL_UpdateAudioDeviceList(IMMDevice *immdevice) {
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

static PL_AudioDevice *PL_FindAudioDeviceFromId(LPCWSTR id) {
	PL_AudioDevice *device = (PL_AudioDevice *)g_AudioDeviceListHead.Next;
	for (;device; device = (PL_AudioDevice *)device->Next) {
		if (wcscmp(id, device->Id) == 0)
			return device;
	}
	return nullptr;
}

static HRESULT STDMETHODCALLTYPE PL_QueryInterface(IMMNotificationClient *This, REFIID riid, void **ppv_object) {
	if (memcmp(&KR_IID_IUnknown, riid, sizeof(GUID)) == 0) {
		*ppv_object = (IUnknown *)This;
	} else if (memcmp(&KR_IID_IMMNotificationClient, riid, sizeof(GUID)) == 0) {
		*ppv_object = (IMMNotificationClient *)This;
	} else {
		*ppv_object = NULL;
		return E_NOINTERFACE;
	}
	return S_OK;
}

static ULONG STDMETHODCALLTYPE PL_AddRef(IMMNotificationClient *This) {
	return 1;
}

static ULONG STDMETHODCALLTYPE PL_Release(IMMNotificationClient *This) {
	return 1;
}

static HRESULT STDMETHODCALLTYPE PL_OnDeviceStateChanged(IMMNotificationClient *This, LPCWSTR deviceid, DWORD newstate) {
	PL_AtomicLock(&g_AudioDeviceListLock);

	PL_AudioDevice *device = PL_FindAudioDeviceFromId(deviceid);
	if (device) {
		u32 active = (newstate == DEVICE_STATE_ACTIVE);
		InterlockedExchange(&device->Active, active);
	} else {
		// New device added
		IMMDevice *immdevice = PL_IMMDeviceFromId(deviceid);
		if (immdevice) {
			PL_UpdateAudioDeviceList(immdevice);
			immdevice->lpVtbl->Release(immdevice);
		}
	}

	// TODO: events

	PL_AtomicUnlock(&g_AudioDeviceListLock);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE PL_OnDeviceAdded(IMMNotificationClient *This, LPCWSTR deviceid) {
	// Handled in "OnDeviceStateChanged"
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE PL_OnDeviceRemoved(IMMNotificationClient *This, LPCWSTR deviceid) {
	// Handled in "OnDeviceStateChanged"
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE PL_OnDefaultDeviceChanged(IMMNotificationClient *This, EDataFlow flow, ERole role, LPCWSTR deviceid) {
	PL_AtomicLock(&g_AudioDeviceListLock);

	if (flow == eRender && role == eMultimedia) {
		if (g_AudioOut.DesiredAudioDevice == &g_AudioDeviceListHead) {
			ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_Restart], 1, 0);
		}
	}

	PL_AtomicUnlock(&g_AudioDeviceListLock);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE PL_OnPropertyValueChanged(IMMNotificationClient *This, LPCWSTR deviceid, const PROPERTYKEY key) {
	if (memcmp(&key, &PKEY_Device_FriendlyName, sizeof(PROPERTYKEY)) == 0) {
		PL_AtomicLock(&g_AudioDeviceListLock);

		// TODO: events

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

		PL_AtomicUnlock(&g_AudioDeviceListLock);
	}
	return S_OK;
}

static bool PL_SetAudioSpec(WAVEFORMATEX *wave_format) {
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
		if ((memcmp(&ext->SubFormat, &KR_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 32)) {
			g_AudioOut.Spec.Format = KrAudioFormat_R32;
		} else if ((memcmp(&ext->SubFormat, &KR_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 32)) {
			g_AudioOut.Spec.Format = KrAudioFormat_I32;
		} else if ((memcmp(&ext->SubFormat, &KR_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) &&
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

static void PL_ReleaseAudioRenderDevice() {
	if (g_AudioOut.Client) {
		g_AudioOut.Client->lpVtbl->Release(g_AudioOut.Client);
		g_AudioOut.Client = nullptr;
	}
	if (g_AudioOut.RenderClient) {
		g_AudioOut.RenderClient->lpVtbl->Release(g_AudioOut.RenderClient);
		g_AudioOut.RenderClient = nullptr;
	}
	g_AudioOut.MaxFrames = 0;

	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
}

static bool PL_SetupAudioRenderDevice(PL_AudioDevice *device) {
	WAVEFORMATEX *wave_format = nullptr;

	IMMDevice *immdevice = device->Id ? PL_IMMDeviceFromId(device->Id) : PL_GetDefaultIMMDeviceForRendering();
	if (!immdevice) return false;

	HRESULT hr = immdevice->lpVtbl->Activate(immdevice, &KR_IID_IAudioClient, CLSCTX_ALL, nullptr, &g_AudioOut.Client);
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

	hr = g_AudioOut.Client->lpVtbl->GetService(g_AudioOut.Client, &KR_IID_IAudioRenderClient, &g_AudioOut.RenderClient);
	if (FAILED(hr)) goto failed;

	immdevice->lpVtbl->Release(immdevice);

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

static bool PL_GetAudioBuffer(BYTE **data, UINT *size) {
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

static void PL_ReleaseAudioBuffer(u32 written, u32 frames) {
	DWORD flags = 0;
	if (written < frames)
		flags |= AUDCLNT_BUFFERFLAGS_SILENT;

	HRESULT hr = g_AudioOut.RenderClient->lpVtbl->ReleaseBuffer(g_AudioOut.RenderClient, written, flags);
	if (SUCCEEDED(hr))
		return;

	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
	Unimplemented();
}

u32 PL_LoadAudioFrames(KrAudioDevice *device, KrAudioSpec *spec, u8 *dst, u32 count, void *user); // todo: cleanup

static void PL_UpdateAudio() {
	BYTE *data = nullptr;
	u32 frames = 0;
	if (PL_GetAudioBuffer(&data, &frames)) {
		// todo: call update things

		u32 written = PL_LoadAudioFrames(g_AudioOut.DesiredAudioDevice, &g_AudioOut.Spec, data, frames, nullptr);

		PL_ReleaseAudioBuffer(written, frames);
	}
}

static void PL_ResumeAudio() {
	InterlockedExchange(&g_AudioOut.DeviceIsResumed, 1);

	HRESULT hr = g_AudioOut.Client->lpVtbl->Start(g_AudioOut.Client);
	if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED)
		return;
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
	Unimplemented();
}

static void PL_PauseAudio() {
	InterlockedExchange(&g_AudioOut.DeviceIsResumed, 0);

	HRESULT hr = g_AudioOut.Client->lpVtbl->Stop(g_AudioOut.Client);
	if (SUCCEEDED(hr))
		return;
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
	Unimplemented();
}

static void PL_ResetAudio() {
	PL_PauseAudio();
	HRESULT hr = g_AudioOut.Client->lpVtbl->Reset(g_AudioOut.Client);
	if (SUCCEEDED(hr))
		return;
	InterlockedExchange(&g_AudioOut.DeviceIsLost, 1);
}

static DWORD WINAPI PL_AudioThread(LPVOID param) {
	HANDLE thread = GetCurrentThread();
	SetThreadDescription(thread, L"PL Audio Thread");

	DWORD task_index;
	HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	if (!g_AudioOut.DesiredAudioDevice) {
		g_AudioOut.DesiredAudioDevice   = &g_AudioDeviceListHead;
		g_AudioOut.EffectiveAudioDevice = g_AudioOut.DesiredAudioDevice;
	}

	if (PL_SetupAudioRenderDevice(g_AudioOut.DesiredAudioDevice)) {
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
		} else if (wait == WAIT_OBJECT_0 + PL_AudioEvent_Restart) {
			PL_ReleaseAudioRenderDevice();
			if (PL_SetupAudioRenderDevice(g_AudioOut.DesiredAudioDevice)) {
				// todo: swapping event
				PL_UpdateAudio();
				if (g_AudioOut.DeviceIsResumed) {
					PL_ResumeAudio();
				}
			}
		}

		// todo: check for default device changes, device lost and device gained and events
	}

	AvRevertMmThreadCharacteristics(&avrt);

	return 0;
}


//
//
//

static void ShutdownAudio() {
	if (g_AudioOut.Thread)
		TerminateThread(g_AudioOut.Thread, 0);

	g_AudioOut.DeviceIsResumed = 0;
	g_AudioOut.DeviceIsLost    = 0;
	PL_ReleaseAudioRenderDevice();

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

static void InitializeAudio() {
	HRESULT hr = CoCreateInstance(&KR_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &KR_IID_IMMDeviceEnumerator, &g_AudioDeviceEnumerator);
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

		PL_AtomicLock(&g_AudioDeviceListLock);

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

		PL_AtomicUnlock(&g_AudioDeviceListLock);
	}

	g_AudioOut.Thread = CreateThread(nullptr, 0, PL_AudioThread, nullptr, 0, 0);
	if (!g_AudioOut.Thread) {
		Unimplemented();
	}
}

//
//
//

static void Shutdown() {
	ShutdownAudio();
	CoUninitialize();
}

static void Initialize() {
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	Assert(hr != RPC_E_CHANGED_MODE);

	if (hr != S_OK && hr != S_FALSE) {
		Unimplemented(); // Handle error
	}

	InitializeAudio();
}

//
//
//

typedef enum KrEventKind {
	KrEvent_AudioDeviceAdded,
	KrEvent_AudioDeviceRemoved,
	KrEvent_AudioDeviceEnabled,
	KrEvent_AudioDeviceDisabled,
	KrEvent_EnumCount
} KrEventKind;

static const char *KrEventEnumName[] = {
	[KrEvent_AudioDeviceAdded]    = "AudioDeviceAdded",
	[KrEvent_AudioDeviceRemoved]  = "AudioDeviceRemoved",
	[KrEvent_AudioDeviceEnabled]  = "AudioDeviceEnabled",
	[KrEvent_AudioDeviceDisabled] = "AudioDeviceDisabled",
};

static_assert(ArrayCount(KrEventEnumName) == KrEvent_EnumCount, "");

typedef struct KrAudioEvent {
	PL_AudioDevice *Device;
} KrAudioEvent;

typedef struct KrEvent {
	KrEventKind Kind;
	union {
		KrAudioEvent Audio;
	} Data;
} KrEvent;

enum KrAudioDeviceIterFlagBits {
	KrAudioDeviceIter_Inactive    = 0x1,
	KrAudioDeviceIter_RenderFlow  = 0x2,
	KrAudioDeviceIter_CaptureFlow = 0x4,
};

typedef struct KrAudioDeviceIter {
	void *Data;
	u32   Flags;
} KrAudioDeviceIter;

void KrAudioDeviceIterInit(KrAudioDeviceIter *iter, u32 flags) {
	iter->Data  = g_AudioDeviceListHead.Next;
	iter->Flags = flags;
	if ((flags & (KrAudioDeviceIter_RenderFlow | KrAudioDeviceIter_CaptureFlow)) == 0) {
		flags |= KrAudioDeviceIter_RenderFlow | KrAudioDeviceIter_CaptureFlow;
	}
}

PL_AudioDevice *KrAudioDeviceIterNext(KrAudioDeviceIter *iter) {
	bool inactive = iter->Flags & KrAudioDeviceIter_Inactive;
	bool render   = iter->Flags & KrAudioDeviceIter_RenderFlow;
	bool capture  = iter->Flags & KrAudioDeviceIter_CaptureFlow;

	PL_AudioDevice *device = iter->Data;
	for (; device; device = device->Next) {
		if (device->Active || inactive) {
			if ((device->Flow == KrAudioDeviceFlow_Render && render) ||
				(device->Flow == KrAudioDeviceFlow_Capture && capture)) {
				iter->Data = device->Next;
				return device;
			}
		}
	}

	return nullptr;
}

bool KrIsAudioPlaying() {
	LONG lost = InterlockedOr(&g_AudioOut.DeviceIsLost, 0);
	if (lost) return false;
	return InterlockedOr(&g_AudioOut.DeviceIsResumed, 0);
}

void KrUpdateAudio() {
	if (!InterlockedOr(&g_AudioOut.DeviceIsResumed, 0)) {
		SetEvent(g_AudioOut.Events[PL_AudioEvent_Update]);
	}
}

void KrResumeAudio() {
	ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_Resume], 1, 0);
}

void KrPauseAudio() {
	ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_Pause], 1, 0);
}

void KrResetAudio() {
	ReleaseSemaphore(g_AudioOut.Events[PL_AudioEvent_Reset], 1, 0);
}

void ListAudioDevices() {
	KrAudioDeviceIter iter;
	PL_AudioDevice *device;

	printf("Render devices\n");
	printf("Current: %s\n", g_AudioOut.EffectiveAudioDevice->Name);
	KrAudioDeviceIterInit(&iter, KrAudioDeviceIter_RenderFlow);
	while (device = KrAudioDeviceIterNext(&iter)) {
		printf(" [%c] %s\n", device->Active ? '*' : ' ', device->Name);
	}

	printf("Capture devices\n");
	KrAudioDeviceIterInit(&iter, KrAudioDeviceIter_CaptureFlow);
	while (device = KrAudioDeviceIterNext(&iter)) {
		printf(" [%c] %s\n", device->Active ? '*' : ' ', device->Name);
	}
}

//int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev_instance, PWSTR args, int show) {
//	Initialize();
//
//	AllocConsole();
//
//	freopen("CONOUT$", "w", stdout);
//	freopen("CONOUT$", "w", stderr);
//	freopen("CONIN$", "r", stdin);
//
//
//	printf("thing\n");
//}

static uint MaxSample     = 0;
static uint CurrentSample = 0;
static i16 *CurrentStream = 0;
static real Volume        = 0.5f;

u32 PL_LoadAudioFrames(KrAudioDevice *device, KrAudioSpec *spec, u8 *dst, u32 count, void *user) {
	Assert(spec->Format == KrAudioFormat_R32 && spec->Channels == 2);

	umem size = count * spec->Channels;

	r32 *samples  = (r32 *)dst;
	for (uint i = 0; i < size; i += spec->Channels) {
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

string ReadEntireFile(const char *path) {
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
	return (string){ .count=size, .data=data };
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

int main(int argc, char *argv[]) {
	if (argc <= 1){
		Usage(argv[0]);
	}

	const char *path = argv[1];

	string buff = ReadEntireFile(path);
	Audio_Stream *audio = (Audio_Stream *)buff.data;

	CurrentStream = Serialize(audio, &MaxSample);

	Initialize();

	KrUpdateAudio();
	KrResumeAudio();

	char input[1024];

	for (;;) {
		printf("> ");
		if (!gets_s(input, sizeof(input)))
			continue;

		if (strcmp(input, "resume") == 0) {
			KrResumeAudio();
		} else if (strcmp(input, "pause") == 0) {
			KrPauseAudio();
		} else if (strcmp(input, "stop") == 0) {
			KrResetAudio();
			CurrentSample = 0;
		} else if (strcmp(input, "reset") == 0) {
			KrResetAudio();
			CurrentSample = 0;
			KrUpdateAudio();
			KrResumeAudio();
		} else if (strcmp(input, "quit") == 0) {
			break;
		} else if (strcmp(input, "list") == 0) {
			ListAudioDevices();
		} else {
			printf("invalid command\n");
		}
	}

	return 0;
}
