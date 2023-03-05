#if 1
#include "Kr/KrMedia.h"
#include "Kr/KrMath.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

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
	float    Frequency;
	//float    FreqRatio;
	float    PitchShift;

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

float WrapAngle(float radians) {
	const float pi = (float)PI;
	return Wrap(-pi, pi, radians);
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

#define FFT_LENGTH    1024
#define WINDOW_LENGTH 1024
#define HOP_LENGTH    (WINDOW_LENGTH/8)

static const int WindowLength         = WINDOW_LENGTH;
static const int HopLength            = HOP_LENGTH;
static const int FFTLength            = WINDOW_LENGTH;
static const int InputFrameLength     = WINDOW_LENGTH;
static const int OutputFrameLength    = WINDOW_LENGTH;

static F32FrameStereo InputFrames[WINDOW_LENGTH];
static Complex        FFTBufferL[FFT_LENGTH];
static Complex        FFTBufferR[FFT_LENGTH];
static F32FrameStereo OutputFrames[WINDOW_LENGTH];
static int            OutputFramePos = HOP_LENGTH;

static float          LastInputPhasesL[FFT_LENGTH];
static float          LastInputPhasesR[FFT_LENGTH];
static float          LastOutputPhasesL[FFT_LENGTH];
static float          LastOutputPhasesR[FFT_LENGTH];

static float          SynthesisMagnitudesL[FFT_LENGTH/2 + 1];
static float          SynthesisMagnitudesR[FFT_LENGTH/2 + 1];
static float          SynthesisFrequenciesL[FFT_LENGTH/2 + 1];
static float          SynthesisFrequenciesR[FFT_LENGTH/2 + 1];

static float          AnalysisMagnitudesL[FFT_LENGTH/2 + 1];
static float          AnalysisMagnitudesR[FFT_LENGTH/2 + 1];
static float          AnalysisFrequenciesL[FFT_LENGTH/2 + 1];
static float          AnalysisFrequenciesR[FFT_LENGTH/2 + 1];

static float          HanWindow[WINDOW_LENGTH];

static bool ApplyTransformation = false;

void InitWindow() {
	for (int i = 0; i < WindowLength; ++i) {
		HanWindow[i] = 0.5f * (1.0f - cosf(2 * (float)PI * (float)i / (float)(WindowLength - 1)));
	}
}

void Transform(Complex *buf, uint length) {
	return;
	//if (!ApplyTransformation) return;

	for (uint i = 0; i < length; ++i) {
		float a     = sqrtf(buf[i].re * buf[i].re + buf[i].im * buf[i].im);
		float pi    = (float)PI;

		// whisper
		//float phase = 2.0f * (float)PI * (float)rand() / (float)RAND_MAX;
		//buf[i].re   = a * cosf(phase);
		//buf[i].im   = a * sinf(phase);

		// robotize
		buf[i].re   = a;
		buf[i].im   = 0.0f;
	}
}

void Process(const PL_AudioSpec *spec) {
	if (OutputFramePos < HopLength)
		return;

	OutputFramePos = 0;

	PCM16FrameStereo *src = (PCM16FrameStereo *)G.Frames;

	memcpy(InputFrames, InputFrames + HopLength, (InputFrameLength - HopLength) * sizeof(F32FrameStereo));

	float pitch_shift = powf(2.0f, G.PitchShift / 16.0f);
	float freq_ratio  = 1.0f;

	for (int index = InputFrameLength - HopLength; index < WindowLength; ++index) {
		InputFrames[index] = LoadFrame(src, G.Pos, G.Last);

		G.Pos += (freq_ratio * G.Frequency / (float)spec->Frequency);

		// repeat for now
		G.Pos = Wrap(0.0f, (float)G.Last, G.Pos);

		// TODO: correct transition
		//if (G.Pos <= 0.0f) {
		//	PrevWaveForm();
		//} else if (G.Pos >= G.Last) {
		//	NextWaveForm();
		//}
	}

	Assert(InputFrameLength >= WindowLength);

	for (int index = 0; index < WindowLength; ++index) {
		FFTBufferL[index].re = HanWindow[index] * InputFrames[index].Left;
		FFTBufferR[index].re = HanWindow[index] * InputFrames[index].Right;
		FFTBufferL[index].im = 0.0f;
		FFTBufferR[index].im = 0.0f;
	}
	for (int index = WindowLength; index < FFTLength; ++index) {
		FFTBufferL[index].re = 0.0f;
		FFTBufferR[index].re = 0.0f;
		FFTBufferL[index].im = 0.0f;
		FFTBufferR[index].im = 0.0f;
	}

	FFT(FFTBufferL, FFTLength);
	FFT(FFTBufferR, FFTLength);

#if 1
	Transform(FFTBufferL, FFTLength);
	Transform(FFTBufferR, FFTLength);
#endif

	for (int index = 0; index <= FFTLength/2; ++index) {
		float amplitudes[2], phases[2];
		amplitudes[0] = ComplexAmplitude(FFTBufferL[index]);
		amplitudes[1] = ComplexAmplitude(FFTBufferR[index]);
		phases[0]     = atan2f(FFTBufferL[index].im, FFTBufferL[index].re);
		phases[1]     = atan2f(FFTBufferR[index].im, FFTBufferR[index].re);

		float center_bin_freq = 2.0f * (float)PI * (float)index / (float)FFTLength;
		float phase_diff_l = phases[0] - LastInputPhasesL[index];
		float phase_diff_r = phases[1] - LastInputPhasesR[index];

		phase_diff_l = WrapAngle(phase_diff_l - center_bin_freq * (float)HopLength);
		phase_diff_r = WrapAngle(phase_diff_r - center_bin_freq * (float)HopLength);

		float deviation_l      = phase_diff_l / (float)HopLength;
		float deviation_r      = phase_diff_r / (float)HopLength;


		float freq = 2.0f * (float)PI * (float)index / (float)FFTLength;

		AnalysisFrequenciesL[index] = freq + deviation_l;
		AnalysisFrequenciesR[index] = freq + deviation_r;

		AnalysisMagnitudesL[index] = amplitudes[0];
		AnalysisMagnitudesR[index] = amplitudes[1];

		LastInputPhasesL[index] = phases[0];
		LastInputPhasesR[index] = phases[1];
	}

	{
		float freqs[2] = {0,0};
		float mags[2]  = {0,0};

		for (int index = 0; index <= FFTLength/2; ++index) {
			if (AnalysisMagnitudesL[index] > mags[0]) {
				mags[0]  = AnalysisMagnitudesL[index];
				freqs[0] = AnalysisFrequenciesL[index];
			}

			if (AnalysisMagnitudesL[index] > mags[1]) {
				mags[1]  = AnalysisMagnitudesR[index];
				freqs[1] = AnalysisFrequenciesR[index];
			}
		}

		float freq_l = freqs[0] * spec->Frequency / (2.0f * (float)PI);
		float freq_r = freqs[1] * spec->Frequency / (2.0f * (float)PI);

		//printf("Freq Left: %f, Freq Right: %f\n", freq_l, freq_r);
	}

	for (int i = 0; i <= FFTLength/2; ++i) {
		SynthesisMagnitudesL[i]  = 0;
		SynthesisMagnitudesR[i]  = 0;
		SynthesisFrequenciesL[i] = 0;
		SynthesisFrequenciesR[i] = 0;
	}

#if 1
	// Shift the pitch
	for (int i = 0; i <= FFTLength/2; ++i) {
		if (ApplyTransformation) {
			// robotize
			float robot_base_freq = 120.0f;
			float harmonic_l = floorf(AnalysisFrequenciesL[i] * spec->Frequency / (2.0f * (float)PI) / robot_base_freq + 0.5f);
			float harmonic_r = floorf(AnalysisFrequenciesR[i] * spec->Frequency / (2.0f * (float)PI) / robot_base_freq + 0.5f);

			if (harmonic_l > 0) {
				float harmonic_freq = robot_base_freq * (2.0f * (float)PI / spec->Frequency) * harmonic_l;

				int bin = (int)floorf(harmonic_freq * (float)FFTLength / (2.0f * (float)PI) + 0.5f);

				if (bin <= FFTLength/2) {
					SynthesisMagnitudesL[bin] += AnalysisMagnitudesL[i];
					SynthesisFrequenciesL[bin] = harmonic_freq;
				}
			}

			if (harmonic_r > 0) {
				float harmonic_freq = robot_base_freq * (2.0f * (float)PI / spec->Frequency) * harmonic_r;

				int bin = (int)floorf(harmonic_freq * (float)FFTLength / (2.0f * (float)PI) + 0.5f);

				if (bin <= FFTLength/2) {
					SynthesisMagnitudesR[bin] += AnalysisMagnitudesR[i];
					SynthesisFrequenciesR[bin] = harmonic_freq;
				}
			}
		} else {
			int bin = (int)floorf(i * pitch_shift + 0.5f);

			// TODOS: 
			// - peak tracking algorithm
			// - combine frequency detector with pitch shift to implement auto-tuning
			// - cross systhesis
			// - noise reduction
			if (bin <= FFTLength/2) {
				// TODO: Use RMS to add these
				SynthesisMagnitudesL[bin] += AnalysisMagnitudesL[i];
				SynthesisMagnitudesR[bin] += AnalysisMagnitudesR[i];

				// TODO: Weighted average here
				SynthesisFrequenciesL[bin] = pitch_shift * AnalysisFrequenciesL[i];
				SynthesisFrequenciesR[bin] = pitch_shift * AnalysisFrequenciesR[i];
			}
		}
	}

	for (int index = 0; index <= FFTLength/2; ++index) {
		float amplitudes[2], phases[2];
		amplitudes[0] = SynthesisMagnitudesL[index];
		amplitudes[1] = SynthesisMagnitudesR[index];

		float freq = 2.0f * (float)PI * (float)index / (float)FFTLength; 

		float deviation_l = SynthesisFrequenciesL[index] - freq;
		float deviation_r = SynthesisFrequenciesR[index] - freq;

		float phase_diff_l = deviation_l * (float)HopLength;
		float phase_diff_r = deviation_r * (float)HopLength;

		float center_bin_freq = 2.0f * (float)PI * (float)index / (float)FFTLength;
		phase_diff_l += center_bin_freq * (float)HopLength;
		phase_diff_r += center_bin_freq * (float)HopLength;

		phases[0] = WrapAngle(LastOutputPhasesL[index] + phase_diff_l);
		phases[1] = WrapAngle(LastOutputPhasesR[index] + phase_diff_r);

		FFTBufferL[index].re = amplitudes[0] * cosf(phases[0]);
		FFTBufferL[index].im = amplitudes[0] * sinf(phases[0]);
		FFTBufferR[index].re = amplitudes[1] * cosf(phases[1]);
		FFTBufferR[index].im = amplitudes[1] * sinf(phases[1]);

		if (index > 0 && index < FFTLength/2) {
			FFTBufferL[FFTLength - index] = (Complex){ .re = FFTBufferL[index].re,.im = -FFTBufferL[index].im };
			FFTBufferR[FFTLength - index] = (Complex){ .re = FFTBufferR[index].re,.im = -FFTBufferR[index].im };
		}

		LastOutputPhasesL[index] = phases[0];
		LastOutputPhasesR[index] = phases[1];
	}
#endif

	IFFT(FFTBufferL, FFTLength);
	IFFT(FFTBufferR, FFTLength);

	memcpy(OutputFrames, OutputFrames + HopLength, (OutputFrameLength - HopLength) * sizeof(F32FrameStereo));
	memset(OutputFrames + OutputFrameLength - HopLength, 0, HopLength * sizeof(F32FrameStereo));

	for (int index = 0; index < WindowLength; ++index) {
		OutputFrames[index].Left += HanWindow[index] * FFTBufferL[index].re;
		OutputFrames[index].Right += HanWindow[index] * FFTBufferR[index].re;
	}
}

u32 UploadAudioFrames(const PL_AudioSpec *spec, u8 *buf, u32 count, void *user) {
	Assert(spec->Format == PL_AudioFormat_R32 && spec->Channels == 2);

	F32FrameStereo   *dst = (F32FrameStereo *)buf;
	F32FrameStereo   *end = dst + count;
	//PCM16FrameStereo *src = (PCM16FrameStereo *)G.Frames;

	while (dst < end) {
		Process(spec);

		float overlap = (float)HopLength / (float)WindowLength;

		F32FrameStereo out = OutputFrames[OutputFramePos];
		out.Left *= overlap;
		out.Right *= overlap;

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

	FFT(G.RenderBufTarget[next_buf_index].FreqDomain, MAX_RENDER_FRAMES);

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

typedef struct WaveForm {
	float Frequency;
	uint  Count;
	i16 * Frames;
} WaveForm;

static uint       WaveFormIndex;
static uint       WaveFormCount;
static WaveForm  *WaveForms;

WaveForm Serialize(Audio_Stream *audio) {
	Wave_Data *ptr = &audio->data;
	for (; ptr->id[0] == 'L';) {
		uint sz = ptr->size;
		ptr += 1;
		ptr = (Wave_Data *)((u8 *)ptr + sz);
	}

	return (WaveForm) {
		.Frequency = (float)audio->fmt.sample_rate,
		.Count = ptr->size / sizeof(i16) / audio->fmt.channels_count,
		.Frames = (i16 *)(ptr + 1)
	};
}

void ResetWaveform() {
	ClearHistory();
	G.Pos       = 0;
	G.Frames    = WaveForms[WaveFormIndex].Frames;
	G.Last      = WaveForms[WaveFormIndex].Count;
	G.Frequency = WaveForms[WaveFormIndex].Frequency;
}

void NextWaveForm() {
	WaveFormIndex = (WaveFormIndex + 1) % WaveFormCount;
	ResetWaveform();
}

void PrevWaveForm() {
	WaveFormIndex = (WaveFormIndex - 1) % WaveFormCount;
	ResetWaveform();
}

void PlayerHandleEvent(const PL_Event *event, void *user) {
	if (event->Kind == PL_Event_KeyPressed && !event->Key.Repeat) {
		if (event->Key.Sym == PL_Key_Space) {
			if (PL_IsAudioRendering()) {
				PL_PauseAudioRender();
			} else {
				PL_ResumeAudioRender();
			}
		}
		if (event->Key.Sym == PL_Key_F11) {
			PL_ToggleFullscreen();
		}
	}

	if (event->Kind == PL_Event_KeyPressed) {
		if (event->Key.Sym == PL_Key_Q) {
			QFactor = Clamp(0.0f, 1.0f, QFactor - 0.1f);
			printf("Q = %f\n", QFactor);
		} else if (event->Key.Sym == PL_Key_W) {
			QFactor = Clamp(0.0f, 1.0f, QFactor + 0.1f);
			printf("Q = %f\n", QFactor);
		}

		if (event->Key.Sym == PL_Key_A) {
			QFreq = Clamp(0.0f, 1.0f, QFreq - 0.1f);
			printf("QFreq = %f\n", QFreq);
		} else if (event->Key.Sym == PL_Key_S) {
			QFreq = Clamp(0.0f, 1.0f, QFreq + 0.1f);
			printf("QFreq = %f\n", QFreq);
		}

		if (event->Key.Sym == PL_Key_Return) {
			ApplyTransformation = !ApplyTransformation;
		}

		if (event->Key.Sym == PL_Key_Left || event->Key.Sym == PL_Key_Right) {
			int direction = event->Key.Sym == PL_Key_Left ? -1 : 1;
			int magnitude = event->Key.Repeat ? 5 : 10;
			float next = G.Pos + (direction * magnitude) * 48000;
			next = Clamp(0, (float)G.Last, next);
			G.Pos = next;
			ClearHistory();
		}

		if (event->Key.Sym == PL_Key_Plus) {
			G.PitchShift += 0.5f;
			printf("PitchShift = %f\n", G.PitchShift);
		} else if (event->Key.Sym == PL_Key_Minus) {
			G.PitchShift -= 0.5f;
			printf("PitchShift = %f\n", G.PitchShift);
		}

		if (event->Key.Sym == PL_Key_N) {
			NextWaveForm();
		} else if (event->Key.Sym == PL_Key_B) {
			PrevWaveForm();
		}

		if (event->Key.Sym == PL_Key_Up || event->Key.Sym == PL_Key_Down) {
			float direction = event->Key.Sym == PL_Key_Down ? -1.0f : 1.0f;
			float magnitude = event->Key.Repeat ? 0.01f : 0.02f;
			float next = G.Volume + direction * magnitude;
			next = Clamp(0.0f, 1.0f, next);
			G.Volume = next;
		}

		if (event->Key.Sym >= PL_Key_0 && event->Key.Sym <= PL_Key_9) {
			float fraction = (float)(event->Key.Sym - PL_Key_0) / 10.0f;
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

#if 0
// TODO: cleanup this mess
void PL_DrawQuad(float p0[2], float p1[2], float p2[2], float p3[2], float color0[4], float color1[4], float color2[4], float color3[4]);
void PL_DrawRect(float x, float y, float w, float h, float color0[4], float color1[4]);
void PL_DrawRectVert(float x, float y, float w, float h, float color0[4], float color1[4]);

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
#endif

WaveForm GenerateSineWave(float freq, float amp) {
	uint samples = 2 * 2 * 48000 / (uint)freq;

	float dt = 1.0f / 48000.0f;

	i16 *data = malloc(sizeof(i16) * samples);
	memset(data, 0, sizeof(i16) * samples);

	float t = 0;
	for (uint i = 0; i < samples; i += 2) {
		float s = 32767.0f * amp * sinf(2.0f * 3.14f * freq * t);

		i16 quantized = (i16)s;
		data[i + 0] = quantized;
		data[i + 1] = quantized;

		t += dt;
	}

	return (WaveForm) {
		.Frames    = data,
		.Count     = samples / 2,
		.Frequency = 48000.0f
	};
}
#endif

#define MAX_CAPTURE_FRAME_SIZE (48000*16)
static i16  CapturesFrames[MAX_CAPTURE_FRAME_SIZE*2];
static int  CapturePos;

u32 CaptureAudio(PL_AudioSpec const *spec, u8 *in, u32 frames, void *data) {
	Assert(spec->Format == PL_AudioFormat_R32 && spec->Channels == 2);

	if (in) {
		float *samples = (float *)in;

		for (int i = 0; i < (int)frames; i += 1) {
			CapturesFrames[CapturePos * 2 + 0] = (i16)MapRange(-1.0f, 1.0f, INT16_MIN, INT16_MAX, samples[i * 2 + 0]);
			CapturesFrames[CapturePos * 2 + 1] = (i16)MapRange(-1.0f, 1.0f, INT16_MIN, INT16_MAX, samples[i * 2 + 1]);
			CapturePos = (CapturePos + 1) % MAX_CAPTURE_FRAME_SIZE;
		}
	} else {
		for (int i = 0; i < (int)frames; i += 1) {
			CapturesFrames[CapturePos * 2 + 0] = 0;
			CapturesFrames[CapturePos * 2 + 1] = 0;
			CapturePos = (CapturePos + 1) % MAX_CAPTURE_FRAME_SIZE;
		}
	}

	return frames;
}

static int CaptureRead = 0;

u32 RenderAudio(PL_AudioSpec const *spec, u8 *out, u32 frames, void *data) {
#if 1
	return UploadAudioFrames(spec, out, frames, data);
#else
	float *samples = (float *)out;

	for (int i = 0; i < (int)frames; ++i) {
		samples[i * 2 + 0] = MapRange(INT16_MIN, INT16_MAX, -1, 1, CapturesFrames[CaptureRead * 2 + 0]);
		samples[i * 2 + 1] = MapRange(INT16_MIN, INT16_MAX, -1, 1, CapturesFrames[CaptureRead * 2 + 1]);
		CaptureRead = (CaptureRead + 1) % MAX_CAPTURE_FRAME_SIZE;
	}

	return frames;
#endif
}

void HandleEvent(PL_Event const *event, void *data) {
	if (event->Kind == PL_Event_Startup) {
		PL_ResumeAudioCapture();
		PL_UpdateAudioRender();
		PL_ResumeAudioRender();
	} else {
		PlayerHandleEvent(event, data);
	}
}

void UpdateFrame(PL_IoDevice const *io, void *data) {
	static int index = 0;

	if (io->Keyboard.Keys[PL_Key_Period].Pressed) {
		if (io->Keyboard.Keys[PL_Key_Ctrl].Down) {
			index = (index + 1) % io->AudioRenderDevices.Count;
			PL_SetAudioDevice(&io->AudioRenderDevices.Data[index]);
		}

		LogTrace("Render Devices:\n");
		for (int i = 0; i < io->AudioRenderDevices.Count; ++i) {
			const char *details = io->AudioRenderDevices.Current == &io->AudioRenderDevices.Data[i] ? " - default" : "";
			LogTrace("\t%d. %s%s\n", i, io->AudioRenderDevices.Data[i].Name, details);
		}
		LogTrace("Capture Devices:\n");
		for (int i = 0; i < io->AudioCaptureDevices.Count; ++i) {
			const char *details = io->AudioCaptureDevices.Current == &io->AudioCaptureDevices.Data[i] ? " - default" : "";
			LogTrace("\t%d. %s%s\n", i, io->AudioCaptureDevices.Data[i].Name, details);
		}
	}
}

int Main(int argc, char **argv) {
	InitWindow();

	int cap_waveforms = argc * 100;

	WaveForms = (WaveForm *)malloc(cap_waveforms * sizeof(WaveForm));

	//WaveForms[WaveFormCount++] = GenerateSineWave(5, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(10, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(15, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(20, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(25, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(30, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(45, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(60, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(120, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(240, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(512, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(1024, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(2048, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(4096, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(8192, 0.5f);
	//WaveForms[WaveFormCount++] = GenerateSineWave(16384, 0.5f);

	//WaveForms[WaveFormCount++] = (WaveForm) { .Frequency = 48000, .Count = MAX_CAPTURE_FRAME_SIZE, .Frames = CapturesFrames };

	for (int i = 1; i < argc; ++i) {
		const char *path = argv[i];

		umem size = 0;
		u8 *buff  = ReadEntireFile(path, &size);
		WaveForms[WaveFormCount++] = Serialize((Audio_Stream *)buff);
	}

	G.Volume     = 1;
	//G.FreqRatio  = 1;
	G.PitchShift = 0;

	ResetWaveform();

	PL_Config config = {
		.Features = PL_Feature_Window | PL_Feature_Audio,
		.Window   = { .Title = "Audio Processing" },
		.Flags    = PL_Flag_ToggleFullscreenF11 | PL_Flag_ExitAltF4,
		.Audio    = { .Render = 1, .Capture = 1 },
		.User     = {
			.OnEvent        = HandleEvent,
			.OnUpdate       = UpdateFrame,
			.OnAudioRender  = RenderAudio,
			.OnAudioCapture = CaptureAudio
		},
	};

#ifdef M_BUILD_DEBUG
	config.Flags |= PL_Flag_ExitEscape;
#endif

	PL_SetConfig(&config);

	return 0;
}
