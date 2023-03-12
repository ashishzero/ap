#include <stdio.h>
#include "Kr/KrPlatform.h"

typedef enum CnvFormat {
	CnvFormat_R32,
	CnvFormat_I32,
	CnvFormat_I16,
	CnvFormat_EnumCount
} CnvFormat;

typedef enum CnvChannel {
	CnvChannel_Equal,
	CnvChannel_Mono,
	CnvChannel_Stereo,
	CnvChannel_StereoIncrease,
	CnvChannel_Increase,
	CnvChannel_Decrease,
	CnvChannel_EnumCount
} CnvChannel;

typedef enum CnvFrequency {
	CnvFrequency_Normal,
	CnvFrequency_Shifted,
	CnvFrequency_EnumCount
} CnvFrequency;

static const char *CnvFormatLower[] = {
	"r32", "i32", "i16"
};

static const char *CnvFloatCast[] = {
	"", "(float)", "(float)"
};

static const char *CnvInvFloatCast[] = {
	"", "(i32)", "(i16)"
};

static const char *CnvFormatUpper[] = {
	"R32", "I32", "I16"
};

static const int CnvFormatBits[] = {
	32, 32, 16
};

void GenCnv(FILE *out, CnvFormat dst, CnvFormat src, CnvChannel chan, CnvFrequency freq) {
	fprintf(out, "static void AudioCnv%sTo%s", CnvFormatUpper[src], CnvFormatUpper[dst]);

	if (chan == CnvChannel_Mono)
		fprintf(out, "_Mono");
	else if (chan == CnvChannel_Stereo)
		fprintf(out, "_Stereo");
	else if (chan == CnvChannel_StereoIncrease)
		fprintf(out, "_StereoInc");
	else if (chan == CnvChannel_Increase)
		fprintf(out, "_Inc");
	else if (chan == CnvChannel_Decrease)
		fprintf(out, "_Dec");

	if (freq == CnvFrequency_Shifted)
		fprintf(out, "_Resample");

	fprintf(out, "(");
	fprintf(out, "AudioCnv *cnv, ");
	fprintf(out, "void *dst, u32 dst_frames, "); 
	fprintf(out, "void *src, u32 src_frames, ");
	fprintf(out, "u32 *written, u32 *read) ");
	fprintf(out, "{\n");

	fprintf(out, "\t%s *out     = dst;\n", CnvFormatLower[dst]);
	fprintf(out, "\t%s *in      = src;\n", CnvFormatLower[src]);

	if (chan == CnvChannel_Equal)
		fprintf(out, "\tu32 channels = cnv->Dst.Channels;\n");

	if (freq == CnvFrequency_Normal) {
		fprintf(out, "\tu32 frames   = Min(dst_frames, src_frames);\n");
		fprintf(out, "\t*written     = frames;\n");
		fprintf(out, "\t*read        = frames;\n");
	} else {
		fprintf(out, "\tr32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;\n");
	}

	if (freq == CnvFrequency_Normal) {
		if (chan == CnvChannel_Equal) {
			if (dst == src) {
				fprintf(out, "\tmemcpy(out, in, frames * channels * sizeof(%s));\n", CnvFormatLower[dst]);
			} else if (dst == CnvFormat_R32) {
				fprintf(out, "\tfor (u32 fi = 0; fi < frames; ++fi) {\n");
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci) {\n");
				fprintf(out, "\t\t\tout[ci] = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, (r32)in[ci]);\n", CnvFormatBits[src], CnvFormatBits[src]);
				fprintf(out, "\t\t}\n");
				fprintf(out, "\t\tout += channels;\n");
				fprintf(out, "\t\tin  += channels;\n");
				fprintf(out, "\t}\n");
			} else if (src == CnvFormat_R32) {
				fprintf(out, "\tfor (u32 fi = 0; fi < frames; ++fi) {\n");
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci) {\n");
				fprintf(out, "\t\t\tout[ci] = (%s)MapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, in[ci]);\n", CnvFormatLower[dst], CnvFormatBits[dst], CnvFormatBits[dst]);
				fprintf(out, "\t\t}\n");
				fprintf(out, "\t\tout += channels;\n");
				fprintf(out, "\t\tin  += channels;\n");
				fprintf(out, "\t}\n");
			} else if (dst == CnvFormat_I32) {
				fprintf(out, "\tfor (u32 fi = 0; fi < frames; ++fi) {\n");
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci) {\n");
				fprintf(out, "\t\t\tout[ci] = (i32)in[ci] * 2;\n");
				fprintf(out, "\t\t}\n");
				fprintf(out, "\t\tout += channels;\n");
				fprintf(out, "\t\tin  += channels;\n");
				fprintf(out, "\t}\n");
			} else {
				Assert(dst == CnvFormat_I16);
				fprintf(out, "\tfor (u32 fi = 0; fi < frames; ++fi) {\n");
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci) {\n");
				fprintf(out, "\t\t\tout[ci] = (i16)(in[ci] / 2);\n");
				fprintf(out, "\t\t}\n");
				fprintf(out, "\t\tout += channels;\n");
				fprintf(out, "\t\tin  += channels;\n");
				fprintf(out, "\t}\n");
			}
		} else if (chan == CnvChannel_Mono) {
			fprintf(out, "\tfor (u32 fi = 0; fi < frames; ++fi) {\n");
			if (dst == src) {
				fprintf(out, "\t\t%s a = in[0];\n", CnvFormatLower[src]);
				fprintf(out, "\t\t%s b = in[1];\n", CnvFormatLower[src]);
				if (dst == CnvFormat_R32)
					fprintf(out, "\t\tout[0] = 0.5f * (a + b);\n");
				else
					fprintf(out, "\t\tout[0] = (a + b) / 2;\n");
			} else if (dst == CnvFormat_R32) {
				fprintf(out, "\t\tr32 a = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, (r32)in[0]);\n", CnvFormatBits[src], CnvFormatBits[src]);
				fprintf(out, "\t\tr32 b = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, (r32)in[1]);\n", CnvFormatBits[src], CnvFormatBits[src]);
				fprintf(out, "\t\tout[0] = 0.5f * (a + b);\n");
			} else if (src == CnvFormat_R32) {
				fprintf(out, "\t\t%s a = (%s)MapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, in[0]);\n",
					CnvFormatLower[dst], CnvFormatLower[dst], CnvFormatBits[src], CnvFormatBits[src]);
				fprintf(out, "\t\t%s b = (%s)MapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, in[1]);\n",
					CnvFormatLower[dst], CnvFormatLower[dst], CnvFormatBits[src], CnvFormatBits[src]);
				fprintf(out, "\t\tout[0] = (%s)(0.5f * (a + b));\n", CnvFormatLower[dst]);
			} else if (dst == CnvFormat_I32) {
				fprintf(out, "\t\tout[0] = (i32)(in[0]) + (i32)(in[1]);\n", CnvFormatLower[dst]);
			} else {
				Assert(dst == CnvFormat_I16);
				fprintf(out, "\t\tout[0] = (i32)(in[0] + in[1]) / 4;\n", CnvFormatLower[dst]);
			}
			fprintf(out, "\t\tout += 1;\n");
			fprintf(out, "\t\tin  += cnv->Src.Channels;\n");
			fprintf(out, "\t}\n");
		} else if (chan == CnvChannel_Stereo) {
			fprintf(out, "\tfor (u32 fi = 0; fi < frames; ++fi) {\n");
			if (dst == src) {
				fprintf(out, "\t\t%s a = in[0];\n", CnvFormatLower[src]);
			} else if (dst == CnvFormat_R32) {
				fprintf(out, "\t\tr32 a = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, (r32)in[1]);\n", CnvFormatBits[src], CnvFormatBits[src]);
			} else if (src == CnvFormat_R32) {
				fprintf(out, "\t\t%s a = (%s)MapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, in[0]);\n",
					CnvFormatLower[dst], CnvFormatLower[dst], CnvFormatBits[src], CnvFormatBits[src]);
			} else if (dst == CnvFormat_I32) {
				fprintf(out, "\t\ti32 a = (i32)in[0] * 2;\n", CnvFormatLower[dst]);
			} else {
				Assert(dst == CnvFormat_I16);
				fprintf(out, "\t\ti16 a = (i16)(in[0] / 2);\n", CnvFormatLower[dst]);
			}
			fprintf(out, "\t\tout[0] = a;\n");
			fprintf(out, "\t\tout[1] = a;\n");
			fprintf(out, "\t\tout += 2;\n");
			fprintf(out, "\t\tin  += 1;\n");
			fprintf(out, "\t}\n");
		} else if (chan == CnvChannel_StereoIncrease) {
			fprintf(out, "\tu32 remains  = cnv->Dst.Channels - 2;\n");
			fprintf(out, "\tfor (u32 fi = 0; fi < frames; ++fi) {\n");
			if (dst == src) {
				fprintf(out, "\t\t%s a = in[0];\n", CnvFormatLower[src]);
			} else if (dst == CnvFormat_R32) {
				fprintf(out, "\t\tr32 a = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, (r32)in[1]);\n", CnvFormatBits[src], CnvFormatBits[src]);
			} else if (src == CnvFormat_R32) {
				fprintf(out, "\t\t%s a = (%s)MapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, in[0]);\n",
					CnvFormatLower[dst], CnvFormatLower[dst], CnvFormatBits[src], CnvFormatBits[src]);
			} else if (dst == CnvFormat_I32) {
				fprintf(out, "\t\ti32 a = (i32)in[0] * 2;\n", CnvFormatLower[dst]);
			} else {
				Assert(dst == CnvFormat_I16);
				fprintf(out, "\t\ti16 a = (i16)(in[0] / 2);\n", CnvFormatLower[dst]);
			}
			fprintf(out, "\t\tout[0] = a;\n");
			fprintf(out, "\t\tout[1] = a;\n");
			fprintf(out, "\t\tmemset(out + 2, 0, remains * sizeof(%s));\n", CnvFormatLower[dst]);
			fprintf(out, "\t\tout += cnv->Dst.Channels;\n");
			fprintf(out, "\t\tin  += 1;\n");
			fprintf(out, "\t}\n");
		} else if (chan == CnvChannel_Increase) {
			fprintf(out, "\tu32 channels = cnv->Src.Channels;\n");
			fprintf(out, "\tu32 remains  = cnv->Dst.Channels - channels;\n");
			fprintf(out, "\tfor (u32 fi = 0; fi < frames; ++fi) {\n");
			if (dst == src) {
				fprintf(out, "\t\tmemcpy(out, in, channels * sizeof(%s));\n", CnvFormatLower[dst]);
			} else if (dst == CnvFormat_R32) {
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci)\n");
				fprintf(out, "\t\t\tout[ci] = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, (r32)in[ci]);\n", CnvFormatBits[src], CnvFormatBits[src]);
			} else if (src == CnvFormat_R32) {
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci)\n");
				fprintf(out, "\t\t\tout[ci] = (%s)MapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, in[ci]);\n",
					CnvFormatLower[dst], CnvFormatBits[src], CnvFormatBits[src]);
			} else if (dst == CnvFormat_I32) {
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci)\n");
				fprintf(out, "\t\t\tout[ci] = (i32)in[ci] * 2;\n", CnvFormatLower[dst]);
			} else {
				Assert(dst == CnvFormat_I16);
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci)\n");
				fprintf(out, "\t\t\tout[ci] = (i16)(in[ci] / 2);\n", CnvFormatLower[dst]);
			}
			fprintf(out, "\t\tmemset(out + channels, 0, remains * sizeof(%s));\n", CnvFormatLower[dst]);
			fprintf(out, "\t\tout += cnv->Dst.Channels;\n");
			fprintf(out, "\t\tin  += cnv->Src.Channels;\n");
			fprintf(out, "\t}\n");
		} else if (chan == CnvChannel_Decrease) {
			fprintf(out, "\tu32 channels = cnv->Dst.Channels;\n");
			fprintf(out, "\tfor (u32 fi = 0; fi < frames; ++fi) {\n");
			if (dst == src) {
				fprintf(out, "\t\tmemcpy(out, in, channels * sizeof(%s));\n", CnvFormatLower[dst]);
			} else if (dst == CnvFormat_R32) {
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci)\n");
				fprintf(out, "\t\t\tout[ci] = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, (r32)in[ci]);\n", CnvFormatBits[src], CnvFormatBits[src]);
			} else if (src == CnvFormat_R32) {
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci)\n");
				fprintf(out, "\t\t\tout[ci] = (%s)MapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, in[ci]);\n",
					CnvFormatLower[dst], CnvFormatBits[src], CnvFormatBits[src]);
			} else if (dst == CnvFormat_I32) {
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci)\n");
				fprintf(out, "\t\t\tout[ci] = (i32)in[ci] * 2;\n", CnvFormatLower[dst]);
			} else {
				Assert(dst == CnvFormat_I16);
				fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci)\n");
				fprintf(out, "\t\t\tout[ci] = (i16)(in[ci] / 2);\n", CnvFormatLower[dst]);
			}
			fprintf(out, "\t\tout += cnv->Dst.Channels;\n");
			fprintf(out, "\t\tin  += cnv->Src.Channels;\n");
			fprintf(out, "\t}\n");
		}
	} else {
		if (chan == CnvChannel_StereoIncrease) {
			fprintf(out, "\tu32 remains  = cnv->Dst.Channels - 2;\n");
		} else if (chan == CnvChannel_Increase) {
			fprintf(out, "\tu32 channels = cnv->Src.Channels;\n");
			fprintf(out, "\tu32 remains  = cnv->Dst.Channels - channels;\n");
		} else if (chan == CnvChannel_Decrease) {
			fprintf(out, "\tu32 channels = cnv->Src.Channels;\n");
		}

		fprintf(out, "\n\tu32 di = 0;\n");
		fprintf(out, "\tfor (; di < dst_frames; ++di) {\n");
		fprintf(out, "\t\tfloat si = (float) di * factor;\n");
		fprintf(out, "\t\tint   i  = (int)si;\n");
		fprintf(out, "\t\tfloat t  = si - i;\n");
		fprintf(out, "\n");
		fprintf(out, "\t\tif ((u32)i + 1 >= src_frames)\n");
		fprintf(out, "\t\t\tbreak;\n\n");

		if (chan == CnvChannel_Equal) {
			fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci) {\n");
			const char *src_cast = CnvFloatCast[src];
			const char *dst_cast = CnvInvFloatCast[dst];
			fprintf(out, "\t\t\tfloat a = %sin[i + 0 + ci];\n", src_cast);
			fprintf(out, "\t\t\tfloat b = %sin[i + 1 + ci];\n", src_cast);
			fprintf(out, "\t\t\tfloat r = Lerp(a, b, t);\n");
			if (dst == src) {
				fprintf(out, "\t\t\tout[ci] = %sr;\n", dst_cast);
			} else if (dst == CnvFormat_R32) {
				fprintf(out, "\t\t\tout[ci] = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, r);\n", CnvFormatBits[src], CnvFormatBits[src]);
			} else if (src == CnvFormat_R32) {
				fprintf(out, "\t\t\tout[ci] = %sMapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, in[ci]);\n",
					dst_cast, CnvFormatBits[src], CnvFormatBits[src]);
			} else if (dst == CnvFormat_I32) {
				fprintf(out, "\t\t\tout[ci] = (i32)(r * 2);\n");
			} else if (dst == CnvFormat_I16) {
				fprintf(out, "\t\t\tout[ci] = (i32)(r * 0.5f);\n");
			}
			fprintf(out, "\t\t}\n");
			fprintf(out, "\t\tout += channels;\n");
			fprintf(out, "\t\tin  += channels;\n");
		} else if (chan == CnvChannel_Mono) {
			const char *src_cast = CnvFloatCast[src];
			const char *dst_cast = CnvInvFloatCast[dst];
			fprintf(out, "\t\tfloat a = %sin[i + 0 + 0];\n", src_cast);
			fprintf(out, "\t\tfloat b = %sin[i + 1 + 0];\n", src_cast);
			fprintf(out, "\t\tfloat c = %sin[i + 0 + 1];\n", src_cast);
			fprintf(out, "\t\tfloat d = %sin[i + 1 + 1];\n", src_cast);
			fprintf(out, "\t\tfloat r = 0.5f * (Lerp(a, b, t) + Lerp(c, d, t));\n");
			if (dst == src) {
				fprintf(out, "\t\tout[0] = %sr;\n", dst_cast);
			} else if (dst == CnvFormat_R32) {
				fprintf(out, "\t\tout[0] = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, r);\n", CnvFormatBits[src], CnvFormatBits[src]);
			} else if (src == CnvFormat_R32) {
				fprintf(out, "\t\tout[0] = %sMapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, r);\n",
					dst_cast, CnvFormatBits[src], CnvFormatBits[src]);
			} else if (dst == CnvFormat_I32) {
				fprintf(out, "\t\tout[0] = (i32)(r * 2);\n");
			} else if (dst == CnvFormat_I16) {
				fprintf(out, "\t\tout[0] = (i32)(r * 0.5f);\n");
			}
			fprintf(out, "\t\tout += 1;\n");
			fprintf(out, "\t\tin  += cnv->Src.Channels;\n");
		} else if (chan == CnvChannel_Stereo) {
			const char *src_cast = CnvFloatCast[src];
			const char *dst_cast = CnvInvFloatCast[dst];
			fprintf(out, "\t\tfloat a = %sin[i + 0];\n", src_cast);
			fprintf(out, "\t\tfloat b = %sin[i + 1];\n", src_cast);
			fprintf(out, "\t\tfloat r = Lerp(a, b, t);\n");
			if (dst == src) {
				fprintf(out, "\t\tout[0] = %sr;\n", dst_cast);
				fprintf(out, "\t\tout[1] = %sr;\n", dst_cast);
			} else if (dst == CnvFormat_R32) {
				fprintf(out, "\t\tr32 o = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, r);\n", CnvFormatBits[src], CnvFormatBits[src]);
				fprintf(out, "\t\tout[0] = o;\n");
				fprintf(out, "\t\tout[1] = o;\n");
			} else if (src == CnvFormat_R32) {
				fprintf(out, "\t\t%s o = %sMapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, r);\n",
					CnvFormatLower[dst], dst_cast, CnvFormatBits[src], CnvFormatBits[src]);
				fprintf(out, "\t\tout[0] = o;\n");
				fprintf(out, "\t\tout[1] = o;\n");
			} else if (dst == CnvFormat_I32) {
				fprintf(out, "\t\ti32 o = (i32)(r * 2);\n");
				fprintf(out, "\t\tout[0] = o;\n");
				fprintf(out, "\t\tout[1] = o;\n");
			} else if (dst == CnvFormat_I16) {
				fprintf(out, "\t\ti16 o = (i32)(r * 0.5f);\n");
				fprintf(out, "\t\tout[0] = o;\n");
				fprintf(out, "\t\tout[1] = o;\n");
			}
			fprintf(out, "\t\tout += 2;\n");
			fprintf(out, "\t\tin  += 1;\n");
		} else if (chan == CnvChannel_StereoIncrease) {
			const char *src_cast = CnvFloatCast[src];
			const char *dst_cast = CnvInvFloatCast[dst];
			fprintf(out, "\t\tfloat a = %sin[i + 0];\n", src_cast);
			fprintf(out, "\t\tfloat b = %sin[i + 1];\n", src_cast);
			fprintf(out, "\t\tfloat r = Lerp(a, b, t);\n");
			fprintf(out, "\t\tout[0]  = %sr;\n", dst_cast);
			fprintf(out, "\t\tout[1]  = %sr;\n", dst_cast);
			fprintf(out, "\t\tmemset(out + 2, 0, remains * sizeof(%s));\n", CnvFormatLower[dst]);
			fprintf(out, "\t\tout += cnv->Dst.Channels;\n");
			fprintf(out, "\t\tin  += 1;\n");
		} else {
			fprintf(out, "\t\tfor (u32 ci = 0; ci < channels; ++ci) {\n");
			const char *src_cast = CnvFloatCast[src];
			const char *dst_cast = CnvInvFloatCast[dst];
			fprintf(out, "\t\t\tfloat a = %sin[i + 0 + ci];\n", src_cast);
			fprintf(out, "\t\t\tfloat b = %sin[i + 1 + ci];\n", src_cast);
			fprintf(out, "\t\t\tfloat r = Lerp(a, b, t);\n");
			if (dst == src) {
				fprintf(out, "\t\t\tout[ci] = %sr;\n", dst_cast);
			} else if (dst == CnvFormat_R32) {
				fprintf(out, "\t\t\tout[ci] = MapRange((r32)INT%d_MIN, (r32)INT%d_MAX, -1, 1, r);\n", CnvFormatBits[src], CnvFormatBits[src]);
			} else if (src == CnvFormat_R32) {
				fprintf(out, "\t\t\tout[ci] = %sMapRange(-1, 1, (r32)INT%d_MIN, (r32)INT%d_MAX, in[ci]);\n",
					dst_cast, CnvFormatBits[src], CnvFormatBits[src]);
			} else if (dst == CnvFormat_I32) {
				fprintf(out, "\t\t\tout[ci] = (i32)(r * 2);\n");
			} else if (dst == CnvFormat_I16) {
				fprintf(out, "\t\t\tout[ci] = (i32)(r * 0.5f);\n");
			}
			fprintf(out, "\t\t}\n");
			if (chan == CnvChannel_Increase) {
				fprintf(out, "\t\tmemset(out + channels, 0, remains * sizeof(%s));\n", CnvFormatLower[dst]);
			}
			fprintf(out, "\t\tout += cnv->Dst.Channels;\n");
			fprintf(out, "\t\tin  += cnv->Src.Channels;\n");
		}

		fprintf(out, "\t}\n\n");
		fprintf(out, "\t*written = di;\n");
		fprintf(out, "\t*read    = (u32)(di * factor);\n");
	}

	fprintf(out, "}\n\n");
}

int main() {
	for (int freq = 0; freq < CnvFrequency_EnumCount; ++freq) {
		for (int chan = 0; chan < CnvChannel_EnumCount; ++chan) {
			for (int src = 0; src < CnvFormat_EnumCount; ++src) {
				for (int dst = 0; dst < CnvFormat_EnumCount; ++dst) {
					GenCnv(stdout, dst, src, chan, freq);
				}
			}
		}
	}
	return 0;
}
