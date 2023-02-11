#pragma once
#include "KrPlatform.h"

typedef struct Complex {
	real re;
	real im;
} Complex;

void FFT_Rearrange(Complex *output, Complex *input, uint count);
void FFT_RearrangeInplace(Complex *data, uint count);

void FFT_Forward(Complex *data, uint count);
void FFT_Reverse(Complex *data, uint count);

void FFT(Complex *output, Complex *input, uint count);
void InvFFT(Complex *output, Complex *input, uint count);
void InplaceFFT(Complex *data, uint count);
void InplaceInvFFT(Complex *data, uint count);
