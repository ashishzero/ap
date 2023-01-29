#include "KrMedia.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static uint MaxFrame      = 0;
static float CurrentFrame = 0;
static i16 *CurrentStream = 0;
static real Volume        = 1;
static float SampleSpeed  = 1;

u32 UploadAudioFrames(const KrAudioSpec *spec, u8 *dst, u32 count, void *user) {
	Assert(spec->Format == KrAudioFormat_R32 && spec->Channels == 2);

	umem size = count * spec->Channels;

	r32 *samples  = (r32 *)dst;
	for (uint i = 0; i < size; i += spec->Channels) {
		uint x = (uint)CurrentFrame;
		uint y = x + 1;

		float lx = (float)CurrentStream[x * spec->Channels + 0] / 32767.0f;
		float ly = (float)CurrentStream[y * spec->Channels + 0] / 32767.0f;
		float rx = (float)CurrentStream[x * spec->Channels + 1] / 32767.0f;
		float ry = (float)CurrentStream[y * spec->Channels + 1] / 32767.0f;

		float t = CurrentFrame - x;

		float l = (1.0f - t) * lx + t * ly;
		float r = (1.0f - t) * rx + t * ry;

		samples[i + 0] = Volume * l;
		samples[i + 1] = Volume * r;

		CurrentFrame += SampleSpeed;

		if (CurrentFrame >= (float)MaxFrame) {
			CurrentFrame = fmodf(CurrentFrame, (float)MaxFrame);
		}
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

u8 *ReadEntireFile(const char *path, umem *size) {
	FILE *fp = fopen(path, "rb");
	if (!fp) {
		fprintf(stderr, "Failed to open file: %s. Reason: %s\n", path, strerror(errno));
		exit(1);
	}
	fseek(fp, 0, SEEK_END);
	*size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	u8 *data = malloc(*size);
	fread(data, *size, 1, fp);
	fclose(fp);
	return data;
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
	*count /= audio->fmt.channels_count;

	return (i16 *)(ptr + 1);
}

void HandleEvent(const KrEvent *event, void *user) {
	if (event->Kind == KrEventKind_Startup) {
		KrAudioUpdate();
		KrAudioResume();
	}

	printf("Event: %s\n", KrEventNamed(event->Kind));
}

void Update(void *data) {
}

int Main(int argc, char **argv, KrUserContext *ctx) {
	if (argc <= 1){
		Usage(argv[0]);
	}

	const char *path = argv[1];

	umem size = 0;
	u8 *buff = ReadEntireFile(path, &size);
	Audio_Stream *audio = (Audio_Stream *)buff;

	CurrentStream = Serialize(audio, &MaxFrame);

	ctx->OnEvent       = HandleEvent;
	ctx->OnUpdate      = Update;
	ctx->OnUploadAudio = UploadAudioFrames;

	return 0;
}
