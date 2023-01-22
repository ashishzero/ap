#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <avrt.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Platform.h"

#pragma comment(lib, "Avrt.lib")

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

_Noreturn void FatalWindowsError(void) {
	DWORD error    = GetLastError();
	LPWSTR message = L"Unknow Error";

	if (error) {
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&message, 0, NULL);
	}

	MessageBoxW(nullptr, message, L"Fatal Error", MB_ICONERROR | MB_OK);

	TriggerBreakpoint();
	ExitProcess(1);
}

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

static IMMDeviceEnumerator *DeviceEnumerator;
static IMMDevice *          Device;
static IAudioClient *       AudioClient;
static IAudioRenderClient * RenderClient;
static IMMNotificationClient NotificationClient;
static IMMNotificationClientVtbl NotificationClientTable;

HRESULT IMMNotificationClient_QueryInterface(IMMNotificationClient *This, REFIID riid, void **ppvObject) {
	if (memcmp(&IID_IUnknown, riid, sizeof(IID_IUnknown)) == 0) {
		*ppvObject = (IUnknown *)This;
	} else if (memcmp(&IID_IMMNotificationClient, riid, sizeof(IID_IMMNotificationClient)) == 0) {
		*ppvObject = (IMMNotificationClient *)This;
	} else {
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	return S_OK;
}

ULONG IMMNotificationClient_AddRef(IMMNotificationClient *This)  { return 2; }
ULONG IMMNotificationClient_Release(IMMNotificationClient *This) { return 1; }

HRESULT IMMNotificationClient_OnDeviceStateChanged(IMMNotificationClient *This, LPCWSTR dev_id, DWORD state) {
	printf("%S changed: %u\n", dev_id, state);

	return S_OK;
}

HRESULT IMMNotificationClient_OnDeviceAdded(IMMNotificationClient *This, LPCWSTR dev_id) {
	printf("%S added\n", dev_id);

	return S_OK;
}

HRESULT IMMNotificationClient_OnDeviceRemoved(IMMNotificationClient *This, LPCWSTR dev_id) {
	printf("%S removed\n", dev_id);

	return S_OK;
}

HRESULT IMMNotificationClient_OnDefaultDeviceChanged(IMMNotificationClient *This, EDataFlow flow, ERole role, LPCWSTR dev_id) {
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

HRESULT IMMNotificationClient_OnPropertyValueChanged(IMMNotificationClient *This, LPCWSTR dev_id, const PROPERTYKEY key) {
	return S_OK;
}

static uint MaxSample     = 0;
static uint CurrentSample = 0;
static i16 *CurrentStream = 0;

DWORD WINAPI AudioThread(LPVOID param) {

	HRESULT hresult;

	DWORD task_index;
	AvSetMmThreadCharacteristicsW(L"Pro Audio", &task_index);

	hresult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (hresult != S_OK && hresult != S_FALSE && hresult != RPC_E_CHANGED_MODE) {
		FatalWindowsError();
	}

	hresult = CoCreateInstance(&CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &DeviceEnumerator);
	if (FAILED(hresult)) FatalWindowsError();

	hresult = DeviceEnumerator->lpVtbl->GetDefaultAudioEndpoint(DeviceEnumerator, eRender, eMultimedia, &Device);
	if (FAILED(hresult)) FatalWindowsError();

	NotificationClient.lpVtbl = &NotificationClientTable;

	NotificationClient.lpVtbl->AddRef                 = IMMNotificationClient_AddRef;
	NotificationClient.lpVtbl->Release                = IMMNotificationClient_Release;
	NotificationClient.lpVtbl->QueryInterface         = IMMNotificationClient_QueryInterface;
	NotificationClient.lpVtbl->OnDeviceAdded          = IMMNotificationClient_OnDeviceAdded;
	NotificationClient.lpVtbl->OnDeviceRemoved        = IMMNotificationClient_OnDeviceRemoved;
	NotificationClient.lpVtbl->OnDefaultDeviceChanged = IMMNotificationClient_OnDefaultDeviceChanged;
	NotificationClient.lpVtbl->OnDeviceStateChanged   = IMMNotificationClient_OnDeviceStateChanged;
	NotificationClient.lpVtbl->OnPropertyValueChanged = IMMNotificationClient_OnPropertyValueChanged;

	hresult = DeviceEnumerator->lpVtbl->RegisterEndpointNotificationCallback(DeviceEnumerator, &NotificationClient);
	if (FAILED(hresult)) FatalWindowsError();

	hresult = Device->lpVtbl->Activate(Device, &IID_IAudioClient, CLSCTX_ALL, nullptr, &AudioClient);
	if (FAILED(hresult)) FatalWindowsError();

	WAVEFORMATEX *format = nullptr;

	hresult = AudioClient->lpVtbl->GetMixFormat(AudioClient, &format);
	if (FAILED(hresult)) FatalWindowsError();

	REFERENCE_TIME requested_duration;

	hresult = AudioClient->lpVtbl->GetDevicePeriod(AudioClient, NULL, &requested_duration);
	if (FAILED(hresult)) FatalWindowsError();

	hresult = AudioClient->lpVtbl->Initialize(AudioClient, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
		requested_duration, requested_duration, format, nullptr);
	if (hresult == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED) {
		const REFERENCE_TIME REFTIMES_PER_SEC = 10000000;

		// Align the buffer if needed, see IAudioClient::Initialize() documentation
		UINT32 frames = 0;
		hresult = AudioClient->lpVtbl->GetBufferSize(AudioClient, &frames);
		if (FAILED(hresult)) FatalWindowsError();
		requested_duration = (REFERENCE_TIME)((double)REFTIMES_PER_SEC / format->nSamplesPerSec * frames + 0.5);
		hresult = AudioClient->lpVtbl->Initialize(AudioClient, AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
			requested_duration, requested_duration, format, nullptr);
	}
	if (FAILED(hresult)) FatalWindowsError();

	HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);

	hresult = AudioClient->lpVtbl->SetEventHandle(AudioClient, event);
	if (FAILED(hresult)) FatalWindowsError();

	UINT32 frame_count;
	hresult = AudioClient->lpVtbl->GetBufferSize(AudioClient, &frame_count);
	if (FAILED(hresult)) FatalWindowsError();

	hresult = AudioClient->lpVtbl->GetService(AudioClient, &IID_IAudioRenderClient, &RenderClient);
	if (FAILED(hresult)) FatalWindowsError();

	BYTE *data = nullptr;
	hresult = RenderClient->lpVtbl->GetBuffer(RenderClient, frame_count, &data);
	if (FAILED(hresult)) FatalWindowsError();

	r32 *samples = (r32 *)data;
	uint count   = format->nChannels * frame_count;

	float v = .5f;

	for (uint i = 0; i < count; i += format->nChannels) {
		float l = (float)CurrentStream[CurrentSample + 0] / 32767.0f;
		float r = (float)CurrentStream[CurrentSample + 1] / 32767.0f;

		samples[i + 0] = v * l;
		samples[i + 1] = v * r;

		CurrentSample += 2;

		if (CurrentSample >= MaxSample)
			CurrentSample=0;
	}

	hresult = RenderClient->lpVtbl->ReleaseBuffer(RenderClient, frame_count, 0);
		if (FAILED(hresult)) FatalWindowsError();

	double actual_duration = 10000000.0 * frame_count / format->nSamplesPerSec;

	hresult = AudioClient->lpVtbl->Start(AudioClient);
	if (FAILED(hresult)) FatalWindowsError();

	UINT flags = 0;

	while (flags != AUDCLNT_BUFFERFLAGS_SILENT) {
		WaitForSingleObject(event, INFINITE);

		UINT32 padding = 0;
		hresult = AudioClient->lpVtbl->GetCurrentPadding(AudioClient, &padding);
		if (FAILED(hresult)) FatalWindowsError();

		uint frames_aval = frame_count-padding;

		//printf("rendering %u frames\n", frames_aval);

		hresult = RenderClient->lpVtbl->GetBuffer(RenderClient, frames_aval, &data);
		if (FAILED(hresult)) FatalWindowsError();

		samples = (r32 *)data;
		count = frames_aval * format->nChannels;

		for (uint i = 0; i < count; i += format->nChannels) {
			float l = (float)CurrentStream[CurrentSample + 0] / 32767.0f;
			float r = (float)CurrentStream[CurrentSample + 1] / 32767.0f;

			samples[i + 0] = v * l;
			samples[i + 1] = v * r;

			CurrentSample += 2;

			if (CurrentSample >= MaxSample)
				CurrentSample=0;
		}

		hresult = RenderClient->lpVtbl->ReleaseBuffer(RenderClient, frames_aval, flags);
		if (FAILED(hresult)) FatalWindowsError();
	}

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

	HANDLE thread = CreateThread(nullptr, 0, AudioThread, nullptr, 0, nullptr);

	WaitForSingleObject(thread, INFINITE);

	return 0;
}
