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

void AudioDeviceLost(KrAudioDevice *device, KrAudioDeviceId *id) {
	printf("lost\n");
}

void AudioDeviceGained(KrAudioDevice *device, KrAudioDeviceId *id) {
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

	KrAudioDevice *device = KrOpenAudioDevice(&ctx, nullptr, nullptr);

	if (!device) {
		fprintf(stderr, "Error: %S\n", KrGetLastError());
		exit(1);
	}

	KrUpdateAudioDevice(device);
	KrResumeAudioDevice(device);

	char input[1024];

	for (;;) {
		printf("> ");
		int n = scanf_s("%s", input, (int)sizeof(input));

		if (strcmp(input, "resume") == 0) {
			KrResumeAudioDevice(device);
		} else if (strcmp(input, "pause") == 0) {
			KrPauseAudioDevice(device);
		} else if (strcmp(input, "stop") == 0) {
			KrResetAudioDevice(device);
			CurrentSample = 0;
		} else if (strcmp(input, "reset") == 0) {
			KrResetAudioDevice(device);
			CurrentSample = 0;
			KrUpdateAudioDevice(device);
			KrResumeAudioDevice(device);
		} else if (strcmp(input, "quit") == 0) {
			break;
		} else if (strcmp(input, "list") == 0) {
			//PL_EnumerateAudioDevices();
		} else {
			printf("invalid command\n");
		}
	}

	KrCloseAudioDevice(device);

	return 0;
}
