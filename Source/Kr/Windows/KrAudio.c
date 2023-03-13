#include "KrMain.h"

#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>

typedef enum PL_AudioCommand {
	PL_AudioCommand_Update,
	PL_AudioCommand_Resume,
	PL_AudioCommand_Pause,
	PL_AudioCommand_Reset,
	PL_AudioCommand_Switch,
	PL_AudioCommand_Terminate,
	PL_AudioCommand_EnumCount
} PL_AudioCommand;

typedef struct PL_AudioEndpoint {
	IAudioClient         * Client;
	IAudioRenderClient   * Render;
	IAudioCaptureClient  * Capture;
	UINT                   MaxFrames;
	PL_AudioSpec           Spec;
	HANDLE                 Commands[PL_AudioCommand_EnumCount];
	LONG volatile          DeviceLost;
	LONG volatile          Resumed;
	HANDLE                 Thread;
	PL_AudioDeviceNative * DesiredDevice;
	PL_AudioDeviceNative * EffectiveDevice;
} PL_AudioEndpoint;

static struct {
	i32 volatile           Lock;
	PL_AudioDeviceNative   FirstDevice;
	IMMDeviceEnumerator  * DeviceEnumerator;
	PL_AudioEndpoint       Endpoints[PL_AudioEndpoint_EnumCount];
	DWORD                  ParentThreadId;
} Audio;

DEFINE_GUID(PL_CLSID_MMDeviceEnumerator,0xbcde0395,0xe52f,0x467c,0x8e,0x3d,0xc4,0x57,0x92,0x91,0x69,0x2e);
DEFINE_GUID(PL_IID_IUnknown,0x00000000,0x0000,0x0000,0xC0,0x00,0x00,0x00,0x00,0x00,0x00,0x46);
DEFINE_GUID(PL_IID_IMMDeviceEnumerator,0xa95664d2,0x9614,0x4f35,0xa7,0x46,0xde,0x8d,0xb6,0x36,0x17,0xe6);
DEFINE_GUID(PL_IID_IMMNotificationClient,0x7991eec9,0x7e89,0x4d85,0x83,0x90,0x6c,0x70,0x3c,0xec,0x60,0xc0);
DEFINE_GUID(PL_IID_IMMEndpoint,0x1BE09788,0x6894,0x4089,0x85,0x86,0x9A,0x2A,0x6C,0x26,0x5A,0xC5);
DEFINE_GUID(PL_IID_IAudioClient,0x1cb9ad4c,0xdbfa,0x4c32,0xb1,0x78,0xc2,0xf5,0x68,0xa7,0x03,0xb2);
DEFINE_GUID(PL_IID_IAudioRenderClient,0xf294acfc,0x3146,0x4483,0xa7,0xbf,0xad,0xdc,0xa7,0xc2,0x60,0xe2);
DEFINE_GUID(PL_IID_IAudioCaptureClient,0xc8adbd64,0xe71e,0x48a0,0xa4,0xde,0x18,0x5c,0x39,0x5c,0xd3,0x17);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_PCM,0x00000001,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT,0x00000003,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);
DEFINE_GUID(PL_KSDATAFORMAT_SUBTYPE_WAVEFORMATEX,0x00000000L,0x0000,0x0010,0x80,0x00,0x00,0xaa,0x00,0x38,0x9b,0x71);

HRESULT STDMETHODCALLTYPE PL_QueryInterface(IMMNotificationClient *, REFIID, void **);
ULONG   STDMETHODCALLTYPE PL_AddRef(IMMNotificationClient *);
ULONG   STDMETHODCALLTYPE PL_Release(IMMNotificationClient *);
HRESULT STDMETHODCALLTYPE PL_OnDeviceStateChanged(IMMNotificationClient *, LPCWSTR, DWORD);
HRESULT STDMETHODCALLTYPE PL_OnDeviceAdded(IMMNotificationClient *, LPCWSTR);
HRESULT STDMETHODCALLTYPE PL_OnDeviceRemoved(IMMNotificationClient *, LPCWSTR);
HRESULT STDMETHODCALLTYPE PL_OnDefaultDeviceChanged(IMMNotificationClient *, EDataFlow, ERole, LPCWSTR);
HRESULT STDMETHODCALLTYPE PL_OnPropertyValueChanged(IMMNotificationClient *, LPCWSTR, const PROPERTYKEY);

HRESULT STDMETHODCALLTYPE PL_QueryInterface(IMMNotificationClient *This, REFIID riid, void **ppv_object) {
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

ULONG STDMETHODCALLTYPE PL_AddRef(IMMNotificationClient *This)  { return 1; }
ULONG STDMETHODCALLTYPE PL_Release(IMMNotificationClient *This) { return 1; }

static PL_AudioDeviceNative *PL_FindAudioDeviceNative(LPCWSTR id) {
	PL_AudioDeviceNative *device = Audio.FirstDevice.Next;
	for (; device; device = device->Next) {
		if (wcscmp(device->Id, id) == 0)
			return device;
	}
	return nullptr;
}

static PL_AudioDeviceNative *PL_AddAudioDeviceNative(IMMDevice *immdevice) {
	HRESULT hr                   = S_OK;
	DWORD state                  = 0;
	LPWSTR id                    = nullptr;
	IMMEndpoint *endpoint        = nullptr;
	PL_AudioDeviceNative *native = nullptr;

	hr = immdevice->lpVtbl->GetState(immdevice, &state);
	if (FAILED(hr)) goto failed;

	hr = immdevice->lpVtbl->GetId(immdevice, &id);
	if (FAILED(hr)) goto failed;

	hr = immdevice->lpVtbl->QueryInterface(immdevice, &PL_IID_IMMEndpoint, &endpoint);
	if (FAILED(hr)) goto failed;

	EDataFlow flow = eRender;
	hr = endpoint->lpVtbl->GetDataFlow(endpoint, &flow);
	if (FAILED(hr)) goto failed;
	endpoint->lpVtbl->Release(endpoint);

	native = CoTaskMemAlloc(sizeof(PL_AudioDeviceNative));
	if (FAILED(hr)) goto failed;

	memset(native, 0, sizeof(*native));

	native->Next      = Audio.FirstDevice.Next;
	native->IsActive  = (state == DEVICE_STATE_ACTIVE);
	native->IsCapture = (flow == eCapture);
	native->Id        = id;

	IPropertyStore *prop = nullptr;
	hr = immdevice->lpVtbl->OpenPropertyStore(immdevice, STGM_READ, &prop);
	if (SUCCEEDED(hr)) {
		PROPVARIANT pv;
		hr = prop->lpVtbl->GetValue(prop, &PKEY_Device_FriendlyName, &pv);
		if (SUCCEEDED(hr)) {
			if (pv.vt == VT_LPWSTR) {
				native->Name = native->NameBufferA;
				PL_UTF16ToUTF8(native->NameBufferA, sizeof(native->NameBufferA), pv.pwszVal);
			}
		}
		prop->lpVtbl->Release(prop);
	}

	Audio.FirstDevice.Next = native;

	return native;

failed:

	if (endpoint) {
		endpoint->lpVtbl->Release(endpoint);
	}

	if (id) {
		CoTaskMemFree(id);
	}

	return nullptr;
}

static PL_AudioDeviceNative *PL_AddAudioDeviceNativeFromId(LPCWSTR id) {
	IMMDevice *immdevice = nullptr;
	HRESULT hr = Audio.DeviceEnumerator->lpVtbl->GetDevice(Audio.DeviceEnumerator, id, &immdevice);
	if (FAILED(hr)) return nullptr;
	PL_AudioDeviceNative *native = PL_AddAudioDeviceNative(immdevice);
	immdevice->lpVtbl->Release(immdevice);
	return native;
}

HRESULT STDMETHODCALLTYPE PL_OnDeviceStateChanged(IMMNotificationClient *this_, LPCWSTR device_id, DWORD new_state) {
	PL_AtomicLock(&Audio.Lock);

	PL_AudioDeviceNative *native = PL_FindAudioDeviceNative(device_id);

	u32 active = (new_state == DEVICE_STATE_ACTIVE);
	u32 prev   = 0;

	if (native) {
		bool changed = (native->IsActive != active);
		InterlockedExchange(&native->IsActive, active);

		if (changed) {
			UINT msg = active ? PL_WM_AUDIO_DEVICE_ACTIVATED : PL_WM_AUDIO_DEVICE_DEACTIVATED;
			PostThreadMessageW(Audio.ParentThreadId, msg, (WPARAM)native, 0);
		}
	} else {
		native = PL_AddAudioDeviceNativeFromId(device_id);
		UINT msg = active ? PL_WM_AUDIO_DEVICE_ACTIVATED : PL_WM_AUDIO_DEVICE_DEACTIVATED;
		PostThreadMessageW(Audio.ParentThreadId, msg, (WPARAM)native, 0);
	}

	PL_AtomicUnlock(&Audio.Lock);

	PL_AudioEndpoint *endpoint = &Audio.Endpoints[native->IsCapture == 0 ?
		PL_AudioEndpoint_Render : PL_AudioEndpoint_Capture];

	PL_AudioDeviceNative *desired   = PL_AtomicCmpExgPtr(&endpoint->DesiredDevice, 0, 0);
	PL_AudioDeviceNative *effective = PL_AtomicCmpExgPtr(&endpoint->EffectiveDevice, 0, 0);
	if (desired == nullptr) return S_OK; // Handled by Default's notification

	if (native->IsActive) {
		if (native == desired || !effective) {
			SetEvent(endpoint->Commands[PL_AudioCommand_Switch]);
		}
	} else if (native == effective) {
		SetEvent(endpoint->Commands[PL_AudioCommand_Switch]);
	}

	return S_OK;
}

// These events is handled in "OnDeviceStateChanged"
HRESULT STDMETHODCALLTYPE PL_OnDeviceAdded(IMMNotificationClient *this_, LPCWSTR device_id)   { return S_OK; }
HRESULT STDMETHODCALLTYPE PL_OnDeviceRemoved(IMMNotificationClient *this_, LPCWSTR device_id) { return S_OK; }

HRESULT STDMETHODCALLTYPE PL_OnDefaultDeviceChanged(IMMNotificationClient *this_, EDataFlow flow, ERole role, LPCWSTR device_id) {
	if (role != eMultimedia) return S_OK;

	PL_AudioEndpointKind kind = flow == eRender ? PL_AudioEndpoint_Render : PL_AudioEndpoint_Capture;
	PL_AudioEndpoint *   endpoint = &Audio.Endpoints[kind];

	if (!device_id) {
		// No device for the endpoint
		PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_DEVICE_LOST, kind, 0);
		return S_OK;
	}

	PL_AudioDevice *desired = PL_AtomicCmpExgPtr(&endpoint->DesiredDevice, nullptr, nullptr);
	if (desired == nullptr) {
		SetEvent(endpoint->Commands[PL_AudioCommand_Switch]);
	}

	return S_OK;
}

HRESULT STDMETHODCALLTYPE PL_OnPropertyValueChanged(IMMNotificationClient *this_, LPCWSTR device_id, const PROPERTYKEY key) {
	if (memcmp(&key, &PKEY_Device_FriendlyName, sizeof(PROPERTYKEY)) == 0) {
		PL_AtomicLock(&Audio.Lock);

		IMMDevice *immdevice = nullptr;
		HRESULT hr = Audio.DeviceEnumerator->lpVtbl->GetDevice(Audio.DeviceEnumerator, device_id, &immdevice);
		if (FAILED(hr)) {
			PL_AtomicUnlock(&Audio.Lock);
			return S_OK;
		}

		PL_AudioDeviceNative *native = PL_FindAudioDeviceNative(device_id);
		if (!native) {
			PL_AddAudioDeviceNativeFromId(device_id);
			PL_AtomicUnlock(&Audio.Lock);
			return S_OK;
		}

		IPropertyStore *prop = nullptr;
		hr = immdevice->lpVtbl->OpenPropertyStore(immdevice, STGM_READ, &prop);
		if (SUCCEEDED(hr)) {
			PROPVARIANT pv;
			hr = prop->lpVtbl->GetValue(prop, &PKEY_Device_FriendlyName, &pv);
			if (SUCCEEDED(hr)) {
				if (pv.vt == VT_LPWSTR) {
					native->Name = native->Name == native->NameBufferA ? native->NameBufferB : native->NameBufferA;
					PL_UTF16ToUTF8(native->Name, sizeof(native->NameBufferA), pv.pwszVal);
				}
			}
			prop->lpVtbl->Release(prop);
		}

		PL_AtomicUnlock(&Audio.Lock);
	}
	return S_OK;
}

static IMMNotificationClientVtbl NotificationClientVTable = {
	.QueryInterface         = PL_QueryInterface,
	.AddRef                 = PL_AddRef,
	.Release                = PL_Release,
	.OnDeviceStateChanged   = PL_OnDeviceStateChanged,
	.OnDeviceAdded          = PL_OnDeviceAdded,
	.OnDeviceRemoved        = PL_OnDeviceRemoved,
	.OnDefaultDeviceChanged = PL_OnDefaultDeviceChanged,
	.OnPropertyValueChanged = PL_OnPropertyValueChanged,
};

static IMMNotificationClient NotificationClient = {
	.lpVtbl = &NotificationClientVTable
};

static void PL_Media_EnumerateAudioDevices(void) {
	HRESULT hr = S_OK;

	IMMDeviceCollection *device_col = nullptr;
	hr = Audio.DeviceEnumerator->lpVtbl->EnumAudioEndpoints(Audio.DeviceEnumerator, eAll, DEVICE_STATEMASK_ALL, &device_col);
	if (FAILED(hr)) return;

	UINT count;
	hr = device_col->lpVtbl->GetCount(device_col, &count);
	if (FAILED(hr)) { device_col->lpVtbl->Release(device_col); return; }

	PL_AtomicLock(&Audio.Lock);

	LPWSTR id = nullptr;
	IMMDevice *immdevice = nullptr;
	for (UINT index = 0; index < count; ++index) {
		hr = device_col->lpVtbl->Item(device_col, index, &immdevice);
		if (FAILED(hr)) continue;

		hr = immdevice->lpVtbl->GetId(immdevice, &id);
		if (SUCCEEDED(hr)) {
			PL_AudioDeviceNative *native = PL_AddAudioDeviceNative(immdevice);
			if (native->IsActive) {
				PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_DEVICE_ACTIVATED, (WPARAM)native, 0);
			}
			CoTaskMemFree(id);
			id = nullptr;
		}

		immdevice->lpVtbl->Release(immdevice);
		immdevice = nullptr;
	}

	device_col->lpVtbl->Release(device_col);

	PL_AtomicUnlock(&Audio.Lock);
}

static void PL_AudioEndpoint_Release(PL_AudioEndpoint *endpoint) {
	if (endpoint->Thread) {
		InterlockedExchange(&endpoint->DeviceLost, 1);
		SetEvent(endpoint->Commands[PL_AudioCommand_Terminate]);
		WaitForSingleObject(endpoint->Thread, INFINITE);
		CloseHandle(endpoint->Thread);
	}
	if (endpoint->Render) {
		endpoint->Render->lpVtbl->Release(endpoint->Render);
	}
	if (endpoint->Capture) {
		endpoint->Capture->lpVtbl->Release(endpoint->Capture);
	}
	if (endpoint->Client) {
		endpoint->Client->lpVtbl->Release(endpoint->Client);
	}
	for (int j = 0; j < PL_AudioCommand_EnumCount; ++j) {
		if (endpoint->Commands[j])
			CloseHandle(endpoint->Commands[j]);
	}
}

static void PL_AudioEndpoint_RenderFrames(void) {
	PL_AudioEndpoint *endpoint = &Audio.Endpoints[PL_AudioEndpoint_Render];

	if (endpoint->DeviceLost)
		return;

	BYTE *data     = nullptr;
	u32 frames     = 0;
	UINT32 padding = 0;

	HRESULT hr = endpoint->Client->lpVtbl->GetCurrentPadding(endpoint->Client, &padding);
	if (SUCCEEDED(hr)) {
		frames = endpoint->MaxFrames - padding;
		if (frames) {
			hr = endpoint->Render->lpVtbl->GetBuffer(endpoint->Render, frames, &data);
			if (SUCCEEDED(hr)) {
				u32 written = Media.UserVTable.OnAudioRender(&endpoint->Spec, data, frames, Media.UserVTable.Data);
				hr = endpoint->Render->lpVtbl->ReleaseBuffer(endpoint->Render, written, 0);
			}
		} else {
			hr = endpoint->Render->lpVtbl->ReleaseBuffer(endpoint->Render, 0, 0);
		}
	}

	if (FAILED(hr)) {
		InterlockedExchange(&endpoint->DeviceLost, 1);
	}
}

static void PL_AudioEndpoint_CaptureFrames(void) {
	PL_AudioEndpoint *endpoint = &Audio.Endpoints[PL_AudioEndpoint_Capture];

	if (endpoint->DeviceLost)
		return;

	BYTE *data     = nullptr;
	UINT32 frames  = 0;
	DWORD flags    = 0;

	HRESULT hr = endpoint->Capture->lpVtbl->GetNextPacketSize(endpoint->Capture, &frames);

	while (frames) {
		if (FAILED(hr)) {
			InterlockedExchange(&endpoint->DeviceLost, 1);
			return;
		}
		hr = endpoint->Capture->lpVtbl->GetBuffer(endpoint->Capture, &data, &frames, &flags, nullptr, nullptr);
		if (SUCCEEDED(hr)) {
			if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
				data = nullptr; // Silence
			u32 read = Media.UserVTable.OnAudioCapture(&endpoint->Spec, data, frames, Media.UserVTable.Data);
			hr = endpoint->Capture->lpVtbl->ReleaseBuffer(endpoint->Capture, read);
			if (SUCCEEDED(hr)) {
				hr = endpoint->Capture->lpVtbl->GetNextPacketSize(endpoint->Capture, &frames);
			}
		}
	}
}

static void PL_AudioEndpoint_ReleaseDevice(PL_AudioEndpointKind kind) {
	PL_AudioEndpoint *endpoint = &Audio.Endpoints[kind];
	if (endpoint->Render) {
		endpoint->Render->lpVtbl->Release(endpoint->Render);
		endpoint->Render = nullptr;
	}
	if (endpoint->Client) {
		endpoint->Client->lpVtbl->Release(endpoint->Client);
		endpoint->Client = nullptr;
	}
	endpoint->MaxFrames = 0;
	InterlockedExchange(&endpoint->DeviceLost, 1);
	InterlockedExchangePointer(&endpoint->EffectiveDevice, nullptr);
}

static bool PL_AudioEndpoint_SetAudioSpec(PL_AudioEndpoint *endpoint, WAVEFORMATEX *wave_format) {
	const WAVEFORMATEXTENSIBLE *ext = (WAVEFORMATEXTENSIBLE *)wave_format;

	if (wave_format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT &&
		wave_format->wBitsPerSample == 32) {
		endpoint->Spec.Format = PL_AudioFormat_R32;
	} else if (wave_format->wFormatTag == WAVE_FORMAT_PCM &&
		wave_format->wBitsPerSample == 32) {
		endpoint->Spec.Format = PL_AudioFormat_I32;
	} else if (wave_format->wFormatTag == WAVE_FORMAT_PCM &&
		wave_format->wBitsPerSample == 16) {
		endpoint->Spec.Format = PL_AudioFormat_I16;
	} else if (wave_format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 32)) {
			endpoint->Spec.Format = PL_AudioFormat_R32;
		} else if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 32)) {
			endpoint->Spec.Format = PL_AudioFormat_I32;
		} else if ((memcmp(&ext->SubFormat, &PL_KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0) &&
			(wave_format->wBitsPerSample == 16)) {
			endpoint->Spec.Format = PL_AudioFormat_I16;
		} else {
			return false;
		}
	} else {
		return false;
	}

	endpoint->Spec.Channels  = wave_format->nChannels;
	endpoint->Spec.Frequency = wave_format->nSamplesPerSec;

	return true;
}

static bool PL_AudioEndpoint_TryLoadDevice(PL_AudioEndpointKind kind, PL_AudioDeviceNative *native) {
	PL_AudioEndpoint *endpoint = &Audio.Endpoints[kind];
	IMMDevice *immdevice       = nullptr;
	WAVEFORMATEX *wave_format  = nullptr;

	HRESULT hr;

	if (!native) {
		EDataFlow flow = kind == PL_AudioEndpoint_Render ? eRender : eCapture;
		hr = Audio.DeviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(Audio.DeviceEnumerator, flow, eMultimedia, &immdevice);
		if (FAILED(hr)) return false;

		LPCWSTR id = nullptr;
		hr         = immdevice->lpVtbl->GetId(immdevice, &id);
		if (SUCCEEDED(hr)) {
			native = PL_FindAudioDeviceNative(id);
			CoTaskMemFree((void *)id);
		}
		if (!native) {
			immdevice->lpVtbl->Release(immdevice);
			return false;
		}
	} else {
		hr = Audio.DeviceEnumerator->lpVtbl->GetDevice(Audio.DeviceEnumerator, native->Id, &immdevice);
		if (FAILED(hr)) return false;
	}

	hr = immdevice->lpVtbl->Activate(immdevice, &PL_IID_IAudioClient, CLSCTX_ALL, nullptr, &endpoint->Client);
	if (FAILED(hr)) goto failed;

	hr = endpoint->Client->lpVtbl->GetMixFormat(endpoint->Client, &wave_format);
	if (FAILED(hr)) goto failed;

	if (!PL_AudioEndpoint_SetAudioSpec(endpoint, wave_format)) {
		LogError("Window: Unsupported audio specification, device = %s\n", native->Name);
		goto failed;
	}

	hr = endpoint->Client->lpVtbl->Initialize(endpoint->Client, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		0, 0, wave_format, nullptr);
	if (FAILED(hr)) goto failed;

	hr = endpoint->Client->lpVtbl->SetEventHandle(endpoint->Client, endpoint->Commands[PL_AudioCommand_Update]);
	if (FAILED(hr)) goto failed;

	hr = endpoint->Client->lpVtbl->GetBufferSize(endpoint->Client, &endpoint->MaxFrames);
	if (FAILED(hr)) goto failed;

	if (kind == PL_AudioEndpoint_Render) {
		hr = endpoint->Client->lpVtbl->GetService(endpoint->Client, &PL_IID_IAudioRenderClient, &endpoint->Render);
	} else {
		hr = endpoint->Client->lpVtbl->GetService(endpoint->Client, &PL_IID_IAudioCaptureClient, &endpoint->Capture);
	}
	if (FAILED(hr)) goto failed;

	CoTaskMemFree(wave_format);
	immdevice->lpVtbl->Release(immdevice);

	InterlockedExchangePointer(&endpoint->EffectiveDevice, native);
	InterlockedExchange(&endpoint->DeviceLost, 0);

	PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_DEVICE_CHANGED,
		(WPARAM)endpoint->EffectiveDevice, (WPARAM)endpoint->DesiredDevice);

	if (kind == PL_AudioEndpoint_Render) {
		PL_AudioEndpoint_RenderFrames();
	} else {
		PL_AudioEndpoint_CaptureFrames();
	}

	if (endpoint->Resumed) {
		IAudioClient *client = endpoint->Client;
		HRESULT hr = client->lpVtbl->Start(client);
		if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED) {
			PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_RESUMED, kind, 0);
		} else {
			InterlockedExchange(&endpoint->DeviceLost, 1);
			goto failed;
		}
	}

	return true;

failed:

	if (wave_format)
		CoTaskMemFree(wave_format);

	immdevice->lpVtbl->Release(immdevice);

	return false;
}

static void PL_AudioEndpoint_RestartDevice(PL_AudioEndpointKind kind) {
	PL_AudioDeviceNative *native = PL_AtomicCmpExgPtr(&Audio.Endpoints[kind].DesiredDevice, nullptr, nullptr);
	PL_AudioEndpoint_ReleaseDevice(kind);
	if (!PL_AudioEndpoint_TryLoadDevice(kind, native)) {
		PL_AudioEndpoint_ReleaseDevice(kind);
		if (native) {
			if (!PL_AudioEndpoint_TryLoadDevice(kind, nullptr)) {
				PL_AudioEndpoint_ReleaseDevice(kind);
				PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_DEVICE_LOST, kind, 0);
			}
		} else {
			PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_DEVICE_LOST, kind, 0);
		}
	}
}

static DWORD WINAPI PL_AudioThread(LPVOID param) {
	PL_AudioEndpointKind kind = (PL_AudioEndpointKind)param;

	HANDLE thread = GetCurrentThread();
	SetThreadDescription(thread, kind == PL_AudioEndpoint_Render ? L"PL Audio Render Thread" : L"PL Audio Capture Thread");

	PL_InitThreadContext();

	DWORD task_index;
	HANDLE avrt = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	PL_AudioEndpoint *endpoint = &Audio.Endpoints[kind];

	PL_AudioEndpoint_RestartDevice(kind);

	HRESULT hr;

	while (1) {
		DWORD wait = WaitForMultipleObjects(PL_AudioCommand_EnumCount, endpoint->Commands, FALSE, INFINITE);

		if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Update) {
			if (kind == PL_AudioEndpoint_Render)
				PL_AudioEndpoint_RenderFrames();
			else
				PL_AudioEndpoint_CaptureFrames();
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Resume) {
			InterlockedExchange(&endpoint->Resumed, 1);
			if (!endpoint->DeviceLost) {
				hr = endpoint->Client->lpVtbl->Start(endpoint->Client);
				if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED) {
					PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_RESUMED, PL_AudioEndpoint_Render, 0);
				} else {
					InterlockedExchange(&endpoint->DeviceLost, 1);
				}
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Pause) {
			InterlockedExchange(&endpoint->Resumed, 0);
			if (!endpoint->DeviceLost) {
				hr = endpoint->Client->lpVtbl->Stop(endpoint->Client);
				if (SUCCEEDED(hr)) {
					if (!PostThreadMessageW(Audio.ParentThreadId, PL_WM_AUDIO_PAUSED, PL_AudioEndpoint_Render, 0)) {
						TriggerBreakpoint();
					}
				} else {
					InterlockedExchange(&endpoint->DeviceLost, 1);
				}
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Reset) {
			LONG resumed = InterlockedExchange(&endpoint->Resumed, 0);
			if (!endpoint->DeviceLost) {
				if (resumed) {
					endpoint->Client->lpVtbl->Stop(endpoint->Client);
				}
				hr = endpoint->Client->lpVtbl->Reset(endpoint->Client);
				if (FAILED(hr)) {
					InterlockedExchange(&endpoint->DeviceLost, 1);
				}
			}
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Switch) {
			PL_AudioEndpoint_RestartDevice(kind);
		} else if (wait == WAIT_OBJECT_0 + PL_AudioCommand_Terminate) {
			break;
		}
	}

	AvRevertMmThreadCharacteristics(avrt);

	return 0;
}

static bool PL_Audio_IsAudioRendering(void) {
	if (InterlockedOr(&Audio.Endpoints[PL_AudioEndpoint_Render].DeviceLost, 0))
		return false;
	return InterlockedOr(&Audio.Endpoints[PL_AudioEndpoint_Render].Resumed, 0);
}

static void PL_Audio_UpdateAudioRender(void) {
	if (!InterlockedOr(&Audio.Endpoints[PL_AudioEndpoint_Render].Resumed, 0)) {
		SetEvent(Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Update]);
	}
}

static void PL_Audio_PauseAudioRender(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Pause]);
}

static void PL_Audio_ResumeAudioRender(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Resume]);
}

static void PL_Audio_ResetAudioRender(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Render].Commands[PL_AudioCommand_Reset]);
}

static bool PL_Audio_IsAudioCapturing(void) {
	if (InterlockedOr(&Audio.Endpoints[PL_AudioEndpoint_Capture].DeviceLost, 0))
		return false;
	return InterlockedOr(&Audio.Endpoints[PL_AudioEndpoint_Capture].Resumed, 0);
}

static void PL_Audio_PauseAudioCapture(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Capture].Commands[PL_AudioCommand_Pause]);
}

static void PL_Audio_ResumeAudioCapture(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Capture].Commands[PL_AudioCommand_Resume]);
}

static void PL_Audio_ResetAudioCapture(void) {
	SetEvent(Audio.Endpoints[PL_AudioEndpoint_Capture].Commands[PL_AudioCommand_Reset]);
}

static void PL_Audio_SetAudioDevice(PL_AudioDevice const *device) {
	Assert((device >= Media.IoDevice.AudioRenderDevices.Data && 
			device <= Media.IoDevice.AudioRenderDevices.Data + Media.IoDevice.AudioRenderDevices.Count) ||
			(device >= Media.IoDevice.AudioCaptureDevices.Data &&
			device <= Media.IoDevice.AudioCaptureDevices.Data + Media.IoDevice.AudioCaptureDevices.Count));

	PL_AudioDeviceNative * native   = device->Handle;
	PL_AudioEndpointKind   endpoint = native->IsCapture == 0 ? PL_AudioEndpoint_Render : PL_AudioEndpoint_Capture;

	PL_AudioDeviceNative *prev = InterlockedExchangePointer(&Audio.Endpoints[endpoint].DesiredDevice, native);
	if (prev != native) {
		SetEvent(Audio.Endpoints[endpoint].Commands[PL_AudioCommand_Switch]);
	}

	if (endpoint == PL_AudioEndpoint_Render) {
		Media.IoDevice.AudioRenderDevices.Current  = (PL_AudioDevice *)device;
	} else {
		Media.IoDevice.AudioCaptureDevices.Current = (PL_AudioDevice *)device;
	}
}

static bool PL_AudioEndpoint_Init(PL_AudioEndpointKind kind) {
	PL_AudioEndpoint *endpoint = &Audio.Endpoints[kind];

	for (int i = 0; i < PL_AudioCommand_EnumCount; ++i) {
		endpoint->Commands[i] = CreateEventExW(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
		if (!endpoint->Commands[i]) {
			return false;
		}
	}

	endpoint->Thread = CreateThread(nullptr, 0, PL_AudioThread, (LPVOID)kind, 0, 0);
	if (!endpoint->Thread) {
		return false;
	}

	if (kind == PL_AudioEndpoint_Render) {
		Media.VTable.IsAudioRendering   = PL_Audio_IsAudioRendering;
		Media.VTable.UpdateAudioRender  = PL_Audio_UpdateAudioRender;
		Media.VTable.PauseAudioRender   = PL_Audio_PauseAudioRender;
		Media.VTable.ResumeAudioRender  = PL_Audio_ResumeAudioRender;
		Media.VTable.ResetAudioRender   = PL_Audio_ResetAudioRender;
	} else {
		Media.VTable.IsAudioCapturing   = PL_Audio_IsAudioCapturing;
		Media.VTable.PauseAudioCapture  = PL_Audio_PauseAudioCapture;
		Media.VTable.ResumeAudioCapture = PL_Audio_ResumeAudioCapture;
		Media.VTable.ResetAudioCapture  = PL_Audio_ResetAudioCapture;
	}

	return true;
}

//
// Public API
//

void PL_InitAudio(bool render, bool capture) {
	MSG msg;
	PeekMessageW(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

	Audio.ParentThreadId = GetCurrentThreadId();

	Media.IoDevice.AudioRenderDevices.Data  = Media.AudioDeviceBuffer[PL_AudioEndpoint_Render].Data;
	Media.IoDevice.AudioCaptureDevices.Data = Media.AudioDeviceBuffer[PL_AudioEndpoint_Capture].Data;

	HRESULT hr = CoCreateInstance(&PL_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &PL_IID_IMMDeviceEnumerator, &Audio.DeviceEnumerator);
	if (FAILED(hr)) {
		LogWarning("Windows: Failed to create audio instance. Reason: %s\n", PL_GetLastError());
		return;
	}

	hr = Audio.DeviceEnumerator->lpVtbl->RegisterEndpointNotificationCallback(Audio.DeviceEnumerator, &NotificationClient);

	if (FAILED(hr)) {
		LogWarning("Windows: Failed to enumerate audio devices. Reason: %s\n", PL_GetLastError());
		Audio.DeviceEnumerator->lpVtbl->Release(Audio.DeviceEnumerator);
		Audio.DeviceEnumerator = nullptr;
		return;
	}

	PL_Media_EnumerateAudioDevices();

	if (render || capture)
		Media.VTable.SetAudioDevice = PL_Audio_SetAudioDevice;

	if (render) {
		if (!PL_AudioEndpoint_Init(PL_AudioEndpoint_Render)) {
			PL_AudioEndpoint_Release(&Audio.Endpoints[PL_AudioEndpoint_Render]);
			LogWarning("Windows: Failed to initialize audio device for rendering. Reason: %s\n", PL_GetLastError());
		}
	}

	if (capture) {
		if (!PL_AudioEndpoint_Init(PL_AudioEndpoint_Capture)) {
			PL_AudioEndpoint_Release(&Audio.Endpoints[PL_AudioEndpoint_Capture]);
			LogWarning("Windows: Failed to initialize audio device for capturing. Reason: %s\n", PL_GetLastError());
		}
	}
}

void PL_ReleaseAudioRender(void) {
	PL_AudioEndpoint_Release(&Audio.Endpoints[PL_AudioEndpoint_Render]);

	Media.VTable.IsAudioRendering   = PL_Media_Fallback_IsAudioRendering;
	Media.VTable.UpdateAudioRender  = PL_Media_Fallback_UpdateAudioRender;
	Media.VTable.PauseAudioRender   = PL_Media_Fallback_PauseAudioRender;
	Media.VTable.ResumeAudioRender  = PL_Media_Fallback_ResumeAudioRender;
	Media.VTable.ResetAudioRender   = PL_Media_Fallback_ResetAudioRender;
}

void PL_ReleaseAudioCapture(void) {
	PL_AudioEndpoint_Release(&Audio.Endpoints[PL_AudioEndpoint_Capture]);

	Media.VTable.IsAudioCapturing   = PL_Media_Fallback_IsAudioCapturing;
	Media.VTable.PauseAudioCapture  = PL_Media_Fallback_PauseAudioCapture;
	Media.VTable.ResumeAudioCapture = PL_Media_Fallback_ResumeAudioCapture;
	Media.VTable.ResetAudioCapture  = PL_Media_Fallback_ResetAudioCapture;
}

void PL_ReleaseAudio(void) {
	PL_ReleaseAudioRender();
	PL_ReleaseAudioCapture();

	if (Audio.DeviceEnumerator)
		Audio.DeviceEnumerator->lpVtbl->Release(Audio.DeviceEnumerator);

	while (Audio.FirstDevice.Next) {
		PL_AudioDeviceNative *native   = Audio.FirstDevice.Next;
		Audio.FirstDevice.Next = native->Next;
		CoTaskMemFree((void *)native->Id);
		CoTaskMemFree(native);
	}

	Media.VTable.SetAudioDevice = PL_Media_Fallback_SetAudioDevice;
}
