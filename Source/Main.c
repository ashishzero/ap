#define MICROSOFT_WINDOWS_WINBASE_H_DEFINE_INTERLOCKED_CPLUSPLUS_OVERLOADS 0
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "Platform.h"

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioClient, 0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioRenderClient, 0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);

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

int main(int argc, char *argv[]) {
	if (argc <= 1){
		Usage(argv[0]);
	}

	const char *path = argv[1];

	String buff = ReadEntireFile(path);
	Audio_Stream *audio = (Audio_Stream *)buff.data;

	uint input_count = 0;
	i16 *input = Serialize(audio, &input_count);

	//uint input_samples = audio->data.size / sizeof(u16);

	uint current_sample = 0;

	HRESULT hresult = CoInitialize(nullptr);
	if (FAILED(hresult)) FatalWindowsError();

	IMMDeviceEnumerator *device_enumerator = nullptr;
	hresult = CoCreateInstance(&CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &device_enumerator);
	if (FAILED(hresult)) FatalWindowsError();

	IMMDevice *device = nullptr;
	hresult = device_enumerator->lpVtbl->GetDefaultAudioEndpoint(device_enumerator, eRender, eMultimedia, &device);
	if (FAILED(hresult)) FatalWindowsError();

	IAudioClient *audio_client = nullptr;
	hresult = device->lpVtbl->Activate(device, &IID_IAudioClient, CLSCTX_ALL, nullptr, &audio_client);
	if (FAILED(hresult)) FatalWindowsError();

	WAVEFORMATEX *format = nullptr;

	hresult = audio_client->lpVtbl->GetMixFormat(audio_client, &format);
	if (FAILED(hresult)) FatalWindowsError();

	REFERENCE_TIME requested_duration = 10000000;

	hresult = audio_client->lpVtbl->Initialize(audio_client, AUDCLNT_SHAREMODE_SHARED, 0, requested_duration, 0, format, nullptr);
	if (FAILED(hresult)) FatalWindowsError();

	UINT32 frame_count;
	hresult = audio_client->lpVtbl->GetBufferSize(audio_client, &frame_count);
	if (FAILED(hresult)) FatalWindowsError();

	IAudioRenderClient *render_client = nullptr;
	hresult = audio_client->lpVtbl->GetService(audio_client, &IID_IAudioRenderClient, &render_client);
	if (FAILED(hresult)) FatalWindowsError();

	BYTE *data = nullptr;
	hresult = render_client->lpVtbl->GetBuffer(render_client, frame_count, &data);
	if (FAILED(hresult)) FatalWindowsError();

	r32 *samples = (r32 *)data;
	uint count   = format->nChannels * frame_count;

	//double t = 0.0f;
	float v = .5f;

	//float dt = 0.1f;
	//float d    = v * (float)sin(t);
	//t += dt;

	for (uint i = 0; i < count; i += format->nChannels) {
		float l = (float)input[current_sample + 0] / 32767.0f;
		float r = (float)input[current_sample + 1] / 32767.0f;

		samples[i + 0] = v * l;
		samples[i + 1] = v * r;

		current_sample += 2;

		if (current_sample >= input_count)
			current_sample=0;
	}

	hresult = render_client->lpVtbl->ReleaseBuffer(render_client, frame_count, 0);
		if (FAILED(hresult)) FatalWindowsError();

	double actual_duration = 10000000.0 * frame_count / format->nSamplesPerSec;

	hresult = audio_client->lpVtbl->Start(audio_client);
	if (FAILED(hresult)) FatalWindowsError();

	UINT flags = 0;

	while (flags != AUDCLNT_BUFFERFLAGS_SILENT) {
		Sleep((DWORD)(actual_duration/20000.0));

		UINT32 padding = 0;
		hresult = audio_client->lpVtbl->GetCurrentPadding(audio_client, &padding);
		if (FAILED(hresult)) FatalWindowsError();

		uint frames_aval = frame_count-padding;


		printf("rendering %u frames\n", frames_aval);
		//if (!frames_aval) continue;

		hresult = render_client->lpVtbl->GetBuffer(render_client, frames_aval, &data);
		if (FAILED(hresult)) FatalWindowsError();

		samples = (r32 *)data;
		count = frames_aval * format->nChannels;

		for (uint i = 0; i < count; i += format->nChannels) {
			float l = (float)input[current_sample + 0] / 32767.0f;
			float r = (float)input[current_sample + 1] / 32767.0f;

			samples[i + 0] = v * l;
			samples[i + 1] = v * r;

			current_sample += 2;

			if (current_sample >= input_count)
				current_sample=0;
		}

		hresult = render_client->lpVtbl->ReleaseBuffer(render_client, frames_aval, flags);
		if (FAILED(hresult)) FatalWindowsError();
	}


	return 0;
}
