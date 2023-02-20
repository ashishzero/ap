#include "KrMedia.h"
#include "FFT.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

float Lerp(float a, float b, float t) {
	return (1.0f - t) * a + t * b;
}

#define MAX_RENDER_FRAMES  512
#define MAX_PROCESS_FRAMES 512

typedef struct AudioRenderBuffer {
	float    TimeDomain[MAX_RENDER_FRAMES];
	Complex  FreqDomain[MAX_RENDER_FRAMES];
} AudioRenderBuffer;

struct {
	void    *Frames;
	float    Pos;
	uint     Last;
	float    Volume;
	float    FreqRatio;

	// Rendering data
	uint              RenderBufIndex;
	AudioRenderBuffer RenderBufTarget[2];
	AudioRenderBuffer RenderBuf;
} G;

typedef i16 PCM16FrameMono;

typedef struct PCM16FrameStereo {
	i16 Left;
	i16 Right;
} PCM16FrameStereo;

typedef struct F32FrameStereo {
	float Left;
	float Right;
} F32FrameStereo;

static uint InFreq = 0; // @todo: cleanup

float Wrap(float min, float a, float max) {
	float range  = max - min;
	float offset = a - min;
	float result = (offset - (floorf(offset / range) * range) + min);
	return result;
}

float MapRange(float in_a, float in_b, float out_a, float out_b, float v) {
	return (out_b - out_a) / (in_b - in_a) * (v - in_a) + out_a;
}

F32FrameStereo LoadFrame(PCM16FrameStereo *src, float frame, uint last) {
	float index;
	float alpha = modff(frame, &index);
	uint pos    = (uint)index;

	PCM16FrameStereo pcm_a = src[pos];
	PCM16FrameStereo pcm_b = src[(pos - 1) % last];

	F32FrameStereo a = {
		.Left  = MapRange(INT16_MIN, INT16_MAX, -1.0f, 1.0f, pcm_a.Left),
		.Right = MapRange(INT16_MIN, INT16_MAX, -1.0f, 1.0f, pcm_a.Right)
	};

	F32FrameStereo b = {
		.Left  = MapRange(INT16_MIN, INT16_MAX, -1.0f, 1.0f, pcm_b.Left),
		.Right = MapRange(INT16_MIN, INT16_MAX, -1.0f, 1.0f, pcm_b.Right)
	};

	F32FrameStereo out = {
		.Left  = Lerp(a.Left, b.Left, alpha),
		.Right = Lerp(a.Right, b.Right, alpha),
	};

	return out;
}

#define HISTORY_COUNT 3

static F32FrameStereo HistoryIn[HISTORY_COUNT];
static F32FrameStereo HistoryOut[HISTORY_COUNT];
static float          Alpha[HISTORY_COUNT];
static float          Beta[HISTORY_COUNT];

void ClearHistory() {
	memset(HistoryIn, 0, sizeof(HistoryIn));
	memset(HistoryOut, 0, sizeof(HistoryOut));

	Alpha[0] = 0.99f;
	Beta[0]  = 1.0f - Alpha[0];
}

void UpdateInputHistory(F32FrameStereo in) {
	for (int i = ArrayCount(HistoryIn) - 1; i > 0; --i) {
		HistoryIn[i] = HistoryIn[i - 1];
	}
	HistoryIn[0]  = in;
}

void UpdateOutputHistory(F32FrameStereo out) {
	for (int i = ArrayCount(HistoryOut) - 1; i > 0; --i) {
		HistoryOut[i] = HistoryOut[i - 1];
	}
	HistoryOut[0] = out;
}

static float QFreq   = 0.1f;
static float QFactor = 0.0f;

void CalcCoefficients(float freq) {
	float f = MapRange(0.0f, 1.0f, 0.0f, 22000.0f, QFreq);
	float q = MapRange(0.0f, 1.0f, 0.1f, 10.0f, QFactor);

	float k = tanf(3.14f * f / freq);
	float norm = 1.0f / (1 + k / q + k * k);

	Beta[0] = k * k * norm;
	Beta[1] = 2.0f * Beta[0];
	Beta[2] = Beta[0];

	Alpha[0] = 2.0f * (k * k - 1) * norm;
	Alpha[1] = (1 - k / q + k * k) * norm;
	Alpha[2] = 0.0f;
}

void NextWaveForm();
void PrevWaveForm();

#define FFT_LENGTH    2048
#define WINDOW_LENGTH 1024
#define HOP_LENGTH    512

static const int WindowLength         = WINDOW_LENGTH;
static const int HopLength            = HOP_LENGTH;
static const int FFTLength            = FFT_LENGTH;
static const int InputFrameLength     = WINDOW_LENGTH;
static const int OutputFrameLength    = WINDOW_LENGTH;

static F32FrameStereo InputFrames[WINDOW_LENGTH];
static Complex        FFTBufferL[FFT_LENGTH];
static Complex        FFTBufferR[FFT_LENGTH];
static F32FrameStereo OutputFrames[WINDOW_LENGTH];
static int            OutputFramePos = HOP_LENGTH;

void Transform(Complex *buf, uint length) {
}

void Process(const KrAudioSpec *spec) {
	if (OutputFramePos < HopLength)
		return;

	OutputFramePos = 0;

	PCM16FrameStereo *src = (PCM16FrameStereo *)G.Frames;

	memcpy(InputFrames, InputFrames + HopLength, (InputFrameLength - HopLength) * sizeof(F32FrameStereo));

	for (int index = InputFrameLength - HopLength; index < WindowLength; ++index) {
		InputFrames[index] = LoadFrame(src, G.Pos, G.Last);
		G.Pos += (G.FreqRatio * (float)InFreq / (float)spec->Frequency);

		// TODO: correct transition
		if (G.Pos <= 0.0f) {
			PrevWaveForm();
		} else if (G.Pos >= G.Last) {
			NextWaveForm();
		}
	}

	Assert(InputFrameLength >= WindowLength);

	for (int index = 0; index < WindowLength; ++index) {
		FFTBufferL[index].re = InputFrames[index].Left;
		FFTBufferR[index].re = InputFrames[index].Right;
		FFTBufferL[index].im = 0.0f;
		FFTBufferR[index].im = 0.0f;
	}
	for (int index = WindowLength; index < FFTLength; ++index) {
		FFTBufferL[index].re = 0.0f;
		FFTBufferR[index].re = 0.0f;
		FFTBufferL[index].im = 0.0f;
		FFTBufferR[index].im = 0.0f;
	}

	InplaceFFT(FFTBufferL, FFTLength);
	InplaceFFT(FFTBufferR, FFTLength);

	Transform(FFTBufferL, FFTLength);
	Transform(FFTBufferR, FFTLength);

	InplaceInvFFT(FFTBufferL, FFTLength);
	InplaceInvFFT(FFTBufferR, FFTLength);

	memcpy(OutputFrames, OutputFrames + HopLength, (OutputFrameLength - HopLength) * sizeof(F32FrameStereo));
	memset(OutputFrames + OutputFrameLength - HopLength, 0, HopLength * sizeof(F32FrameStereo));

	for (int index = 0; index < WindowLength; ++index) {
		OutputFrames[index].Left += FFTBufferL[index].re;
		OutputFrames[index].Right += FFTBufferR[index].re;
	}
}

u32 UploadAudioFrames(const KrAudioSpec *spec, u8 *buf, u32 count, void *user) {
	Assert(spec->Format == KrAudioFormat_R32 && spec->Channels == 2);

	F32FrameStereo   *dst = (F32FrameStereo *)buf;
	F32FrameStereo   *end = dst + count;
	//PCM16FrameStereo *src = (PCM16FrameStereo *)G.Frames;

	while (dst < end) {
		Process(spec);

		F32FrameStereo out = OutputFrames[OutputFramePos];
		OutputFramePos += 1;

		dst->Left  = G.Volume * out.Left;
		dst->Right = G.Volume * out.Right;

		dst += 1;
	}

#if 1
	// @Todo: support for multiple channels
	F32FrameStereo *out = (F32FrameStereo*)buf;

	uint next_buf_index = (G.RenderBufIndex + 1) & 1;

	if (count < MAX_RENDER_FRAMES) {
		uint offset = MAX_RENDER_FRAMES - count;
		memcpy(G.RenderBufTarget[next_buf_index].TimeDomain, G.RenderBufTarget[G.RenderBufIndex].TimeDomain + count, offset * sizeof(float));
		for (uint i = 0; i < count; ++i) {
			G.RenderBufTarget[next_buf_index].TimeDomain[offset + i] = out[i].Left;
		}
	} else {
		uint offset = count - MAX_RENDER_FRAMES;
		for (uint i = 0; i < MAX_RENDER_FRAMES; ++i) {
			G.RenderBufTarget[next_buf_index].TimeDomain[i] = out[offset + i].Left;
		}
	}

	memset(G.RenderBufTarget[next_buf_index].FreqDomain, 0, sizeof(G.RenderBufTarget[next_buf_index].FreqDomain));

	for (uint i = 0; i < MAX_RENDER_FRAMES; ++i) {
		G.RenderBufTarget[next_buf_index].FreqDomain[i].re = G.RenderBufTarget[next_buf_index].TimeDomain[i];
	}

	InplaceFFT(G.RenderBufTarget[next_buf_index].FreqDomain, MAX_RENDER_FRAMES);

	G.RenderBufIndex = next_buf_index;
#endif

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

static uint           WaveFormIndex;
static uint           WaveFormCount;
static Audio_Stream **WaveForms;

i16 *Serialize(Audio_Stream *audio, uint *count, uint *freq) {
	Wave_Data *ptr = &audio->data;
	for (; ptr->id[0] == 'L';) {
		uint sz = ptr->size;
		ptr += 1;
		ptr = (Wave_Data *)((u8 *)ptr + sz);
	}

	*count = ptr->size / sizeof(i16);
	*count /= audio->fmt.channels_count;

	*freq = audio->fmt.sample_rate;

	return (i16 *)(ptr + 1);
}

void NextWaveForm() {
	WaveFormIndex = (WaveFormIndex + 1) % WaveFormCount;
	G.Pos = 0;
	ClearHistory();
	G.Frames = Serialize(WaveForms[WaveFormIndex], &G.Last, &InFreq);
}

void PrevWaveForm() {
	WaveFormIndex = (WaveFormIndex - 1) % WaveFormCount;
	G.Pos = 0;
	ClearHistory();
	G.Frames = Serialize(WaveForms[WaveFormIndex], &G.Last, &InFreq);
}

void HandleEvent(const KrEvent *event, void *user) {
	if (event->Kind == KrEvent_Startup) {
		KrAudio_Update();
		KrAudio_Resume();
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
	}

	if (event->Kind == KrEvent_KeyPressed) {
		if (event->Key.Code == KrKey_Q) {
			QFactor = Clamp(0.0f, 1.0f, QFactor - 0.1f);
			printf("Q = %f\n", QFactor);
		} else if (event->Key.Code == KrKey_W) {
			QFactor = Clamp(0.0f, 1.0f, QFactor + 0.1f);
			printf("Q = %f\n", QFactor);
		}

		if (event->Key.Code == KrKey_A) {
			QFreq = Clamp(0.0f, 1.0f, QFreq - 0.1f);
			printf("QFreq = %f\n", QFreq);
		} else if (event->Key.Code == KrKey_S) {
			QFreq = Clamp(0.0f, 1.0f, QFreq + 0.1f);
			printf("QFreq = %f\n", QFreq);
		}

		if (event->Key.Code == KrKey_Left || event->Key.Code == KrKey_Right) {
			int direction = event->Key.Code == KrKey_Left ? -1 : 1;
			int magnitude = event->Key.Repeat ? 5 : 10;
			float next = G.Pos + (direction * magnitude) * 48000;
			next = Clamp(0, (float)G.Last, next);
			G.Pos = next;
			ClearHistory();
		}

		if (event->Key.Code == KrKey_Plus) {
			G.FreqRatio += 0.1f;
			printf("FreqRatio = %f\n", G.FreqRatio);
		} else if (event->Key.Code == KrKey_Minus) {
			G.FreqRatio -= 0.1f;
			printf("FreqRatio = %f\n", G.FreqRatio);
		}

		if (event->Key.Code == KrKey_N) {
			NextWaveForm();
		} else if (event->Key.Code == KrKey_B) {
			PrevWaveForm();
		}

		if (event->Key.Code == KrKey_Up || event->Key.Code == KrKey_Down) {
			float direction = event->Key.Code == KrKey_Down ? -1.0f : 1.0f;
			float magnitude = event->Key.Repeat ? 0.01f : 0.02f;
			float next = G.Volume + direction * magnitude;
			next = Clamp(0.0f, 1.0f, next);
			G.Volume = next;
		}

		if (event->Key.Code >= KrKey_0 && event->Key.Code <= KrKey_9) {
			float fraction = (float)(event->Key.Code - KrKey_0) / 10.0f;
			G.Pos = fraction * (float)G.Last;
			ClearHistory();
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

Complex ComplexLerp(Complex a, Complex b, float t) {
	Complex r;
	r.re = Lerp(a.re, b.re, t);
	r.im = Lerp(a.im, b.im, t);
	return r;
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

	float d = w / (float)MAX_RENDER_FRAMES;
	float g = d - Gap;

	Color base  = { 1.0f, 1.0f, 0.0f, 1.0f };
	Color highy = { 1.0f, 0.0f, 0.0f, 1.0f };
	Color highx = { 0.0f, 1.0f, 0.0f, 1.0f };

	float max_spec_mag = -1.0f;
	i32   max_spec_idx = -1;

	float spec_color_a[] = { 1,1,0,1 };
	float spec_color_b[] = { 1,0,0,1 };

	int   max_spec = MAX_RENDER_FRAMES / 2;

	float spec_g = 0.2f;
	float spec_w = 2.0f;
	float spec_x = 0.5f * (window_w - (spec_w + spec_g) * max_spec);
	float spec_y = y + 0.4f * h;
	float spec_min_height = 2.0f;
	float spec_max_height = 100.0f;

	for (i32 index = 0; index < MAX_RENDER_FRAMES; ++index) {
		G.RenderBuf.TimeDomain[index] = Lerp(G.RenderBuf.TimeDomain[index], G.RenderBufTarget[G.RenderBufIndex].TimeDomain[index], 0.2f); 
		G.RenderBuf.FreqDomain[index] = ComplexLerp(G.RenderBuf.FreqDomain[index], G.RenderBufTarget[G.RenderBufIndex].FreqDomain[index], 0.2f);
	}

	for (i32 index = 0; index < max_spec; ++index) {
		Complex val   = G.RenderBuf.FreqDomain[index];
		float mag     = sqrtf(val.re * val.re + val.im * val.im);
		float spec_h  = log10f(1.0f + mag) * spec_max_height;

		PL_DrawRectVert(spec_x, spec_y, spec_w, spec_min_height + spec_h, spec_color_a, spec_color_b);
		spec_x += (spec_w + spec_g);
	}

	for (i32 index = 0; index < MAX_RENDER_FRAMES; ++index) {
		float val = G.RenderBuf.TimeDomain[index];

		float midx = (float)index / (float)MAX_RENDER_FRAMES;
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

	float progress_target = (progress_w * G.Pos) / (float)G.Last;

	float progress_bg[] = { 0.0f, 0.0f, 0.0f, 1.0f };
	float progress_fg[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	Color volume_fg_start = { 1.0f, 1.0f, 0.1f, 1.0f };
	Color volume_fg_stop = { 8.0f, 0.1f, 0.0f, 1.0f };

	Progress = Lerp(Progress, progress_target, 0.2f);
	VolumeDisp = Lerp(VolumeDisp, G.Volume, 0.2f);

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

	ClearHistory();

	WaveForms = (Audio_Stream **)malloc(argc * sizeof(Audio_Stream *));

	for (int i = 1; i < argc; ++i) {
		const char *path = argv[i];

		umem size = 0;
		u8 *buff  = ReadEntireFile(path, &size);
		WaveForms[WaveFormCount++] = (Audio_Stream *)buff;
	}

	G.Volume    = 1;
	G.FreqRatio = 1;
	G.Frames    = Serialize(WaveForms[WaveFormIndex], &G.Last, &InFreq);

	ctx->OnEvent = HandleEvent;
	ctx->OnUpdate = Update;
	ctx->OnUploadAudio = UploadAudioFrames;

	return 0;
}
