#include <stdio.h>

#pragma warning(push)
#pragma warning(disable: 5105)

#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>

#pragma warning(pop)

#include "KrPlatform.h"

DEFINE_GUID(KR_CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);

DEFINE_GUID(KR_IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
DEFINE_GUID(KR_IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(KR_IID_IMMNotificationClient, 0x7991eec9, 0x7e89, 0x4d85, 0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0);
DEFINE_GUID(KR_IID_IMMEndpoint, 0x1BE09788, 0x6894, 0x4089, 0x85, 0x86, 0x9A, 0x2A, 0x6C, 0x26, 0x5A, 0xC5);

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

enum KrAudioDeviceFlagBits {
	KrAudioDevice_IsAvailable = 0x1,
	KrAudioDevice_IsRender    = 0x2,
	KrAudioDevice_IsCapture   = 0x4
};

typedef struct KrAudioDevice {
	struct KrAudioDevice *Next;
	char *                Name;
	KrAudioDeviceFlow     Flow;
	volatile u32          Active;
} KrAudioDevice;

typedef struct PL_KrDeviceName {
	char Buffer[64];
} PL_KrDeviceName;

typedef struct PL_KrAudioDevice {
	KrAudioDevice   Base;
	LPWSTR          Id;
	PL_KrDeviceName Names[2];
} PL_KrAudioDevice;

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

static IMMDeviceEnumerator * g_DeviceEnumerator;
static PL_KrAudioDevice      g_AudioDeviceListHead = { .Base.Name = "---", .Base.Flow = KrAudioDeviceFlow_EnumMax };
static PL_KrAudioDevice     *g_AudioDeviceListTail = &g_AudioDeviceListHead;
static volatile LONG         g_AudioDeviceListLock;

//
//
//

static IMMDevice *PL_IMMDeviceFromId(LPCWSTR id) {
	IMMDevice *immdevice = nullptr;
	HRESULT hr = g_DeviceEnumerator->lpVtbl->GetDevice(g_DeviceEnumerator, id, &immdevice);
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

static PL_KrAudioDevice *PL_UpdateAudioDeviceList(IMMDevice *immdevice) {
	LPWSTR id = PL_IMMDeviceId(immdevice);
	if (!id) {
		return nullptr;
	}

	PL_KrAudioDevice *device = CoTaskMemAlloc(sizeof(*device));
	if (!device) {
		CoTaskMemFree(id);
		return nullptr;
	}

	memset(device, 0, sizeof(*device));

	device->Id          = id;
	device->Names[0]    = PL_IMMDeviceName(immdevice);
	device->Base.Flow   = PL_IMMDeviceFlow(immdevice);
	device->Base.Active = PL_IMMDeviceActive(immdevice);
	device->Base.Name   = device->Names[0].Buffer;

	g_AudioDeviceListTail->Base.Next = &device->Base;
	g_AudioDeviceListTail = device;

	return device;
}

static PL_KrAudioDevice *PL_FindAudioDeviceFromId(LPCWSTR id) {
	PL_KrAudioDevice *device = (PL_KrAudioDevice *)g_AudioDeviceListHead.Base.Next;
	for (;device; device = (PL_KrAudioDevice *)device->Base.Next) {
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

	PL_KrAudioDevice *device = PL_FindAudioDeviceFromId(deviceid);
	if (device) {
		u32 active = (newstate == DEVICE_STATE_ACTIVE);
		InterlockedExchange(&device->Base.Active, active);
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
	// TODO: Events
	PL_AtomicUnlock(&g_AudioDeviceListLock);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE PL_OnPropertyValueChanged(IMMNotificationClient *This, LPCWSTR deviceid, const PROPERTYKEY key) {
	if (memcmp(&key, &PKEY_Device_FriendlyName, sizeof(PROPERTYKEY)) == 0) {
		PL_AtomicLock(&g_AudioDeviceListLock);

		// TODO: events

		IMMDevice *immdevice = PL_IMMDeviceFromId(deviceid);
		if (immdevice) {
			PL_KrAudioDevice *device = PL_FindAudioDeviceFromId(deviceid);
			if (device) {
				char *name_buff;
				if (device->Names[0].Buffer == device->Base.Name) {
					device->Names[1] = PL_IMMDeviceName(immdevice);
					name_buff = device->Names[1].Buffer;
				} else {
					device->Names[0] = PL_IMMDeviceName(immdevice);
					name_buff = device->Names[0].Buffer;
				}
				InterlockedExchangePointer(&device->Base.Name, name_buff);
			} else {
				PL_UpdateAudioDeviceList(immdevice);
			}
			immdevice->lpVtbl->Release(immdevice);
		}

		PL_AtomicUnlock(&g_AudioDeviceListLock);
	}
	return S_OK;
}

//
//
//

static void ShutdownAudio() {
	if (g_DeviceEnumerator) {
		g_DeviceEnumerator->lpVtbl->UnregisterEndpointNotificationCallback(g_DeviceEnumerator, &g_NotificationClient);
		g_DeviceEnumerator->lpVtbl->Release(g_DeviceEnumerator);
		g_DeviceEnumerator = nullptr;
	}

	for (PL_KrAudioDevice *ptr = (PL_KrAudioDevice *)g_AudioDeviceListHead.Base.Next; ptr; ) {
		PL_KrAudioDevice *next = (PL_KrAudioDevice *)ptr->Base.Next;
		CoTaskMemFree((void *)ptr->Id);
		CoTaskMemFree(ptr);
		ptr = next;
	}

	g_AudioDeviceListHead.Base.Next = nullptr;

	g_AudioDeviceListLock = 0;
}

static void InitializeAudio() {
	HRESULT hr = CoCreateInstance(&KR_CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &KR_IID_IMMDeviceEnumerator, &g_DeviceEnumerator);
	if (FAILED(hr)) {
		Unimplemented();
	}

	hr = g_DeviceEnumerator->lpVtbl->RegisterEndpointNotificationCallback(g_DeviceEnumerator, &g_NotificationClient);
	if (FAILED(hr)) {
		Unimplemented();
	}

	{
		// Enumerate all the audio devices

		IMMDeviceCollection *device_col = nullptr;
		hr = g_DeviceEnumerator->lpVtbl->EnumAudioEndpoints(g_DeviceEnumerator, eAll, DEVICE_STATEMASK_ALL, &device_col);
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
	const KrAudioDevice *Base;
} KrAudioEvent;

typedef struct KrEvent {
	KrEventKind Kind;
	union {
		KrAudioEvent Audio;
	} Data;
} KrEvent;

void ListAudioDevices() {
	printf("Render devices\n");
	for (KrAudioDevice *device = g_AudioDeviceListHead.Base.Next; device; device = device->Next) {
		if (device->Flow == KrAudioDeviceFlow_Render)
			printf(" [%c] %s\n", device->Active ? '*' : ' ', device->Name);
	}
	printf("Capture devices\n");
	for (KrAudioDevice *device = g_AudioDeviceListHead.Base.Next; device; device = device->Next) {
		if (device->Flow == KrAudioDeviceFlow_Capture)
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

int main() {
	Initialize();

	char input[1024];

	for (;;) {
		printf("> ");
		int n = scanf_s("%s", input, (int)sizeof(input));

		if (strcmp(input, "quit") == 0) {
			break;
		} else if (strcmp(input, "list") == 0) {
			ListAudioDevices();
		} else {
			printf("invalid command\n");
		}
	}

	return 0;
}

#if 0
#include "KrModule.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

static uint MaxSample     = 0;
static uint CurrentSample = 0;
static i16 *CurrentStream = 0;
static real Volume        = 0.5f;

u32 AudioUpdate(KrAudioDevice *device, KrAudioSpec *spec, u8 *dst, u32 count, void *user) {
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

void AudioResumed(KrAudioDevice *device) {
	printf("resumed\n");
}

void AudioPaused(KrAudioDevice *device) {
	printf("paused\n");
}

void AudioReset(KrAudioDevice *device) {
	printf("reset\n");
}

void AudioDeviceLost(KrAudioDevice *device, KrAudioDeviceId id) {
	printf("lost\n");
}

void AudioDeviceGained(KrAudioDevice *device, KrAudioDeviceId id) {
	printf("gained\n");
}

int main(int argc, char *argv[]) {
	if (argc <= 1){
		Usage(argv[0]);
	}

	const char *path = argv[1];

	string buff = ReadEntireFile(path);
	Audio_Stream *audio = (Audio_Stream *)buff.data;

	CurrentStream = Serialize(audio, &MaxSample);

	KrAudioContext ctx = {
		.UserData     = nullptr,
		.Update       = AudioUpdate,
		.Resumed      = AudioResumed,
		.Paused       = AudioPaused,
		.Reset        = AudioReset,
		.DeviceGained = AudioDeviceGained,
		.DeviceLost   = AudioDeviceLost,
	};

	KrAudioDevice *device = KrAudioDeviceOpen(&ctx, nullptr, nullptr);

	if (!device) {
		fprintf(stderr, "Error: %S\n", KrGetLastError());
		exit(1);
	}

	KrAudioDeviceUpdate(device);
	KrAudioDeviceResume(device);

	char input[1024];

	for (;;) {
		printf("> ");
		int n = scanf_s("%s", input, (int)sizeof(input));

		if (strcmp(input, "resume") == 0) {
			KrAudioDeviceResume(device);
		} else if (strcmp(input, "pause") == 0) {
			KrAudioDevicePause(device);
		} else if (strcmp(input, "stop") == 0) {
			KrAudioDeviceReset(device);
			CurrentSample = 0;
		} else if (strcmp(input, "reset") == 0) {
			KrAudioDeviceReset(device);
			CurrentSample = 0;
			KrAudioDeviceUpdate(device);
			KrAudioDeviceResume(device);
		} else if (strcmp(input, "quit") == 0) {
			break;
		} else if (strcmp(input, "list") == 0) {
			//PL_EnumerateAudioDevices();
		} else {
			printf("invalid command\n");
		}
	}

	KrAudioDeviceClose(device);

	return 0;
}
#endif
