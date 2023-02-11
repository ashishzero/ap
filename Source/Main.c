#include "KrMedia.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RENDER_MAX_FRAMES 512

static uint MaxFrame = 0;
static float CurrentFrame = 0;
static i16 *CurrentStream = 0;
static real CurrentVolume = 1;
static float SampleSpeed = 1;

u32 UploadAudioFrames(const KrAudioSpec *spec, u8 *dst, u32 count, void *user) {
	Assert(spec->Format == KrAudioFormat_R32 && spec->Channels == 2);

	umem size = count * spec->Channels;

	r32 *samples = (r32 *)dst;
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

		samples[i + 0] = CurrentVolume * l;
		samples[i + 1] = CurrentVolume * r;

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
} Riff_Header;

typedef struct Wave_Fmt {
	u8	id[4];
	u32 size;
	u16 audio_format;
	u16 channels_count;
	u32 sample_rate;
	u32 byte_rate;
	u16 block_align;
	u16 bits_per_sample;
} Wave_Fmt;

typedef struct Wave_Data {
	u8 id[4];
	u32 size;
} Wave_Data;

typedef struct Audio_Stream {
	Riff_Header header;
	Wave_Fmt	fmt;
	Wave_Data	data;
	i16			samples[1];
} Audio_Stream;

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
	if (event->Kind == KrEvent_Startup) {
		KrAudio_Update();
		KrAudio_Resume();
	}

	if (event->Kind == KrEvent_KeyPressed) {
		if (event->Key.Code == KrKey_Left || event->Key.Code == KrKey_Right) {
			int direction = event->Key.Code == KrKey_Left ? -1 : 1;
			int magnitude = event->Key.Repeat ? 5 : 10;
			float next = CurrentFrame + (float)(direction * magnitude) * 48000;
			next = Clamp(0.0f, (float)MaxFrame, next);
			CurrentFrame = next;
		}

		if (event->Key.Code == KrKey_Up || event->Key.Code == KrKey_Down) {
			float direction = event->Key.Code == KrKey_Down ? -1.0f : 1.0f;
			float magnitude = event->Key.Repeat ? 0.01f : 0.02f;
			float next = CurrentVolume + direction * magnitude;
			next = Clamp(0.0f, 1.0f, next);
			CurrentVolume = next;
		}
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
			float fraction = (float)(event->Key.Code - KrKey_0) / 10.0f;
			CurrentFrame = (float)lroundf(fraction * (float)MaxFrame);
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

typedef struct Quad {
	float p0[2];
	float p1[2];
	float p2[2];
	float p3[2];
} Quad;

Quad LerpQuad(Quad a, Quad b, float t) {
	Quad r;
	r.p0[0] = Lerp(a.p0[0], b.p0[0], t);
	r.p0[1] = Lerp(a.p0[1], b.p0[1], t);
	r.p1[0] = Lerp(a.p1[0], b.p1[0], t);
	r.p1[1] = Lerp(a.p1[1], b.p1[1], t);
	r.p2[0] = Lerp(a.p2[0], b.p2[0], t);
	r.p2[1] = Lerp(a.p2[1], b.p2[1], t);
	r.p3[0] = Lerp(a.p3[0], b.p3[0], t);
	r.p3[1] = Lerp(a.p3[1], b.p3[1], t);
	return r;
}

// TODO: cleanup this mess
proc void PL_DrawQuad(float p0[2], float p1[2], float p2[2], float p3[2], float color0[4], float color1[4], float color2[4], float color3[4]);
proc void PL_DrawRect(float x, float y, float w, float h, float color0[4], float color1[4]);
proc void PL_DrawRectVert(float x, float y, float w, float h, float color0[4], float color1[4]);

static float RenderingFrames[RENDER_MAX_FRAMES];

void Update(float w, float h, void *data) {
	const float PaddingX = 50.0f;
	const float PaddingY = 100.0f;
	const float Gap = 1.0f;
	const float MinHeight = 2.0f;

	w -= PaddingX * 2;
	h -= PaddingY * 2;
	h /= 2;

	float x = PaddingX;
	float y = PaddingY + h;

	float d = w / (float)RENDER_MAX_FRAMES;
	float g = d - Gap;

	Color base = { 1.0f, 1.0f, 0.0f, 1.0f };
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

	static float Progress;
	static float VolumeDisp;

	float progress_x = PaddingX;
	float progress_y = 20.0f;
	float progress_w = w;
	float progress_h = 10.0f;

	float progress_target = (progress_w * CurrentFrame) / (float)MaxFrame;

	float progress_bg[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float progress_fg[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	Color volume_fg_start = { 1.0f, 1.0f, 0.1f, 1.0f };
	Color volume_fg_stop = { 8.0f, 0.1f, 0.0f, 1.0f };

	Progress = Lerp(Progress, progress_target, 0.2f);
	VolumeDisp = Lerp(VolumeDisp, CurrentVolume, 0.2f);

	float volume_w = 10.0f;
	float volume_h = 150.0f;
	float volume_x = progress_x + progress_w - volume_w - PaddingX;
	float volume_y = progress_y + progress_h + 5.0f;

	Color volume_fg_mid = Mix(volume_fg_start, volume_fg_stop, VolumeDisp);

	PL_DrawRect(volume_x, volume_y, volume_w, volume_h, progress_bg, progress_bg);
	PL_DrawRectVert(volume_x, volume_y, volume_w, VolumeDisp * volume_h, volume_fg_start.m, volume_fg_mid.m);

	PL_DrawRect(progress_x, progress_y, progress_w, progress_h, progress_bg, progress_bg);
	PL_DrawRect(progress_x, progress_y, Progress, progress_h, progress_fg, progress_fg);

#define BUTTON_HALF_DIM  25.0F

	float button_x = PaddingX + w * 0.5f - BUTTON_HALF_DIM;
	float button_y = progress_y + progress_h + 15.0f;

	static Quad PauseButtonQuads[2] = {
		{
			.p0 = {0, 0},
			.p1 = {BUTTON_HALF_DIM - 5.0f, 0},
			.p2 = {BUTTON_HALF_DIM - 5.0f, 2.0f * BUTTON_HALF_DIM},
			.p3 = {0, 2.0f * BUTTON_HALF_DIM}
		},
		{
			.p0 = {BUTTON_HALF_DIM + 5.0f, 0},
			.p1 = {2.0f * BUTTON_HALF_DIM, 0},
			.p2 = {2.0f * BUTTON_HALF_DIM, 2.0f * BUTTON_HALF_DIM},
			.p3 = {BUTTON_HALF_DIM + 5.0f, 2.0f * BUTTON_HALF_DIM}
		}
	};

	static Quad PlayButtonQuads[2] = {
		{
			.p0 = {0, 2.0f * BUTTON_HALF_DIM},
			.p1 = {0, 0},
			.p2 = {2.0f * BUTTON_HALF_DIM, BUTTON_HALF_DIM},
			.p3 = {2.0f * BUTTON_HALF_DIM, BUTTON_HALF_DIM}
		},
		{
			.p0 = {0, 2.0f * BUTTON_HALF_DIM},
			.p1 = {0, 0},
			.p2 = {2.0f * BUTTON_HALF_DIM, BUTTON_HALF_DIM},
			.p3 = {2.0f * BUTTON_HALF_DIM, BUTTON_HALF_DIM}
		}
	};

	static Quad ButtonQuads[2];

	if (KrAudio_IsPlaying()) {
		for (int i = 0; i < 2; ++i) {
			ButtonQuads[i] = LerpQuad(ButtonQuads[i], PauseButtonQuads[i], 0.1f);
		}
	} else {
		for (int i = 0; i < 2; ++i) {
			ButtonQuads[i] = LerpQuad(ButtonQuads[i], PlayButtonQuads[i], 0.1f);
		}
	}

	for (int i = 0; i < 2; ++i) {
		Quad r = ButtonQuads[i];
		r.p0[0] = button_x + r.p0[0];
		r.p0[1] = button_y + r.p0[1];
		r.p1[0] = button_x + r.p1[0];
		r.p1[1] = button_y + r.p1[1];
		r.p2[0] = button_x + r.p2[0];
		r.p2[1] = button_y + r.p2[1];
		r.p3[0] = button_x + r.p3[0];
		r.p3[1] = button_y + r.p3[1];
		PL_DrawQuad(r.p0, r.p1, r.p2, r.p3, progress_fg, progress_fg, progress_fg, progress_fg);
	}


	//PL_DrawRect(button_x, button_y, button_w, button_h, progress_fg, progress_fg);
	//PL_DrawRect(button_x + button_w + pause_button_gap, button_y, button_w, button_h, progress_fg, progress_fg);

	//PL_DrawQuad(pause_button0.p0, pause_button0.p1, pause_button0.p2, pause_button0.p3, progress_fg, progress_fg, progress_fg, progress_fg);
	//PL_DrawQuad(pause_button1.p0, pause_button1.p1, pause_button1.p2, pause_button1.p3, progress_fg, progress_fg, progress_fg, progress_fg);
}

#define M_TAU_F 6.28318531f

typedef struct Complex {
	float r, i;
} Complex;

Complex CAdd(Complex a, Complex b) {
	return  (Complex) { .r = a.r + b.r, .i = a.i + b.i };
}

Complex CSub(Complex a, Complex b) {
	return  (Complex) { .r = a.r - b.r, .i = a.i - b.i };
}

Complex CMul(Complex a, Complex b) {
	return (Complex) { .r = a.r * b.r - a.i * b.i, .i = a.r * b.i + a.i * b.r };
}

Complex CEuler(float e) {
	return (Complex) {
		.r = cosf(e),
			.i = sinf(e)
	};
}

uint BitLength(uint number) {
	uint result = 0;
	while (number) {
		number = number >> 1;
		result += 1;
	}
	return result - 1;
}

uint ReverseBits(uint number, uint bits) {
	uint reverse = 0;
	for (; number; number = number >> 1) {
		reverse = (reverse << 1) | (number & 1);
		bits -= 1;
	}
	reverse = reverse << bits;
	return reverse;
}

void BitReversePermutation(Complex *x, uint count) {
	uint bits = BitLength(count);

	uint max = (count >> 1);
	for (uint index = 1; index < max; ++index) {
		uint reverse = ReverseBits(index, bits);
		Complex y = x[index];
		x[index] = x[reverse];
		x[reverse] = y;
	}
}

#if 0
void FFTRecursive(Complex *output, Complex *input, uint count) {
	Assert(count > 1);

	if (count == 2) {
		output[0] = CAdd(input[0], input[1]);
		output[1] = CSub(input[0], input[1]);
		return;
	}

	float twid = -M_TAU_F / (float)count;

	count >>= 1;

	FFTRecursive(output, input, count);
	FFTRecursive(output + count, input + count, count);

	uint k0 = 0;
	uint k1 = count;

	for (uint k = 0; k < count; ++k) {
		Complex w = CEuler(twid * (float)k);
		Complex p = output[k0];
		Complex q = CMul(w, output[k1]);
		output[k0] = CAdd(p, q);
		output[k1] = CSub(p, q);

		k0 += 1;
		k1 += 1;
	}
}

void FFT(Complex *output, Complex *input, uint count) {
	BitReversePermutation(input, count);
	FFTRecursive(output, input, count);

	for (uint index = 0; index < count; ++index) {
		printf("%f, %f\n", output[index].r, output[index].i);
	}
}

#else

bool IsPowerOf2(uint number) {
	return (number & (number - 1)) == 0;
}

void FFTCopyBitReversed(Complex *dst, Complex *src, uint count) {
	uint bits = BitLength(count);
	uint max = (count >> 1);

	for (uint index = 0; index < count; ++index) {
		uint reverse = ReverseBits(index, bits);
		dst[reverse] = src[index];
	}
}

void FFT(Complex *output, Complex *input, uint count) {
	Assert(IsPowerOf2(count));

	FFTCopyBitReversed(output, input, count);

	// radix = 2
	for (uint index = 0; index < count; index += 2) {
		Complex p = output[index + 0];
		Complex q = output[index + 1];
		output[index + 0] = CAdd(p, q);
		output[index + 1] = CSub(p, q);
	}

	// radix = 4
	for (uint index = 0; index < count; index += 4) {
		Complex p = output[index + 0];
		Complex r = output[index + 1];
		Complex q = output[index + 2];
		Complex s = output[index + 3];

		s = (Complex) { s.i, -s.r };

		output[index + 0] = CAdd(p, q);
		output[index + 1] = CAdd(r, s);
		output[index + 2] = CSub(p, q);
		output[index + 3] = CSub(r, s);
	}

	uint division = 4;
	for (uint radix = 8; radix <= count; radix = radix << 1) {
		float twid = -M_TAU_F / (float)radix;

		for (uint first = 0; first < count; first += radix) {
			for (uint index = 0; index < division; ++index) {
				uint idx0 = first + index;
				uint idx1 = idx0 + division;
				Complex w = CEuler(twid * (float)index); // @Todo: Cache these values
				Complex p = output[idx0];
				Complex q = CMul(w, output[idx1]);
				output[idx0] = CAdd(p, q);
				output[idx1] = CSub(p, q);
			}
		}

		division = division << 1;
	}
}
#endif

int Main(int argc, char **argv, KrUserContext *ctx) {
	Complex input[] = { {2},{1},{-1},{5},{0},{3},{0},{-4} };
	Complex output[8];

	memset(output, 0, sizeof(output));

	FFT(output, input, ArrayCount(input));

	for (uint index = 0; index < ArrayCount(output); ++index) {
		printf("%f, %f\n", output[index].r, output[index].i);
	}

	if (argc <= 1) {
		Usage(argv[0]);
	}

	const char *path = argv[1];

	umem size = 0;
	u8 *buff = ReadEntireFile(path, &size);
	Audio_Stream *audio = (Audio_Stream *)buff;

	CurrentStream = Serialize(audio, &MaxFrame);

	ctx->OnEvent = HandleEvent;
	ctx->OnUpdate = Update;
	ctx->OnUploadAudio = UploadAudioFrames;

	return 0;
}
