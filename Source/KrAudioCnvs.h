static void AudioCnvR32ToR32(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	memcpy(out, in, frames * channels * sizeof(r32));
}

static void AudioCnvR32ToI32(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci) {
			out[ci] = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		}
		out += channels;
		in  += channels;
	}
}

static void AudioCnvR32ToI16(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci) {
			out[ci] = (i16)MapRange(-1, 1, (r32)INT16_MIN, (r32)INT16_MAX, in[ci]);
		}
		out += channels;
		in  += channels;
	}
}

static void AudioCnvI32ToR32(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci) {
			out[ci] = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, (r32)in[ci]);
		}
		out += channels;
		in  += channels;
	}
}

static void AudioCnvI32ToI32(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	memcpy(out, in, frames * channels * sizeof(i32));
}

static void AudioCnvI32ToI16(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci) {
			out[ci] = (i16)(in[ci] / 2);
		}
		out += channels;
		in  += channels;
	}
}

static void AudioCnvI16ToR32(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	u32 channels = cnv->Dst.Channels;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci) {
			out[ci] = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, (r32)in[ci]);
		}
		out += channels;
		in  += channels;
	}
}

static void AudioCnvI16ToI32(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	u32 channels = cnv->Dst.Channels;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci) {
			out[ci] = (i32)in[ci] * 2;
		}
		out += channels;
		in  += channels;
	}
}

static void AudioCnvI16ToI16(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	u32 channels = cnv->Dst.Channels;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	memcpy(out, in, frames * channels * sizeof(i16));
}

static void AudioCnvR32ToR32_Mono(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		r32 a = in[0];
		r32 b = in[1];
		out[0] = 0.5f * (a + b);
		out += 1;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvR32ToI32_Mono(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		i32 a = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[0]);
		i32 b = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[1]);
		out[0] = (i32)(0.5f * (a + b));
		out += 1;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvR32ToI16_Mono(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		i16 a = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[0]);
		i16 b = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[1]);
		out[0] = (i16)(0.5f * (a + b));
		out += 1;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI32ToR32_Mono(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		r32 a = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, (r32)in[0]);
		r32 b = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, (r32)in[1]);
		out[0] = 0.5f * (a + b);
		out += 1;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI32ToI32_Mono(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		i32 a = in[0];
		i32 b = in[1];
		out[0] = (a + b) / 2;
		out += 1;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI32ToI16_Mono(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		out[0] = (i32)(in[0] + in[1]) / 4;
		out += 1;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI16ToR32_Mono(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		r32 a = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, (r32)in[0]);
		r32 b = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, (r32)in[1]);
		out[0] = 0.5f * (a + b);
		out += 1;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI16ToI32_Mono(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		out[0] = (i32)(in[0]) + (i32)(in[1]);
		out += 1;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI16ToI16_Mono(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		i16 a = in[0];
		i16 b = in[1];
		out[0] = (a + b) / 2;
		out += 1;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvR32ToR32_Stereo(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		r32 a = in[0];
		out[0] = a;
		out[1] = a;
		out += 2;
		in  += 1;
	}
}

static void AudioCnvR32ToI32_Stereo(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		i32 a = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[0]);
		out[0] = a;
		out[1] = a;
		out += 2;
		in  += 1;
	}
}

static void AudioCnvR32ToI16_Stereo(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		i16 a = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[0]);
		out[0] = a;
		out[1] = a;
		out += 2;
		in  += 1;
	}
}

static void AudioCnvI32ToR32_Stereo(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		r32 a = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, (r32)in[1]);
		out[0] = a;
		out[1] = a;
		out += 2;
		in  += 1;
	}
}

static void AudioCnvI32ToI32_Stereo(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		i32 a = in[0];
		out[0] = a;
		out[1] = a;
		out += 2;
		in  += 1;
	}
}

static void AudioCnvI32ToI16_Stereo(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		i16 a = (i16)(in[0] / 2);
		out[0] = a;
		out[1] = a;
		out += 2;
		in  += 1;
	}
}

static void AudioCnvI16ToR32_Stereo(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		r32 a = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, (r32)in[1]);
		out[0] = a;
		out[1] = a;
		out += 2;
		in  += 1;
	}
}

static void AudioCnvI16ToI32_Stereo(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		i32 a = (i32)in[0] * 2;
		out[0] = a;
		out[1] = a;
		out += 2;
		in  += 1;
	}
}

static void AudioCnvI16ToI16_Stereo(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	for (u32 fi = 0; fi < frames; ++fi) {
		i16 a = in[0];
		out[0] = a;
		out[1] = a;
		out += 2;
		in  += 1;
	}
}

static void AudioCnvR32ToR32_StereoInc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 remains  = cnv->Dst.Channels - 2;
	for (u32 fi = 0; fi < frames; ++fi) {
		r32 a = in[0];
		out[0] = a;
		out[1] = a;
		memset(out + 2, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += 1;
	}
}

static void AudioCnvR32ToI32_StereoInc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 remains  = cnv->Dst.Channels - 2;
	for (u32 fi = 0; fi < frames; ++fi) {
		i32 a = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[0]);
		out[0] = a;
		out[1] = a;
		memset(out + 2, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += 1;
	}
}

static void AudioCnvR32ToI16_StereoInc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 remains  = cnv->Dst.Channels - 2;
	for (u32 fi = 0; fi < frames; ++fi) {
		i16 a = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[0]);
		out[0] = a;
		out[1] = a;
		memset(out + 2, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += 1;
	}
}

static void AudioCnvI32ToR32_StereoInc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 remains  = cnv->Dst.Channels - 2;
	for (u32 fi = 0; fi < frames; ++fi) {
		r32 a = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, (r32)in[1]);
		out[0] = a;
		out[1] = a;
		memset(out + 2, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += 1;
	}
}

static void AudioCnvI32ToI32_StereoInc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 remains  = cnv->Dst.Channels - 2;
	for (u32 fi = 0; fi < frames; ++fi) {
		i32 a = in[0];
		out[0] = a;
		out[1] = a;
		memset(out + 2, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += 1;
	}
}

static void AudioCnvI32ToI16_StereoInc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 remains  = cnv->Dst.Channels - 2;
	for (u32 fi = 0; fi < frames; ++fi) {
		i16 a = (i16)(in[0] / 2);
		out[0] = a;
		out[1] = a;
		memset(out + 2, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += 1;
	}
}

static void AudioCnvI16ToR32_StereoInc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 remains  = cnv->Dst.Channels - 2;
	for (u32 fi = 0; fi < frames; ++fi) {
		r32 a = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, (r32)in[1]);
		out[0] = a;
		out[1] = a;
		memset(out + 2, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += 1;
	}
}

static void AudioCnvI16ToI32_StereoInc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 remains  = cnv->Dst.Channels - 2;
	for (u32 fi = 0; fi < frames; ++fi) {
		i32 a = (i32)in[0] * 2;
		out[0] = a;
		out[1] = a;
		memset(out + 2, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += 1;
	}
}

static void AudioCnvI16ToI16_StereoInc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 remains  = cnv->Dst.Channels - 2;
	for (u32 fi = 0; fi < frames; ++fi) {
		i16 a = in[0];
		out[0] = a;
		out[1] = a;
		memset(out + 2, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += 1;
	}
}

static void AudioCnvR32ToR32_Inc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		memcpy(out, in, channels * sizeof(r32));
		memset(out + channels, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvR32ToI32_Inc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		memset(out + channels, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvR32ToI16_Inc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		memset(out + channels, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI32ToR32_Inc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, (r32)in[ci]);
		memset(out + channels, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI32ToI32_Inc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		memcpy(out, in, channels * sizeof(i32));
		memset(out + channels, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI32ToI16_Inc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = (i16)(in[ci] / 2);
		memset(out + channels, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI16ToR32_Inc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, (r32)in[ci]);
		memset(out + channels, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI16ToI32_Inc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = (i32)in[ci] * 2;
		memset(out + channels, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI16ToI16_Inc(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		memcpy(out, in, channels * sizeof(i16));
		memset(out + channels, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvR32ToR32_Dec(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Dst.Channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		memcpy(out, in, channels * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvR32ToI32_Dec(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Dst.Channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvR32ToI16_Dec(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Dst.Channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI32ToR32_Dec(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Dst.Channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, (r32)in[ci]);
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI32ToI32_Dec(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Dst.Channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		memcpy(out, in, channels * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI32ToI16_Dec(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Dst.Channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = (i16)(in[ci] / 2);
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI16ToR32_Dec(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Dst.Channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, (r32)in[ci]);
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI16ToI32_Dec(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Dst.Channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		for (u32 ci = 0; ci < channels; ++ci)
			out[ci] = (i32)in[ci] * 2;
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvI16ToI16_Dec(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	u32 frames   = Min(dst_frames, src_frames);
	*written     = frames;
	*read        = frames;
	u32 channels = cnv->Dst.Channels;
	for (u32 fi = 0; fi < frames; ++fi) {
		memcpy(out, in, channels * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}
}

static void AudioCnvR32ToR32_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = in[i + 0 + ci];
			float b = in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = r;
		}
		out += channels;
		in  += channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI32_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = in[i + 0 + ci];
			float b = in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		}
		out += channels;
		in  += channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI16_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = in[i + 0 + ci];
			float b = in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		}
		out += channels;
		in  += channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToR32_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, r);
		}
		out += channels;
		in  += channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI32_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)r;
		}
		out += channels;
		in  += channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI16_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	u32 channels = cnv->Dst.Channels;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)(r * 0.5f);
		}
		out += channels;
		in  += channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToR32_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	u32 channels = cnv->Dst.Channels;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, r);
		}
		out += channels;
		in  += channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI32_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	u32 channels = cnv->Dst.Channels;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)(r * 2);
		}
		out += channels;
		in  += channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI16_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	u32 channels = cnv->Dst.Channels;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i16)r;
		}
		out += channels;
		in  += channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToR32_Mono_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = in[i + 0 + 0];
		float b = in[i + 1 + 0];
		float c = in[i + 0 + 1];
		float d = in[i + 1 + 1];
		float r = 0.5f * (Lerp(a, b, t) + Lerp(c, d, t));
		out[0] = r;
		out += 1;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI32_Mono_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = in[i + 0 + 0];
		float b = in[i + 1 + 0];
		float c = in[i + 0 + 1];
		float d = in[i + 1 + 1];
		float r = 0.5f * (Lerp(a, b, t) + Lerp(c, d, t));
		out[0] = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, r);
		out += 1;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI16_Mono_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = in[i + 0 + 0];
		float b = in[i + 1 + 0];
		float c = in[i + 0 + 1];
		float d = in[i + 1 + 1];
		float r = 0.5f * (Lerp(a, b, t) + Lerp(c, d, t));
		out[0] = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, r);
		out += 1;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToR32_Mono_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0 + 0];
		float b = (float)in[i + 1 + 0];
		float c = (float)in[i + 0 + 1];
		float d = (float)in[i + 1 + 1];
		float r = 0.5f * (Lerp(a, b, t) + Lerp(c, d, t));
		out[0] = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, r);
		out += 1;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI32_Mono_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0 + 0];
		float b = (float)in[i + 1 + 0];
		float c = (float)in[i + 0 + 1];
		float d = (float)in[i + 1 + 1];
		float r = 0.5f * (Lerp(a, b, t) + Lerp(c, d, t));
		out[0] = (i32)r;
		out += 1;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI16_Mono_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0 + 0];
		float b = (float)in[i + 1 + 0];
		float c = (float)in[i + 0 + 1];
		float d = (float)in[i + 1 + 1];
		float r = 0.5f * (Lerp(a, b, t) + Lerp(c, d, t));
		out[0] = (i32)(r * 0.5f);
		out += 1;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToR32_Mono_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0 + 0];
		float b = (float)in[i + 1 + 0];
		float c = (float)in[i + 0 + 1];
		float d = (float)in[i + 1 + 1];
		float r = 0.5f * (Lerp(a, b, t) + Lerp(c, d, t));
		out[0] = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, r);
		out += 1;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI32_Mono_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0 + 0];
		float b = (float)in[i + 1 + 0];
		float c = (float)in[i + 0 + 1];
		float d = (float)in[i + 1 + 1];
		float r = 0.5f * (Lerp(a, b, t) + Lerp(c, d, t));
		out[0] = (i32)(r * 2);
		out += 1;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI16_Mono_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0 + 0];
		float b = (float)in[i + 1 + 0];
		float c = (float)in[i + 0 + 1];
		float d = (float)in[i + 1 + 1];
		float r = 0.5f * (Lerp(a, b, t) + Lerp(c, d, t));
		out[0] = (i16)r;
		out += 1;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToR32_Stereo_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = in[i + 0];
		float b = in[i + 1];
		float r = Lerp(a, b, t);
		out[0] = r;
		out[1] = r;
		out += 2;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI32_Stereo_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = in[i + 0];
		float b = in[i + 1];
		float r = Lerp(a, b, t);
		i32 o = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, r);
		out[0] = o;
		out[1] = o;
		out += 2;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI16_Stereo_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = in[i + 0];
		float b = in[i + 1];
		float r = Lerp(a, b, t);
		i16 o = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, r);
		out[0] = o;
		out[1] = o;
		out += 2;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToR32_Stereo_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		r32 o = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, r);
		out[0] = o;
		out[1] = o;
		out += 2;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI32_Stereo_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		out[0] = (i32)r;
		out[1] = (i32)r;
		out += 2;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI16_Stereo_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		i16 o = (i32)(r * 0.5f);
		out[0] = o;
		out[1] = o;
		out += 2;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToR32_Stereo_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		r32 o = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, r);
		out[0] = o;
		out[1] = o;
		out += 2;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI32_Stereo_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		i32 o = (i32)(r * 2);
		out[0] = o;
		out[1] = o;
		out += 2;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI16_Stereo_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		out[0] = (i16)r;
		out[1] = (i16)r;
		out += 2;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToR32_StereoInc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 remains  = cnv->Dst.Channels - 2;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = in[i + 0];
		float b = in[i + 1];
		float r = Lerp(a, b, t);
		out[0]  = r;
		out[1]  = r;
		memset(out + 2, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI32_StereoInc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 remains  = cnv->Dst.Channels - 2;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = in[i + 0];
		float b = in[i + 1];
		float r = Lerp(a, b, t);
		out[0]  = (i32)r;
		out[1]  = (i32)r;
		memset(out + 2, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI16_StereoInc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 remains  = cnv->Dst.Channels - 2;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = in[i + 0];
		float b = in[i + 1];
		float r = Lerp(a, b, t);
		out[0]  = (i16)r;
		out[1]  = (i16)r;
		memset(out + 2, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToR32_StereoInc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 remains  = cnv->Dst.Channels - 2;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		out[0]  = r;
		out[1]  = r;
		memset(out + 2, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI32_StereoInc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 remains  = cnv->Dst.Channels - 2;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		out[0]  = (i32)r;
		out[1]  = (i32)r;
		memset(out + 2, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI16_StereoInc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 remains  = cnv->Dst.Channels - 2;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		out[0]  = (i16)r;
		out[1]  = (i16)r;
		memset(out + 2, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToR32_StereoInc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 remains  = cnv->Dst.Channels - 2;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		out[0]  = r;
		out[1]  = r;
		memset(out + 2, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI32_StereoInc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 remains  = cnv->Dst.Channels - 2;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		out[0]  = (i32)r;
		out[1]  = (i32)r;
		memset(out + 2, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI16_StereoInc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 remains  = cnv->Dst.Channels - 2;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		float a = (float)in[i + 0];
		float b = (float)in[i + 1];
		float r = Lerp(a, b, t);
		out[0]  = (i16)r;
		out[1]  = (i16)r;
		memset(out + 2, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += 1;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToR32_Inc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = in[i + 0 + ci];
			float b = in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = r;
		}
		memset(out + channels, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI32_Inc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = in[i + 0 + ci];
			float b = in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		}
		memset(out + channels, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI16_Inc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = in[i + 0 + ci];
			float b = in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		}
		memset(out + channels, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToR32_Inc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, r);
		}
		memset(out + channels, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI32_Inc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)r;
		}
		memset(out + channels, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI16_Inc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)(r * 0.5f);
		}
		memset(out + channels, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToR32_Inc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, r);
		}
		memset(out + channels, 0, remains * sizeof(r32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI32_Inc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)(r * 2);
		}
		memset(out + channels, 0, remains * sizeof(i32));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI16_Inc_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;
	u32 remains  = cnv->Dst.Channels - channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i16)r;
		}
		memset(out + channels, 0, remains * sizeof(i16));
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToR32_Dec_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = in[i + 0 + ci];
			float b = in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = r;
		}
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI32_Dec_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = in[i + 0 + ci];
			float b = in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		}
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvR32ToI16_Dec_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	r32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = in[i + 0 + ci];
			float b = in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i16)MapRange(-1, 1, (r32)INT32_MIN, (r32)INT32_MAX, in[ci]);
		}
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToR32_Dec_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = MapRange((r32)INT32_MIN, (r32)INT32_MAX, -1, 1, r);
		}
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI32_Dec_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)r;
		}
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI32ToI16_Dec_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i32 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)(r * 0.5f);
		}
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToR32_Dec_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	r32 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = MapRange((r32)INT16_MIN, (r32)INT16_MAX, -1, 1, r);
		}
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI32_Dec_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i32 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i32)(r * 2);
		}
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

static void AudioCnvI16ToI16_Dec_Resample(AudioCnv *cnv, void *dst, u32 dst_frames, void *src, u32 src_frames, u32 *written, u32 *read) {
	i16 *out     = dst;
	i16 *in      = src;
	r32 factor   = (float)cnv->Src.Frequency / (float)cnv->Dst.Frequency;
	u32 channels = cnv->Src.Channels;

	u32 di = 0;
	for (; di < dst_frames; ++di) {
		float si = (float) di * factor;
		int   i  = (int)si;
		float t  = si - i;

		if ((u32)i + 1 >= src_frames)
			break;

		for (u32 ci = 0; ci < channels; ++ci) {
			float a = (float)in[i + 0 + ci];
			float b = (float)in[i + 1 + ci];
			float r = Lerp(a, b, t);
			out[ci] = (i16)r;
		}
		out += cnv->Dst.Channels;
		in  += cnv->Src.Channels;
	}

	*written = di;
	*read    = (u32)(di * factor);
}

