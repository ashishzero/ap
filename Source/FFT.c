#include "FFT.h"
#include "TwiddleFactorFFT.h"

#include <math.h>

#define MATH_PI    3.14159265359

//
// Complex Number Utilities
//

Complex ComplexAdd(Complex a, Complex b) {
	return (Complex) {
		.re = a.re + b.re,
		.im = a.im + b.im
	};
}

Complex ComplexSub(Complex a, Complex b) {
	return (Complex) {
		.re = a.re - b.re,
		.im = a.im - b.im
	};
}

Complex ComplexMul(Complex a, Complex b) {
	return (Complex) {
		.re = a.re * b.re - a.im * b.im,
		.im = a.re * b.im + a.im * b.re
	};
}

Complex ComplexPolar(real e) {
	return (Complex) {
		.re = cosf(e),
		.im = sinf(e)
	};
}

Complex ComplexRect(real re, real im) {
	return (Complex) {
		.re = re,
		.im = im
	};
}

//
// Twiddle Factors Lookup
//

static Complex TwiddleFactor(uint half_turns) {
	if (half_turns < ArrayCount(TwiddleFactors)) {
		uint index = half_turns << 1;
		return ComplexRect(TwiddleFactors[index], TwiddleFactors[index + 1]);
	}
	return ComplexPolar((float)(-MATH_PI / (double)(half_turns)));
}

static Complex InvTwiddleFactor(uint half_turns) {
	if (half_turns < ArrayCount(TwiddleFactors)) {
		uint index = half_turns << 1;
		return ComplexRect(TwiddleFactors[index], -TwiddleFactors[index + 1]);
	}
	return ComplexPolar((float)(MATH_PI / (double)(half_turns)));
}

//
// Procedures to rearrange data by bit-reversing permutation
// 

void FFT_Rearrange(Complex *output, Complex *input, uint count) {
	uint target = 0;
	for (uint pos = 0; pos < count; ++pos) {
		output[pos] = input[target];

		uint mask = count;
		while (target & (mask >>= 1)) {
			target &= ~mask;
		}
		target |= mask;
	}
}

void FFT_RearrangeInplace(Complex *data, uint count) {
	uint target = 0;
	for (uint pos = 0; pos < count; ++pos) {
		if (target > pos) {
			Complex temp = data[pos];
			data[pos]    = data[target];
			data[target] = temp;
		}

		uint mask = count;
		while (target & (mask >>= 1)) {
			target &= ~mask;
		}
		target |= mask;
	}
}

//
// FFT Utilities
//

void FFT_Forward(Complex *data, uint count) {
	Assert((count & (count - 1)) == 0);

	// radix = 2
	for (uint index = 0; index < count; index += 2) {
		Complex p = data[index + 0];
		Complex q = data[index + 1];
		data[index + 0] = ComplexAdd(p, q);
		data[index + 1] = ComplexSub(p, q);
	}

	// radix = 4
	for (uint index = 0; index < count; index += 4) {
		Complex p = data[index + 0];
		Complex r = data[index + 1];
		Complex q = data[index + 2];
		Complex s = data[index + 3];

		s = (Complex) { s.im, -s.re };

		data[index + 0] = ComplexAdd(p, q);
		data[index + 1] = ComplexAdd(r, s);
		data[index + 2] = ComplexSub(p, q);
		data[index + 3] = ComplexSub(r, s);
	}

	for (uint step = 4; step < count; step <<= 1) {
		uint jump  = step << 1;
		Complex tw = TwiddleFactor(step);
		Complex w  = ComplexRect(1.0f, 0.0f);
		for (uint block = 0; block < step; ++block) {
			for (uint index = block; index < count; index += jump) {
				uint next   = index + step;
				Complex a   = data[index];
				Complex b   = ComplexMul(w, data[next]);
				data[index] = ComplexAdd(a, b);
				data[next]  = ComplexSub(a, b);
			}
			w = ComplexMul(w, tw);
		}
	}
}

void FFT_Reverse(Complex *data, uint count) {
	Assert((count & (count - 1)) == 0);

	// radix = 2
	for (uint index = 0; index < count; index += 2) {
		Complex p = data[index + 0];
		Complex q = data[index + 1];
		data[index + 0] = ComplexAdd(p, q);
		data[index + 1] = ComplexSub(p, q);
	}

	// radix = 4
	for (uint index = 0; index < count; index += 4) {
		Complex p = data[index + 0];
		Complex r = data[index + 1];
		Complex q = data[index + 2];
		Complex s = data[index + 3];

		s = (Complex) { -s.im, s.re };

		data[index + 0] = ComplexAdd(p, q);
		data[index + 1] = ComplexAdd(r, s);
		data[index + 2] = ComplexSub(p, q);
		data[index + 3] = ComplexSub(r, s);
	}

	for (uint step = 4; step < count; step <<= 1) {
		uint jump  = step << 1;
		Complex tw = InvTwiddleFactor(step);
		Complex w  = ComplexRect(1.0f, 0.0f);
		for (uint block = 0; block < step; ++block) {
			for (uint index = block; index < count; index += jump) {
				uint next   = index + step;
				Complex a   = data[index];
				Complex b   = ComplexMul(w, data[next]);
				data[index] = ComplexAdd(a, b);
				data[next]  = ComplexSub(a, b);
			}
			w = ComplexMul(w, tw);
		}
	}

	float factor = 1.0f / count;
	for (uint index = 0; index < count; ++index) {
		data[index].re *= factor;
		data[index].im *= factor;
	}
}

//
// High-Level procedures
//

void FFT(Complex *output, Complex *input, uint count) {
	FFT_Rearrange(output, input, count);
	FFT_Forward(output, count);
}

void InvFFT(Complex *output, Complex *input, uint count) {
	FFT_Rearrange(output, input, count);
	FFT_Reverse(output, count);
}

void InplaceFFT(Complex *data, uint count) {
	FFT_RearrangeInplace(data, count);
	FFT_Forward(data, count);
}

void InplaceInvFFT(Complex *data, uint count) {
	FFT_RearrangeInplace(data, count);
	FFT_Reverse(data, count);
}
