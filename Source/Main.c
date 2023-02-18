#include "KrMedia.h"
#include "FFT.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define RENDER_MAX_FRAMES 512

static uint  MaxFrame      = 0;
static uint  CurrentFrame  = 0;
static i16 * CurrentStream = 0;
static real  CurrentVolume = 1;
static bool  Transform     = false;

#define RENDER_MAX_SPECTRUM 1024
#define FFT_MIN_FILTER      0
#define FFT_MAX_FILTER      (RENDER_MAX_SPECTRUM-FFT_MIN_FILTER)

static Complex ShiftedFFTBuffer[RENDER_MAX_SPECTRUM];
static Complex FFTBuffer[RENDER_MAX_SPECTRUM];
static Complex WindowBuffer[RENDER_MAX_SPECTRUM];

void ApplyLowPassFilter(Complex *values, uint count, uint threshold_freq) {
	for (uint index = threshold_freq + 1; index < count; ++index) {
		values[index] = ComplexRect(0, 0);
	}
}

void ApplyHighPassFilter(Complex *values, uint count, uint threshold_freq) {
	for (uint index = 0; index < threshold_freq; ++index) {
		values[index] = ComplexRect(0, 0);
	}
}

void ApplyBandPassFilter(Complex *values, uint count, uint threshold_a, uint threshold_b) {
	for (uint index = 0; index < threshold_a; ++index) {
		values[index] = ComplexRect(0, 0);
	}
	for (uint index = threshold_b + 1; index < count; ++index) {
		values[index] = ComplexRect(0, 0);
	}
}

float ComplexLenSq(Complex c) {
	return c.re * c.re + c.im * c.im;
}

float PreApplyTransformations(Complex *data, uint count) {
	if (!Transform) return 1.0f;

	float sum = 0.0f;

#if 1
	// Blackman window
	for (uint index = 0; index < count; ++index) {
		float alpha = 0.16f;
		float a0 = (1.0f - alpha) / 2.0f;
		float a1 = 1.0f / 2.0f;
		float a2 = alpha / 2.0f;
		float div = (float)index / (float)count;
		float window_x = a0 - a1 * cosf(2 * 3.14f * div) + a2 * cosf(4 * 3.14f * div);
		data[index].re *= window_x;
		sum += window_x;
	}
#endif

#if 0
	// Sine window
	for (uint index = 0; index < count; ++index) {
		float freq = 40.0f;
		float div  = (float)index / (float)count;
		float window_x = sinf(2.0f * 3.14f * freq * div);
		data[index].re *= window_x;
	}
#endif

#if 0
	// Rect window
	for (uint index = 0; index < count; ++index) {
		float alpha = 0.16f;
		data[index].re *= alpha;
	}
#endif

	return sum;
}

void ApplyTransformations(Complex *data, uint count, uint first, uint max) {
	if (!Transform) return;

	#if 1
	//for (uint i = 0; i < count; ++i) {
	//	data[i] = ComplexMul(data[i], WindowBuffer[i]);
	//}
	#endif

	//for (uint idx = 0; idx < count; idx += 2) {
	//	data[idx] = ComplexRect(0, 0);
	//}
	//for (uint idx = 128; idx < count; ++idx) {
	//	data[idx] = ComplexRect(0, 0);
	//}

	//for (int iter = 0; iter < 60; ++iter) {
	//	float max_mag = 0.0f;
	//	uint  max_idx = 0;

	//	for (uint idx = 0; idx < RENDER_MAX_SPECTRUM; ++idx) {
	//		float mag = ComplexLenSq(data[idx]);
	//		if (mag > max_mag) {
	//			max_mag = mag;
	//			max_idx = idx;
	//		}
	//	}

	//	data[max_idx] = ComplexRect(0, 0);
	//}

	
	//for (uint index = 100; index < RENDER_MAX_SPECTRUM-100; ++index) {
	//	data[index] = ComplexRect(0, 0);
	//}

	//ApplyHighPassFilter(data, count, 100);
	//ApplyLowPassFilter(data, count, 1024-100);
}

u32 UploadAudioFrames(const KrAudioSpec *spec, u8 *dst, u32 count, void *user) {
	Assert(spec->Format == KrAudioFormat_R32 && spec->Channels == 2);

	uint sample_count = count * 2;
	float *write_ptr  = (float *)dst;
	float *last_ptr   = write_ptr + sample_count;

	uint frame = CurrentFrame;
	while (write_ptr < last_ptr) {
		// @Hack: Currently just working with 1 channel

		memset(FFTBuffer, 0, sizeof(FFTBuffer));

		ptrdiff_t input_size = Min(ArrayCount(FFTBuffer)/2, (last_ptr - write_ptr)/2);
		for (ptrdiff_t index = 0; index < input_size; index += 1) {
			FFTBuffer[index + 0] = ComplexRect((float)CurrentStream[frame * 2 + 0] / 32767.0f, 0);
			//FFTBuffer[index + 1] = ComplexRect((float)CurrentStream[frame * 2 + 1] / 32767.0f, 0);
			frame = (frame + 1) % MaxFrame;
		}

		ptrdiff_t target = ArrayCount(FFTBuffer)/2;
		for (ptrdiff_t index = 0; index < ArrayCount(FFTBuffer)/2; index += 1) {
			ShiftedFFTBuffer[target++] = FFTBuffer[index];
		}
		target = 0;
		for (ptrdiff_t index = ArrayCount(FFTBuffer)/2; index < ArrayCount(FFTBuffer); index += 1) {
			ShiftedFFTBuffer[target++] = FFTBuffer[index];
		}

		float f = PreApplyTransformations(ShiftedFFTBuffer, ArrayCount(ShiftedFFTBuffer));

		InplaceFFT(ShiftedFFTBuffer, ArrayCount(ShiftedFFTBuffer));
		ApplyTransformations(ShiftedFFTBuffer, ArrayCount(ShiftedFFTBuffer), CurrentFrame, MaxFrame);
		InplaceInvFFT(ShiftedFFTBuffer, ArrayCount(ShiftedFFTBuffer));

		target = ArrayCount(FFTBuffer)/2;
		for (ptrdiff_t index = 0; index < ArrayCount(FFTBuffer)/2; index += 1) {
			FFTBuffer[target++] = ShiftedFFTBuffer[index];
		}
		target = 0;
		for (ptrdiff_t index = ArrayCount(FFTBuffer)/2; index < ArrayCount(FFTBuffer); index += 1) {
			FFTBuffer[target++] = ShiftedFFTBuffer[index];
		}

		for (uint index = 0; write_ptr < last_ptr && index < ArrayCount(FFTBuffer); ++index) {
			write_ptr[0] = f * FFTBuffer[index].re;
			write_ptr[1] = f * FFTBuffer[index].re;
			write_ptr += 2;
		}
	}

	CurrentFrame = frame; //(CurrentFrame + count) % MaxFrame;

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
			int next = CurrentFrame + (direction * magnitude) * 48000;
			next = Clamp(0, (int)MaxFrame, next);
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
		if (event->Key.Code == KrKey_T) {
			Transform = !Transform;
		}
		if (event->Key.Code == KrKey_F11) {
			KrWindow_ToggleFullscreen();
		}

		if (event->Key.Code >= KrKey_0 && event->Key.Code <= KrKey_9) {
			float fraction = (float)(event->Key.Code - KrKey_0) / 10.0f;
			CurrentFrame = (uint)lroundf(fraction * (float)MaxFrame);
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

static float   RenderingFrames[RENDER_MAX_FRAMES];
static float   SpectrumMagnitudes[RENDER_MAX_SPECTRUM];
static float   SpectrumMagnitudesTarget[RENDER_MAX_SPECTRUM];
static Complex SpectrumScratchShift[RENDER_MAX_SPECTRUM];
static Complex SpectrumScratch[RENDER_MAX_SPECTRUM];

void Update(float window_w, float window_h, void *data) {
	const float PaddingX = 50.0f;
	const float PaddingY = 100.0f;
	const float Gap = 1.0f;
	const float MinHeight = 2.0f;

	float w = window_w;
	float h = window_h;

	w -= PaddingX * 2;
	h -= PaddingY * 2;
	h /= 2;

	float x = PaddingX;
	float y = PaddingY + h / 2;

	float d = w / (float)RENDER_MAX_FRAMES;
	float g = d - Gap;

	Color base  = { 1.0f, 1.0f, 0.0f, 1.0f };
	Color highy = { 1.0f, 0.0f, 0.0f, 1.0f };
	Color highx = { 0.0f, 1.0f, 0.0f, 1.0f };

	u32 first_frame = (uint)CurrentFrame;
	u32 frame_pos = first_frame;

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

	frame_pos = first_frame;
	for (i32 index = 0; index < RENDER_MAX_SPECTRUM; index += 1) {
		SpectrumScratchShift[index + 0] = ComplexRect(CurrentStream[frame_pos * 2 + 0] / 32767.0f, 0);
		//SpectrumScratch[index + 1] = ComplexRect(CurrentStream[frame_pos * 2 + 1] / 32767.0f, 0);

		frame_pos += 1;
		if (frame_pos >= MaxFrame) {
			frame_pos = 0;
		}
	}

	i32 target = RENDER_MAX_SPECTRUM/2;
	for (i32 index = 0; index < RENDER_MAX_SPECTRUM/2; index += 1) {
		SpectrumScratch[target++] = SpectrumScratchShift[index];
	}
	target = 0;
	for (i32 index = RENDER_MAX_SPECTRUM/2; index < RENDER_MAX_SPECTRUM; ++index) {
		SpectrumScratch[target++] = SpectrumScratchShift[index];
	}

	PreApplyTransformations(SpectrumScratch, ArrayCount(SpectrumScratch));

	InplaceFFT(SpectrumScratch, ArrayCount(SpectrumScratch));
	ApplyTransformations(SpectrumScratch, ArrayCount(SpectrumScratch), first_frame, MaxFrame);

	float max_spec_mag = -1.0f;
	i32   max_spec_idx = -1;

	for (i32 index = 0; index < RENDER_MAX_SPECTRUM; ++index) {
		Complex z = SpectrumScratch[index];
		float re2 = z.re * z.re;
		float im2 = z.im * z.im;
		SpectrumMagnitudesTarget[index] = sqrtf(re2 + im2);

		if (max_spec_mag < SpectrumMagnitudesTarget[target]) {
			max_spec_mag = SpectrumMagnitudesTarget[target];
			max_spec_idx = index;
		}
	}

	//printf("Max: %d, %f\n", max_spec_idx, max_spec_mag);

	for (i32 index = 0; index < RENDER_MAX_SPECTRUM; ++index) {
		SpectrumMagnitudesTarget[index] = log10f(1.0f + SpectrumMagnitudesTarget[index]);
	}

	for (i32 index = 0; index < RENDER_MAX_SPECTRUM; ++index) {
		SpectrumMagnitudes[index] = Lerp(SpectrumMagnitudes[index], SpectrumMagnitudesTarget[index], 0.2f);
	}

	float spec_color_a[] = { 1,1,0,1 };
	float spec_color_b[] = { 1,0,0,1 };

	float spec_g = 0.2f;
	float spec_w = 1.0f;
	float spec_x = 0.5f * (window_w - (spec_w + spec_g) * RENDER_MAX_SPECTRUM);
	float spec_y = y + 0.4f * h;
	float spec_min_height = 2.0f;
	float spec_max_height = 100.0f;
	for (i32 index = 0; index < RENDER_MAX_SPECTRUM; ++index) {
		float val = SpectrumMagnitudes[index];

		float spec_h = val * spec_max_height;
		PL_DrawRectVert(spec_x, spec_y, spec_w, spec_min_height + spec_h, spec_color_a, spec_color_b);
		spec_x += (spec_w + spec_g);
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

i16 *GenerateSineWave(float freq, uint *count) {
	uint samples = 2 * 48000 / (uint)freq;

	float dt = 1.0f / 48000.0f;

	i16 *data = malloc(sizeof(i16) * samples);
	memset(data, 0, sizeof(i16) * samples);

	float t = 0;
	for (uint i = 0; i < samples; i += 2) {
		float s = 32767.0f * sinf(2.0f * 3.14f * freq * t);

		i16 quantized = (i16)s;
		data[i + 0] = quantized;
		data[i + 1] = quantized;

		t += dt;
	}

	*count = samples / 2;

	return data;
}

int Main(int argc, char **argv, KrUserContext *ctx) {
#if 0
	Complex data[] = { {2},{1},{-1},{5},{0},{3},{0},{-4} };

	printf("===============================================\n");

	for (uint index = 0; index < ArrayCount(data); ++index) {
		printf("%f, %f\n", data[index].re, data[index].im);
	}

	InplaceFFT(data, ArrayCount(data));

	printf("===============================================\n");

	for (uint index = 0; index < ArrayCount(data); ++index) {
		printf("%f, %f\n", data[index].re, data[index].im);
	}

	printf("===============================================\n");

	InplaceInvFFT(data, ArrayCount(data));

	for (uint index = 0; index < ArrayCount(data); ++index) {
		printf("%f, %f\n", data[index].re, data[index].im);
	}

	printf("===============================================\n");
#endif

	if (argc <= 1) {
		Usage(argv[0]);
	}

	const char *path = argv[1];

	umem size = 0;
	u8 *buff = ReadEntireFile(path, &size);
	Audio_Stream *audio = (Audio_Stream *)buff;

	CurrentStream = Serialize(audio, &MaxFrame);
	//CurrentStream = GenerateSineWave(1024, &MaxFrame);

	ctx->OnEvent = HandleEvent;
	ctx->OnUpdate = Update;
	ctx->OnUploadAudio = UploadAudioFrames;

	return 0;
}
