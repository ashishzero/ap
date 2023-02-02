#include "KrMedia.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RENDER_MAX_FRAMES 500

static uint MaxFrame      = 0;
static float CurrentFrame = 0;
static i16 *CurrentStream = 0;
static real Volume        = 1;
static float SampleSpeed  = 1;

//static float RenderingFrames[RENDER_MAX_FRAMES];

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

		// This is very very slow!!! Optimize!!!
		//memmove(RenderingFrames, RenderingFrames + 1, sizeof(float) * (RENDER_MAX_FRAMES - 1));
		//RenderingFrames[RENDER_MAX_FRAMES-1] = 0.5f * (l + r);
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
	//printf("Event: %s\n", KrEvent_GetName(event->Kind));

	if (event->Kind == KrEvent_Startup) {
		KrAudio_Update();
		KrAudio_Resume();
	}

	if (event->Kind == KrEvent_AudioRenderDeviceChanged) {
		printf("Render device changed: %s\n", event->AudioDevice.Name);
	} else if (event->Kind == KrEvent_AudioRenderDeviceActived) {
		printf("Activated render device: %s\n", event->AudioDevice.Name);
	} else if (event->Kind == KrEvent_AudioRenderDeviceDeactived) {
		printf("Deactivated render device: %s\n", event->AudioDevice.Name);
	} else if (event->Kind == KrEvent_AudioRenderDeviceLost) {
		printf("ERROR: No audio render device\n");
	}

	if (event->Kind == KrEvent_KeyPressed && !event->Key.Repeat) {
		if (event->Key.Code == KrKey_Space) {
			if (KrAudio_IsPlaying()) {
				KrAudio_Pause();
			} else {
				KrAudio_Resume();
			}
		}
		if (event->Key.Code == KrKey_F11) {
			KrWindow_ToggleFullscreen();
		}

		if (event->Key.Code == KrKey_Plus) {
			SampleSpeed *= 2.0f;
		} else if (event->Key.Code == KrKey_Minus) {
			SampleSpeed *= 0.5f;
		}
		
		if (event->Key.Code >= KrKey_0 && event->Key.Code <= KrKey_9) {
			uint i = event->Key.Code - KrKey_0;
			if (!i) {
				printf("Setting render device to default\n");
				KrAudio_SetRenderDevice(0);
			} else {
				KrAudioDeviceInfo infos[10];
				uint n = KrAudio_GetDeviceList(KrAudioDeviceFlow_Render, false, infos, ArrayCount(infos));
				if (i - 1 < n) {
					KrAudio_SetRenderDevice(infos[i - 1].Id);
					printf("Setting render device to %s\n", infos[i - 1].Name);
				}
			}
		}
	}
}

typedef struct Color {
	float m[4];
} Color;

Color Mix(Color a, Color b, float t) {
	Color r;
	for (int i = 0; i < 4; ++i) {
		r.m[i] = (1.0f - t) * a.m[i] + t * b.m[i];
	}
	return r;
}


float Lerp(float a, float b, float t) {
	return (1.0f - t) * a + t * b;
}

// TODO: cleanup this mess
proc void PL_DrawRect(float x, float y, float w, float h, float color0[4], float color1[4]);

static float RenderingFrames[RENDER_MAX_FRAMES];

void Update(float w, float h, void *data) {
	const float PaddingX  = 50.0f;
	const float PaddingY  = 100.0f;
	const float Gap       = 2.0f;
	const float MinHeight = 2.0f;

	w -= PaddingX * 2;
	h -= PaddingY * 2;
	h /= 2;

	float x = PaddingX;
	float y = PaddingY + h;

	float d = w / (float)RENDER_MAX_FRAMES;
	float g = d - Gap;

	Color base  = { 1.0f, 1.0f, 0.0f, 1.0f };
	Color highy = { 1.0f, 0.0f, 0.0f, 1.0f };
	Color highx = { 0.0f, 1.0f, 0.0f, 1.0f };

	u32 frame_pos = (uint)CurrentFrame;

	for (i32 index = 0; index < RENDER_MAX_FRAMES; ++index) {
		float l = (float)CurrentStream[frame_pos * 2 + 0] / 32767.0f;
		float r = (float)CurrentStream[frame_pos * 2 + 1] / 32767.0f;
		float val = 0.5f * (l + r);

		RenderingFrames[index] = Lerp(RenderingFrames[index], val, 0.2f);

		frame_pos += 1;
		if (frame_pos >= MaxFrame) {
			frame_pos = 0;
		}
	}

	for (i32 index = 0; index < RENDER_MAX_FRAMES; ++index) {
		float val = RenderingFrames[index];

		float midx = (float)index / (float)RENDER_MAX_FRAMES;
		float midy = fabsf(val);
		Color basex = Mix(base, highx, midx);
		Color midcolor = Mix(basex, highy, midy);
		
		if (val >= 0.0f) {
			float render_h = val * h + MinHeight * 2;
			PL_DrawRect(x, y - MinHeight, g, render_h, base.m, midcolor.m);
		} else {
			float render_h = -val * h + MinHeight;
			PL_DrawRect(x, y - render_h, g, render_h + MinHeight, base.m, midcolor.m);
		}

		x += d;
	}
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
