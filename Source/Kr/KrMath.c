#include "KrMath.h"

bool AlmostEqual(float a, float b, float delta) {
	float diff   = Abs(a - b);
	a            = Abs(a);
	b            = Abs(b);
	float larger = (b > a) ? b : a;
	if (diff <= larger * delta)
		return true;
	return false;
}

float Wrap(float min, float max, float a) {
	float range  = max - min;
	float offset = a - min;
	float result = (offset - (Floor(offset / range) * range) + min);
	return result;
}

float WrapPhase(float turns) {
	return Wrap(-0.5f, 0.5f, turns);
}

V2f ComplexMul(V2f a, V2f b) {
	V2f c;
	c.re = a.re * b.re - a.im * b.im;
	c.im = a.re * b.im + a.im * b.re;
	return c;
}

V2f ComplexPolar(float amp, float turns) {
	V2f c;
	c.re = amp * Cos(turns);
	c.im = amp * Sin(turns);
	return c;
}

V2f ComplexRect(float re, float im) {
	V2f c;
	c.re = re;
	c.im = im;
	return c;
}

V2f ComplexConjugate(V2f c) {
	c.im = -c.im;
	return c;
}

float ComplexAmplitudeSq(V2f c) {
	float ampsq = c.re * c.re + c.im * c.im;
	return ampsq;
}

float ComplexAmplitude(V2f c) {
	float ampsq = ComplexAmplitudeSq(c);
	return Sqrt(ampsq);
}

float ComplexPhase(V2f c) {
	float phase = ArcTan2(c.im, c.re);
	return phase;
}

V2f V2fNeg(V2f a) {
	a.x = -a.x;
	a.y = -a.y;
	return a;
}

V3f V3fNeg(V3f a) {
	a.x = -a.x;
	a.y = -a.y;
	a.z = -a.z;
	return a;
}

V4f V4fNeg(V4f a) {
	a.x = -a.x;
	a.y = -a.y;
	a.z = -a.z;
	a.w = -a.w;
	return a;
}

V2i V2iNeg(V2i a) {
	a.x = -a.x;
	a.y = -a.y;
	return a;
}

V3i V3iNeg(V3i a) {
	a.x = -a.x;
	a.y = -a.y;
	a.z = -a.z;
	return a;
}

V4i V4iNeg(V4i a) {
	a.x = -a.x;
	a.y = -a.y;
	a.z = -a.z;
	a.w = -a.w;
	return a;
}

V2f V2fAdd(V2f a, V2f b) {
	V2f r;
	r.x = a.x + b.x;
	r.y = a.y + b.y;
	return r;
}

V2f V2fSub(V2f a, V2f b) {
	V2f r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	return r;
}

V2f V2fMul(V2f a, V2f b) {
	V2f r;
	r.x = a.x * b.x;
	r.y = a.y * b.y;
	return r;
}

V2f V2fDiv(V2f a, V2f b) {
	V2f r;
	r.x = a.x / b.x;
	r.y = a.y / b.y;
	return r;
}

V2f V2fMulF(V2f a, float b) {
	V2f r;
	r.x = a.x * b;
	r.y = a.y * b;
	return r;
}

V2f V2fDivF(V2f a, float b) {
	V2f r;
	r.x = a.x / b;
	r.y = a.y / b;
	return r;
}

void V2fAddScaled(V2f *r, float s, V2f o) {
	r->x += o.x * s;
	r->y += o.y * s;
}

void V2fSubScaled(V2f *r, float s, V2f o) {
	r->x -= o.x * s;
	r->y -= o.y * s;
}

V3f V3fAdd(V3f a, V3f b) {
	V3f r;
	r.x = a.x + b.x;
	r.y = a.y + b.y;
	r.z = a.z + b.z;
	return r;
}

V3f V3fSub(V3f a, V3f b) {
	V3f r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	r.z = a.z - b.z;
	return r;
}

V3f V3fMul(V3f a, V3f b) {
	V3f r;
	r.x = a.x * b.x;
	r.y = a.y * b.y;
	r.z = a.z * b.z;
	return r;
}

V3f V3fDiv(V3f a, V3f b) {
	V3f r;
	r.x = a.x / b.x;
	r.y = a.y / b.y;
	r.z = a.z / b.z;
	return r;
}

V3f V3fMulF(V3f a, float b) {
	V3f r;
	r.x = a.x * b;
	r.y = a.y * b;
	r.z = a.z * b;
	return r;
}

V3f V3fDivF(V3f a, float b) {
	V3f r;
	r.x = a.x / b;
	r.y = a.y / b;
	r.z = a.z / b;
	return r;
}

void V3fAddScaled(V3f *r, float s, V3f o) {
	r->x += o.x * s;
	r->y += o.y * s;
	r->z += o.z * s;
}

void V3fSubScaled(V3f *r, float s, V3f o) {
	r->x -= o.x * s;
	r->y -= o.y * s;
	r->z -= o.z * s;
}

V4f V4fAdd(V4f a, V4f b) {
	V4f r;
	r.x = a.x + b.x;
	r.y = a.y + b.y;
	r.z = a.z + b.z;
	r.w = a.w + b.w;
	return r;
}

V4f V4fSub(V4f a, V4f b) {
	V4f r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	r.z = a.z - b.z;
	r.w = a.w - b.w;
	return r;
}

V4f V4fMul(V4f a, V4f b) {
	V4f r;
	r.x = a.x * b.x;
	r.y = a.y * b.y;
	r.z = a.z * b.z;
	r.w = a.w * b.w;
	return r;
}

V4f V4fDiv(V4f a, V4f b) {
	V4f r;
	r.x = a.x / b.x;
	r.y = a.y / b.y;
	r.z = a.z / b.z;
	r.w = a.w / b.w;
	return r;
}

V4f V4fMulF(V4f a, float b) {
	V4f r;
	r.x = a.x * b;
	r.y = a.y * b;
	r.z = a.z * b;
	r.w = a.w * b;
	return r;
}

V4f V4fDivF(V4f a, float b) {
	V4f r;
	r.x = a.x / b;
	r.y = a.y / b;
	r.z = a.z / b;
	r.w = a.w / b;
	return r;
}

void V4fAddScaled(V4f *r, float s, V4f o) {
	r->x += o.x * s;
	r->y += o.y * s;
	r->z += o.z * s;
	r->w += o.w * s;
}

void V4fSubScaled(V4f *r, float s, V4f o) {
	r->x -= o.x * s;
	r->y -= o.y * s;
	r->z -= o.z * s;
	r->w -= o.w * s;
}

V2i V2iAdd(V2i a, V2i b) {
	V2i r;
	r.x = a.x + b.x;
	r.y = a.y + b.y;
	return r;
}

V2i V2iSub(V2i a, V2i b) {
	V2i r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	return r;
}

V2i V2iMul(V2i a, V2i b) {
	V2i r;
	r.x = a.x * b.x;
	r.y = a.y * b.y;
	return r;
}

V2i V2iDiv(V2i a, V2i b) {
	V2i r;
	r.x = a.x / b.x;
	r.y = a.y / b.y;
	return r;
}

V2i V2iMulI(V2i a, int b) {
	V2i r;
	r.x = a.x * b;
	r.y = a.y * b;
	return r;
}

V2i V2iDivI(V2i a, int b) {
	V2i r;
	r.x = a.x / b;
	r.y = a.y / b;
	return r;
}

void V2iAddScaled(V2i *r, int s, V2i o) {
	r->x += o.x * s;
	r->y += o.y * s;
}

void V2iSubScaled(V2i *r, int s, V2i o) {
	r->x -= o.x * s;
	r->y -= o.y * s;
}

V3i V3iAdd(V3i a, V3i b) {
	V3i r;
	r.x = a.x + b.x;
	r.y = a.y + b.y;
	r.z = a.z + b.z;
	return r;
}

V3i V3iSub(V3i a, V3i b) {
	V3i r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	r.z = a.z - b.z;
	return r;
}

V3i V3iMul(V3i a, V3i b) {
	V3i r;
	r.x = a.x * b.x;
	r.y = a.y * b.y;
	r.z = a.z * b.z;
	return r;
}

V3i V3iDiv(V3i a, V3i b) {
	V3i r;
	r.x = a.x / b.x;
	r.y = a.y / b.y;
	r.z = a.z / b.z;
	return r;
}

V3i V3iMulI(V3i a, int b) {
	V3i r;
	r.x = a.x * b;
	r.y = a.y * b;
	r.z = a.z * b;
	return r;
}

V3i V3iDivI(V3i a, int b) {
	V3i r;
	r.x = a.x / b;
	r.y = a.y / b;
	r.z = a.z / b;
	return r;
}

void V3iAddScaled(V3i *r, int s, V3i o) {
	r->x += o.x * s;
	r->y += o.y * s;
	r->z += o.z * s;
}

void V3iSubScaled(V3i *r, int s, V3i o) {
	r->x -= o.x * s;
	r->y -= o.y * s;
	r->z -= o.z * s;
}

V4i V4iAdd(V4i a, V4i b) {
	V4i r;
	r.x = a.x + b.x;
	r.y = a.y + b.y;
	r.z = a.z + b.z;
	r.w = a.w + b.w;
	return r;
}

V4i V4iSub(V4i a, V4i b) {
	V4i r;
	r.x = a.x - b.x;
	r.y = a.y - b.y;
	r.z = a.z - b.z;
	r.w = a.w - b.w;
	return r;
}

V4i V4iMul(V4i a, V4i b) {
	V4i r;
	r.x = a.x * b.x;
	r.y = a.y * b.y;
	r.z = a.z * b.z;
	r.w = a.w * b.w;
	return r;
}

V4i V4iDiv(V4i a, V4i b) {
	V4i r;
	r.x = a.x / b.x;
	r.y = a.y / b.y;
	r.z = a.z / b.z;
	r.w = a.w / b.w;
	return r;
}

V4i V4iMulI(V4i a, int b) {
	V4i r;
	r.x = a.x * b;
	r.y = a.y * b;
	r.z = a.z * b;
	r.w = a.w * b;
	return r;
}

V4i V4iDivI(V4i a, int b) {
	V4i r;
	r.x = a.x / b;
	r.y = a.y / b;
	r.z = a.z / b;
	r.w = a.w / b;
	return r;
}

void V4iAddScaled(V4i *r, int s, V4i o) {
	r->x += o.x * s;
	r->y += o.y * s;
	r->z += o.z * s;
	r->w += o.w * s;
}

void V4iSubScaled(V4i *r, int s, V4i o) {
	r->x -= o.x * s;
	r->y -= o.y * s;
	r->z -= o.z * s;
	r->w -= o.w * s;
}

float V2fDot(V2f a, V2f b) {
	float dot = a.x * b.x + a.y * b.y;
	return dot;
}

float V3fDot(V3f a, V3f b) {
	float dot = a.x * b.x + a.y * b.y + a.z * b.z;
	return dot;
}

float V4fDot(V4f a, V4f b) {
	float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	return dot;
}

float V2fDet(V2f a, V2f b) {
	float det = a.x * b.y - a.y * b.x;
	return det;
}

int V2iDot(V2i a, V2i b) {
	int dot = a.x * b.x + a.y * b.y;
	return dot;
}

int V3iDot(V3i a, V3i b) {
	int dot = a.x * b.x + a.y * b.y + a.z * b.z;
	return dot;
}

int V4iDot(V4i a, V4i b) {
	int dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	return dot;
}

int V2iDet(V2i a, V2i b) {
	int det = a.x * b.y - a.y * b.x;
	return det;
}

float V2fCross(V2f a, V2f b) {
	float cross = (a.x * b.y) - (a.y * b.x);
	return cross;
}

V3f V3fCross(V3f a, V3f b) {
	V3f cross;
	cross.x = (a.y * b.z) - (a.z * b.y);
	cross.y = (a.z * b.x) - (a.x * b.z);
	cross.z = (a.x * b.y) - (a.y * b.x);
	return cross;
}

int V2iCross(V2i a, V2i b) {
	int cross = (a.x * b.y) - (a.y * b.x);
	return cross;
}

V3i V3iCross(V3i a, V3i b) {
	V3i cross;
	cross.x = (a.y * b.z) - (a.z * b.y);
	cross.y = (a.z * b.x) - (a.x * b.z);
	cross.z = (a.x * b.y) - (a.y * b.x);
	return cross;
}

V2f V2fTriple(V2f a, V2f b, V2f c) {
	float det = V2fDet(a, b);
	V2f triple;
	triple.x = -c.y * det;
	triple.y =  c.x * det;
	return triple;
}

V3f V3fTriple(V3f a, V3f b, V3f c) {
	V3f cross  = V3fCross(a, b);
	V3f triple = V3fCross(cross, c);
	return triple;
}

V2i V2iTriple(V2i a, V2i b, V2i c) {
	int det = V2iDet(a, b);
	V2i triple;
	triple.x = -c.y * det;
	triple.y =  c.x * det;
	return triple;
}

V3i V3iTriple(V3i a, V3i b, V3i c) {
	V3i cross  = V3iCross(a, b);
	V3i triple = V3iCross(cross, c);
	return triple;
}

float V2fLengthSq(V2f a) {
	return V2fDot(a, a);
}

float V2fLength(V2f a) {
	return Sqrt(V2fLengthSq(a));
}

float V3fLengthSq(V3f a) {
	return V3fDot(a, a);
}

float V3fLength(V3f a) {
	return Sqrt(V3fLengthSq(a));
}

float V4fLengthSq(V4f a) {
	return V4fDot(a, a);
}

float V4fLength(V4f a) {
	return Sqrt(V4fLengthSq(a));
}

int V2iLengthSq(V2i a) {
	return V2iDot(a, a);
}

int V2iLength(V2i a) {
	return (int)Sqrt((float)V2iLengthSq(a));
}

int V3iLengthSq(V3i a) {
	return V3iDot(a, a);
}

int V3iLength(V3i a) {
	return (int)Sqrt((float)V3iLengthSq(a));
}

int V4iLengthSq(V4i a) {
	return V4iDot(a, a);
}

int V4iLength(V4i a) {
	return (int)Sqrt((float)V4iLengthSq(a));
}

float V2fDistance(V2f a, V2f b) {
	V2f vel = V2fSub(b, a);
	return V2fLength(vel);
}

float V3fDistance(V3f a, V3f b) {
	V3f vel = V3fSub(b, a);
	return V3fLength(vel);
}

float V4fDistance(V4f a, V4f b) {
	V4f vel = V4fSub(b, a);
	return V4fLength(vel);
}

int V2iDistance(V2i a, V2i b) {
	V2i vel = V2iSub(b, a);
	return V2iLength(vel);
}

int V3iDistance(V3i a, V3i b) {
	V3i vel = V3iSub(b, a);
	return V3iLength(vel);
}

int V4iDistance(V4i a, V4i b) {
	V4i vel = V4iSub(b, a);
	return V4iLength(vel);
}

V2f V2fNormalizeOrZero(V2f a) {
	float len = V2fLength(a);
	if (len != 0) {
		float ilen = 1.0f / len;
		return V2fMulF(a, ilen);
	}
	return (V2f){0};
}

V2f V2fNormalize(V2f a) {
	float len = V2fLength(a);
	float ilen = 1.0f / len;
	return V2fMulF(a, ilen);
}

V3f V3fNormalizeOrZero(V3f a) {
	float len = V3fLength(a);
	if (len != 0) {
		float ilen = 1.0f / len;
		return V3fMulF(a, ilen);
	}
	return (V3f){0};
}

V3f V3fNormalize(V3f a) {
	float len = V3fLength(a);
	float ilen = 1.0f / len;
	return V3fMulF(a, ilen);
}

V4f V4fNormalizeOrZero(V4f a) {
	float len = V4fLength(a);
	if (len != 0) {
		float ilen = 1.0f / len;
		return V4fMulF(a, ilen);
	}
	return (V4f){0};
}

V4f V4fNormalize(V4f a) {
	float len = V4fLength(a);
	float ilen = 1.0f / len;
	return V4fMulF(a, ilen);
}

V2i V2iNormalizeOrZero(V2i a) {
	int len = V2iLength(a);
	if (len != 0) {
		return V2iDivI(a, len);
	}
	return (V2i){0};
}

V2i V2iNormalize(V2i a) {
	int len = V2iLength(a);
	return V2iDivI(a, len);
}

V3i V3iNormalizeOrZero(V3i a) {
	int len = V3iLength(a);
	if (len != 0) {
		return V3iDivI(a, len);
	}
	return (V3i){0};
}

V3i V3iNormalize(V3i a) {
	int len = V3iLength(a);
	return V3iDivI(a, len);
}

V4i V4iNormalizeOrZero(V4i a) {
	int len = V4iLength(a);
	if (len != 0) {
		return V4iDivI(a, len);
	}
	return (V4i){0};
}

V4i V4iNormalize(V4i a) {
	int len = V4iLength(a);
	return V4iDivI(a, len);
}

void V3fOrthoNormalBasis(V3f *r, V3f *u, V3f *f) {
	*r = V3fNormalizeOrZero(*r);
	*f = V3fCross(*u, *r);
	if (V3fLength(*f) == 0) return;
	*f = V3fNormalize(*f);
	*u = V3fCross(*r, *f);
}

void V3iOrthoNormalBasis(V3i *r, V3i *u, V3i *f) {
	*r = V3iNormalizeOrZero(*r);
	*f = V3iCross(*u, *r);
	if (V3iLength(*f) == 0) return;
	*f = V3iNormalize(*f);
	*u = V3iCross(*r, *f);
}

float V2fTurns(V2f a, V2f b) {
	float dot    = V2fDot(a, b);
	dot          = Clamp(-1, 1, dot);
	float  turns = ArcCos(dot);
	float  cross = V2fCross(a, b);
	if (cross < 0)
		turns = -turns;
	return turns;
}

float V3fTurns(V3f a, V3f b, V3f norm) {
	float dot    = V3fDot(a, b);
	dot          = Clamp(-1, 1, dot);
	float  turns = ArcCos(dot);
	V3f cross = V3fCross(a, b);
	if (V3fDot(norm, cross) < 0)
		turns = -turns;
	return turns;
}

float V2iTurns(V2i a, V2i b) {
	int dot      = V2iDot(a, b);
	dot          = Clamp(-1, 1, dot);
	float  turns = ArcCos((float)dot);
	int    cross = V2iCross(a, b);
	if (cross < 0)
		turns = -turns;
	return turns;
}

float V3iTurns(V3i a, V3i b, V3i norm) {
	int dot      = V3iDot(a, b);
	dot          = Clamp(-1, 1, dot);
	float  turns = ArcCos((float)dot);
	V3i cross = V3iCross(a, b);
	if (V3iDot(norm, cross) < 0)
		turns = -turns;
	return turns;
}

M2f M2fNeg(M2f const *a) {
	M2f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = -a->m[i];
	}
	return res;
}

M3f M3fNeg(M3f const *a) {
	M3f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = -a->m[i];
	}
	return res;
}

M4f M4fNeg(M4f const *a) {
	M4f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = -a->m[i];
	}
	return res;
}

M2f M2fAdd(M2f const *a, M2f const *b) {
	M2f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] + b->m[i];
	}
	return res;
}

M3f M3fAdd(M3f const *a, M3f const *b) {
	M3f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] + b->m[i];
	}
	return res;
}

M4f M4fAdd(M4f const *a, M4f const *b) {
	M4f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] + b->m[i];
	}
	return res;
}

M2f M2fSub(M2f const *a, M2f const *b) {
	M2f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] - b->m[i];
	}
	return res;
}

M3f M3fSub(M3f const *a, M3f const *b) {
	M3f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] - b->m[i];
	}
	return res;
}

M4f M4fSub(M4f const *a, M4f const *b) {
	M4f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] - b->m[i];
	}
	return res;
}

M2f M2fMulF(M2f const *a, float b) {
	M2f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] * b;
	}
	return res;
}

M3f M3fMulF(M3f const *a, float b) {
	M3f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] * b;
	}
	return res;
}

M4f M4fMulF(M4f const *a, float b) {
	M4f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] * b;
	}
	return res;
}

M2f M2fDivF(M2f const *a, float b) {
	M2f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] / b;
	}
	return res;
}

M3f M3fDivF(M3f const *a, float b) {
	M3f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] / b;
	}
	return res;
}

M4f M4fDivF(M4f const *a, float b) {
	M4f res;
	for (int i = 0; i < ArrayCount(res.m); ++i) {
		res.m[i] = a->m[i] / b;
	}
	return res;
}

float M2fDet(M2f const *mat) {
	return mat->m2[0][0] * mat->m2[1][1] - mat->m2[0][1] * mat->m2[1][0];
}

M2f M2fInverse(M2f const *mat) {
	float det = mat->m2[0][0] * mat->m2[1][1] - mat->m2[0][1] * mat->m2[1][0];
	det /= det;
	M2f res;
	res.m2[0][0] = mat->m2[1][1];
	res.m2[0][1] = -mat->m2[0][1];
	res.m2[1][0] = -mat->m2[1][0];
	res.m2[1][1] = mat->m2[0][0];
	res.m[0] *= det;
	res.m[1] *= det;
	res.m[2] *= det;
	res.m[3] *= det;
	return res;
}

M2f M2fTranspose(M2f const *m) {
	M2f t;
	t.m2[0][0] = m->m2[0][0];
	t.m2[0][1] = m->m2[1][0];
	t.m2[1][0] = m->m2[0][1];
	t.m2[1][1] = m->m2[1][1];
	return t;
}

float M3fDet(M3f const *mat) {
	float det = mat->m2[0][0] * (mat->m2[1][1] * mat->m2[2][2] - mat->m2[2][1] * mat->m2[1][2]) +
		mat->m2[0][1] * (mat->m2[1][2] * mat->m2[2][0] - mat->m2[1][0] * mat->m2[2][2]) +
		mat->m2[0][2] * (mat->m2[1][0] * mat->m2[2][1] - mat->m2[2][0] * mat->m2[1][1]);
	return det;
}

M3f M3fInverse(M3f const *mat) {
	M3f result;
	result.m2[0][0] = mat->m2[1][1] * mat->m2[2][2] - mat->m2[2][1] * mat->m2[1][2];
	result.m2[0][1] = mat->m2[0][2] * mat->m2[2][1] - mat->m2[0][1] * mat->m2[2][2];
	result.m2[0][2] = mat->m2[0][1] * mat->m2[1][2] - mat->m2[0][2] * mat->m2[1][1];
	result.m2[1][0] = mat->m2[1][2] * mat->m2[2][0] - mat->m2[1][0] * mat->m2[2][2];
	result.m2[1][1] = mat->m2[0][0] * mat->m2[2][2] - mat->m2[0][2] * mat->m2[2][0];
	result.m2[1][2] = mat->m2[1][0] * mat->m2[0][2] - mat->m2[0][0] * mat->m2[1][2];
	result.m2[2][0] = mat->m2[1][0] * mat->m2[2][1] - mat->m2[2][0] * mat->m2[1][1];
	result.m2[2][1] = mat->m2[2][0] * mat->m2[0][1] - mat->m2[0][0] * mat->m2[2][1];
	result.m2[2][2] = mat->m2[0][0] * mat->m2[1][1] - mat->m2[1][0] * mat->m2[0][1];

	float det = mat->m2[0][0] * result.m2[0][0] + mat->m2[0][1] * result.m2[1][0] + mat->m2[0][2] * result.m2[2][0];
	det /= det;
	for (int i = 0; i < 3; i++)
		result.rows[i] = V3fMulF(result.rows[i], det);
	return result;
}

M3f M3fTranspose(M3f const *m) {
	M3f res;
	res.rows[0] = (V3f){ m->m2[0][0], m->m2[1][0], m->m2[2][0] };
	res.rows[1] = (V3f){ m->m2[0][1], m->m2[1][1], m->m2[2][1] };
	res.rows[2] = (V3f){ m->m2[0][2], m->m2[1][2], m->m2[2][2] };
	return res;
}

float M4fDet(M4f const *mat) {
	float m0 =  mat->m[5] * mat->m[10] * mat->m[15] - mat->m[5] * mat->m[11] * mat->m[14] -
				mat->m[9] * mat->m[6] * mat->m[15] + mat->m[9] * mat->m[7] * mat->m[14] +
				mat->m[13] * mat->m[6] * mat->m[11] - mat->m[13] * mat->m[7] * mat->m[10];

	float m4 = -mat->m[4] * mat->m[10] * mat->m[15] + mat->m[4] * mat->m[11] * mat->m[14] +
				mat->m[8] * mat->m[6] * mat->m[15] - mat->m[8] * mat->m[7] * mat->m[14] -
				mat->m[12] * mat->m[6] * mat->m[11] + mat->m[12] * mat->m[7] * mat->m[10];

	float m8 =  mat->m[4] * mat->m[9] * mat->m[15] - mat->m[4] * mat->m[11] * mat->m[13] -
				mat->m[8] * mat->m[5] * mat->m[15] + mat->m[8] * mat->m[7] * mat->m[13] +
				mat->m[12] * mat->m[5] * mat->m[11] - mat->m[12] * mat->m[7] * mat->m[9];

	float m12 = -mat->m[4] * mat->m[9] * mat->m[14] + mat->m[4] * mat->m[10] * mat->m[13] +
				mat->m[8] * mat->m[5] * mat->m[14] - mat->m[8] * mat->m[6] * mat->m[13] -
				mat->m[12] * mat->m[5] * mat->m[10] + mat->m[12] * mat->m[6] * mat->m[9];

	float det = mat->m[0] * m0 + mat->m[1] * m4 + mat->m[2] * m8 + mat->m[3] * m12;
	return det;
}

M4f M4fInverse(M4f const *mat) {
	M4f result;

	result.m[0] =	mat->m[5] * mat->m[10] * mat->m[15] - mat->m[5] * mat->m[11] * mat->m[14] -
					mat->m[9] * mat->m[6] * mat->m[15] + mat->m[9] * mat->m[7] * mat->m[14] +
					mat->m[13] * mat->m[6] * mat->m[11] - mat->m[13] * mat->m[7] * mat->m[10];

	result.m[4] =	-mat->m[4] * mat->m[10] * mat->m[15] + mat->m[4] * mat->m[11] * mat->m[14] +
					mat->m[8] * mat->m[6] * mat->m[15] - mat->m[8] * mat->m[7] * mat->m[14] -
					mat->m[12] * mat->m[6] * mat->m[11] + mat->m[12] * mat->m[7] * mat->m[10];

	result.m[8] =	mat->m[4] * mat->m[9] * mat->m[15] - mat->m[4] * mat->m[11] * mat->m[13] -
					mat->m[8] * mat->m[5] * mat->m[15] + mat->m[8] * mat->m[7] * mat->m[13] +
					mat->m[12] * mat->m[5] * mat->m[11] - mat->m[12] * mat->m[7] * mat->m[9];

	result.m[12] =	-mat->m[4] * mat->m[9] * mat->m[14] + mat->m[4] * mat->m[10] * mat->m[13] +
					mat->m[8] * mat->m[5] * mat->m[14] - mat->m[8] * mat->m[6] * mat->m[13] -
					mat->m[12] * mat->m[5] * mat->m[10] + mat->m[12] * mat->m[6] * mat->m[9];

	result.m[1] =	-mat->m[1] * mat->m[10] * mat->m[15] + mat->m[1] * mat->m[11] * mat->m[14] +
					mat->m[9] * mat->m[2] * mat->m[15] - mat->m[9] * mat->m[3] * mat->m[14] -
					mat->m[13] * mat->m[2] * mat->m[11] + mat->m[13] * mat->m[3] * mat->m[10];

	result.m[5] =	mat->m[0] * mat->m[10] * mat->m[15] - mat->m[0] * mat->m[11] * mat->m[14] -
					mat->m[8] * mat->m[2] * mat->m[15] + mat->m[8] * mat->m[3] * mat->m[14] +
					mat->m[12] * mat->m[2] * mat->m[11] - mat->m[12] * mat->m[3] * mat->m[10];

	result.m[9] =	-mat->m[0] * mat->m[9] * mat->m[15] + mat->m[0] * mat->m[11] * mat->m[13] +
					mat->m[8] * mat->m[1] * mat->m[15] - mat->m[8] * mat->m[3] * mat->m[13] -
					mat->m[12] * mat->m[1] * mat->m[11] + mat->m[12] * mat->m[3] * mat->m[9];

	result.m[13] =	mat->m[0] * mat->m[9] * mat->m[14] - mat->m[0] * mat->m[10] * mat->m[13] -
					mat->m[8] * mat->m[1] * mat->m[14] + mat->m[8] * mat->m[2] * mat->m[13] +
					mat->m[12] * mat->m[1] * mat->m[10] - mat->m[12] * mat->m[2] * mat->m[9];

	result.m[2] =	mat->m[1] * mat->m[6] * mat->m[15] - mat->m[1] * mat->m[7] * mat->m[14] -
					mat->m[5] * mat->m[2] * mat->m[15] + mat->m[5] * mat->m[3] * mat->m[14] +
					mat->m[13] * mat->m[2] * mat->m[7] - mat->m[13] * mat->m[3] * mat->m[6];

	result.m[6] =	-mat->m[0] * mat->m[6] * mat->m[15] + mat->m[0] * mat->m[7] * mat->m[14] +
					mat->m[4] * mat->m[2] * mat->m[15] - mat->m[4] * mat->m[3] * mat->m[14] -
					mat->m[12] * mat->m[2] * mat->m[7] + mat->m[12] * mat->m[3] * mat->m[6];

	result.m[10] =	mat->m[0] * mat->m[5] * mat->m[15] - mat->m[0] * mat->m[7] * mat->m[13] -
					mat->m[4] * mat->m[1] * mat->m[15] + mat->m[4] * mat->m[3] * mat->m[13] +
					mat->m[12] * mat->m[1] * mat->m[7] - mat->m[12] * mat->m[3] * mat->m[5];

	result.m[14] =	-mat->m[0] * mat->m[5] * mat->m[14] + mat->m[0] * mat->m[6] * mat->m[13] +
					mat->m[4] * mat->m[1] * mat->m[14] - mat->m[4] * mat->m[2] * mat->m[13] -
					mat->m[12] * mat->m[1] * mat->m[6] + mat->m[12] * mat->m[2] * mat->m[5];

	result.m[3] =	-mat->m[1] * mat->m[6] * mat->m[11] + mat->m[1] * mat->m[7] * mat->m[10] +
					mat->m[5] * mat->m[2] * mat->m[11] - mat->m[5] * mat->m[3] * mat->m[10] -
					mat->m[9] * mat->m[2] * mat->m[7] + mat->m[9] * mat->m[3] * mat->m[6];

	result.m[7] =	mat->m[0] * mat->m[6] * mat->m[11] - mat->m[0] * mat->m[7] * mat->m[10] -
					mat->m[4] * mat->m[2] * mat->m[11] + mat->m[4] * mat->m[3] * mat->m[10] +
					mat->m[8] * mat->m[2] * mat->m[7] - mat->m[8] * mat->m[3] * mat->m[6];

	result.m[11] =	-mat->m[0] * mat->m[5] * mat->m[11] + mat->m[0] * mat->m[7] * mat->m[9] +
					mat->m[4] * mat->m[1] * mat->m[11] - mat->m[4] * mat->m[3] * mat->m[9] -
					mat->m[8] * mat->m[1] * mat->m[7] + mat->m[8] * mat->m[3] * mat->m[5];

	result.m[15] =	mat->m[0] * mat->m[5] * mat->m[10] - mat->m[0] * mat->m[6] * mat->m[9] -
					mat->m[4] * mat->m[1] * mat->m[10] + mat->m[4] * mat->m[2] * mat->m[9] +
					mat->m[8] * mat->m[1] * mat->m[6] - mat->m[8] * mat->m[2] * mat->m[5];

	float det = mat->m[0] * result.m[0] + mat->m[1] * result.m[4] +
				mat->m[2] * result.m[8] + mat->m[3] * result.m[12];

	det = 1.0f / det;
	for (int i = 0; i < 4; i++)
		result.rows[i] = V4fMulF(result.rows[i], det);
	return result;
}

M4f M4fTranspose(M4f const *m) {
	M4f res;
	res.rows[0] = (V4f){ m->m2[0][0], m->m2[1][0], m->m2[2][0], m->m2[3][0] };
	res.rows[1] = (V4f){ m->m2[0][1], m->m2[1][1], m->m2[2][1], m->m2[3][1] };
	res.rows[2] = (V4f){ m->m2[0][2], m->m2[1][2], m->m2[2][2], m->m2[3][2] };
	res.rows[3] = (V4f){ m->m2[0][3], m->m2[1][3], m->m2[2][3], m->m2[3][3] };
	return res;
}

M2f M2fMul(M2f const *left, M2f const *right) {
	M2f res;
	M2f tras  = M2fTranspose(right);
	res.m2[0][0] = V2fDot(left->rows[0], tras.rows[0]);
	res.m2[0][1] = V2fDot(left->rows[0], tras.rows[1]);
	res.m2[1][0] = V2fDot(left->rows[1], tras.rows[0]);
	res.m2[1][1] = V2fDot(left->rows[1], tras.rows[1]);
	return res;
}

M3f M3fMul(M3f const *left, M3f const *right) {
	M3f res;
	M3f tras  = M3fTranspose(right);
	res.m2[0][0] = V3fDot(left->rows[0], tras.rows[0]);
	res.m2[0][1] = V3fDot(left->rows[0], tras.rows[1]);
	res.m2[0][2] = V3fDot(left->rows[0], tras.rows[2]);
	res.m2[1][0] = V3fDot(left->rows[1], tras.rows[0]);
	res.m2[1][1] = V3fDot(left->rows[1], tras.rows[1]);
	res.m2[1][2] = V3fDot(left->rows[1], tras.rows[2]);
	res.m2[2][0] = V3fDot(left->rows[2], tras.rows[0]);
	res.m2[2][1] = V3fDot(left->rows[2], tras.rows[1]);
	res.m2[2][2] = V3fDot(left->rows[2], tras.rows[2]);
	return res;
}

M4f M4fMul(M4f const *left, M4f const *right) {
	M4f res;
	M4f tras  = M4fTranspose(right);
	res.m2[0][0] = V4fDot(left->rows[0], tras.rows[0]);
	res.m2[0][1] = V4fDot(left->rows[0], tras.rows[1]);
	res.m2[0][2] = V4fDot(left->rows[0], tras.rows[2]);
	res.m2[0][3] = V4fDot(left->rows[0], tras.rows[3]);
	res.m2[1][0] = V4fDot(left->rows[1], tras.rows[0]);
	res.m2[1][1] = V4fDot(left->rows[1], tras.rows[1]);
	res.m2[1][2] = V4fDot(left->rows[1], tras.rows[2]);
	res.m2[1][3] = V4fDot(left->rows[1], tras.rows[3]);
	res.m2[2][0] = V4fDot(left->rows[2], tras.rows[0]);
	res.m2[2][1] = V4fDot(left->rows[2], tras.rows[1]);
	res.m2[2][2] = V4fDot(left->rows[2], tras.rows[2]);
	res.m2[2][3] = V4fDot(left->rows[2], tras.rows[3]);
	res.m2[3][0] = V4fDot(left->rows[3], tras.rows[0]);
	res.m2[3][1] = V4fDot(left->rows[3], tras.rows[1]);
	res.m2[3][2] = V4fDot(left->rows[3], tras.rows[2]);
	res.m2[3][3] = V4fDot(left->rows[3], tras.rows[3]);
	return res;
}

V2f V2fM2fMul(V2f vec, M2f const *mat) {
	V2f res;
	res.m[0] = V2fDot(vec, (V2f){ mat->m2[0][0], mat->m2[1][0] });
	res.m[1] = V2fDot(vec, (V2f){ mat->m2[0][1], mat->m2[1][1] });
	return res;
}

V2f M2fV2fMul(M2f const *mat, V2f vec) {
	V2f res;
	res.m[0] = V2fDot(vec, mat->rows[0]);
	res.m[1] = V2fDot(vec, mat->rows[1]);
	return res;
}

V3f V3fM3fMul(V3f vec, M3f const *mat) {
	V3f res;
	res.m[0] = V3fDot(vec, (V3f){ mat->m2[0][0], mat->m2[1][0], mat->m2[2][0] });
	res.m[1] = V3fDot(vec, (V3f){ mat->m2[0][1], mat->m2[1][1], mat->m2[2][1] });
	res.m[2] = V3fDot(vec, (V3f){ mat->m2[0][2], mat->m2[1][2], mat->m2[2][2] });
	return res;
}

V3f M3fV3fMul(M3f const *mat, V3f vec) {
	V3f res;
	res.m[0] = V3fDot(vec, mat->rows[0]);
	res.m[1] = V3fDot(vec, mat->rows[1]);
	res.m[2] = V3fDot(vec, mat->rows[2]);
	return res;
}

V4f V4fM4fMul(V4f vec, M4f const *mat) {
	V4f res;
	res.m[0] = V4fDot(vec, (V4f){ mat->m2[0][0], mat->m2[1][0], mat->m2[2][0], mat->m2[3][0] });
	res.m[1] = V4fDot(vec, (V4f){ mat->m2[0][1], mat->m2[1][1], mat->m2[2][1], mat->m2[3][1] });
	res.m[2] = V4fDot(vec, (V4f){ mat->m2[0][2], mat->m2[1][2], mat->m2[2][2], mat->m2[3][2] });
	res.m[2] = V4fDot(vec, (V4f){ mat->m2[0][3], mat->m2[1][3], mat->m2[2][3], mat->m2[3][3] });
	return res;
}

V4f M4fV4fMul(M4f const *mat, V4f vec) {
	V4f res;
	res.m[0] = V4fDot(vec, mat->rows[0]);
	res.m[1] = V4fDot(vec, mat->rows[1]);
	res.m[2] = V4fDot(vec, mat->rows[2]);
	res.m[3] = V4fDot(vec, mat->rows[3]);
	return res;
}

void M2fTranform(M2f *mat, M2f const *t) {
	*mat = M2fMul(mat, t);
}

void M3fTranform(M3f *mat, M3f const *t) {
	*mat = M3fMul(mat, t);
}

void M4fTranform(M4f *mat, M4f const *t) {
	*mat = M4fMul(mat, t);
}

Quat QuatNeg(Quat q) {
	return (Quat) {
		.v = V4fNeg(q.v)
	};
}

Quat QuatAdd(Quat r1, Quat r2) {
	return (Quat) {
		.v = V4fAdd(r1.v, r2.v)
	};
}

Quat QuatSub(Quat r1, Quat r2) {
	return (Quat) {
		.v = V4fSub(r1.v, r2.v)
	};
}

Quat QuatMulF(Quat q, float s) {
	return (Quat) {
		.v = V4fMulF(q.v, s)
	};
}

Quat QuatDivF(Quat q, float s) {
	return (Quat) {
		.v = V4fDivF(q.v, s)
	};
}

void QuatAddScaled(Quat *q, float s, Quat o) {
	V4fAddScaled(&q->v, s, o.v);
}

void QuatSubScaled(Quat *q, float s, Quat o) {
	V4fSubScaled(&q->v, s, o.v);
}

float QuatDot(Quat a, Quat b) {
	return V4fDot(a.v, b.v);
}

float QuatLengthSq(Quat a) {
	return V4fLengthSq(a.v);
}

float QuatLength(Quat a) {
	return V4fLength(a.v);
}

Quat QuatNormalizeOrIdentity(Quat q) {
	float len = QuatLength(q);
	if (len != 0) {
		float ilen = 1.0f / len;
		return QuatMulF(q, ilen);
	}

	return (Quat) { 0, 0, 0, 1 };
}

Quat QuatNormalize(Quat q) {
	return (Quat) {
		.v = V4fNormalize(q.v)
	};
}

Quat QuatConjugate(Quat q) {
	q.x = -q.x;
	q.y = -q.y;
	q.z = -q.z;
	return q;
}

Quat QuatMul(Quat q1, Quat q2) {
	float a = q1.w;
	float b = q1.x;
	float c = q1.y;
	float d = q1.z;

	float e = q2.w;
	float f = q2.x;
	float g = q2.y;
	float h = q2.z;

	Quat res;
	res.w = a * e - b * f - c * g - d * h;
	res.x = a * f + b * e + c * h - d * g;
	res.y = a * g - b * h + c * e + d * f;
	res.z = a * h + b * g - c * f + d * e;
	return res;
}

V3f QuatMulV3f(Quat q, V3f v) {
	Quat p   = (Quat){ v.x, v.y, v.z, 0 };
	Quat qc  = QuatConjugate(q);
	Quat ta  = QuatMul(q, p);
	Quat res = QuatMul(ta, qc);
	return (V3f){ res.x, res.y, res.z };
}

V3f M4fRight(M4f const *m) {
	V3f v;
	v.x = m->m2[0][0];
	v.y = m->m2[1][0];
	v.z = m->m2[2][0];
	return v;
}

V3f M4fUp(M4f const *m) {
	V3f v;
	v.x = m->m2[0][2];
	v.y = m->m2[1][2];
	v.z = m->m2[2][2];
	return v;
}

V3f M4fForward(M4f const *m) {
	V3f v;
	v.x = m->m2[0][1];
	v.y = m->m2[1][1];
	v.z = m->m2[2][1];
	return v;
}

V3f QuatRight(Quat q) {
	V3f  right;
	right.x = 1 - 2 * (q.y * q.y + q.z * q.z);
	right.y = 2 * (q.x * q.y + q.z * q.w);
	right.z = 2 * (q.x * q.z - q.y * q.w);
	return V3fNormalizeOrZero(right);
}

V3f QuatUp(Quat q) {
	V3f forward;
	forward.x = 2 * (q.x * q.y - q.z * q.w);
	forward.y = 1 - 2 * (q.x * q.x + q.z * q.z);
	forward.z = 2 * (q.y * q.z + q.x * q.w);
	return V3fNormalizeOrZero(forward);
}

V3f QuatForward(Quat q) {
	V3f up;
	up.x = 2 * (q.x * q.z + q.y * q.w);
	up.y = 2 * (q.y * q.z - q.x * q.w);
	up.z = 1 - 2 * (q.x * q.x + q.y * q.y);
	return V3fNormalizeOrZero(up);
}

M2f M3fToM2f(M3f const *mat) {
	M2f result;
	result.rows[0] = (V2f){ mat->rows[0].x, mat->rows[0].y };
	result.rows[1] = (V2f){ mat->rows[1].x, mat->rows[1].y };
	return result;
}

M3f M2fToM3f(M2f const *mat) {
	M3f result;
	result.rows[0] = (V3f){ .xy = mat->rows[0], .z = 0 };
	result.rows[1] = (V3f){ .xy = mat->rows[1], .z = 0 };
	result.rows[2] = (V3f){ 0, 0, 1 };
	return result;
}

M3f M4fToM3f(M4f const *mat) {
	M3f result;
	result.rows[0] = (V3f){ mat->rows[0].x, mat->rows[0].y, mat->rows[0].z };
	result.rows[1] = (V3f){ mat->rows[1].x, mat->rows[1].y, mat->rows[1].z };
	result.rows[2] = (V3f){ mat->rows[2].x, mat->rows[2].y, mat->rows[2].z };
	return result;
}

M4f M3fToM4f(M3f const *mat) {
	M4f result;
	result.rows[0] = (V4f){ .xyz = mat->rows[0], .w = 0 };
	result.rows[1] = (V4f){ .xyz = mat->rows[1], .w = 0 };
	result.rows[2] = (V4f){ .xyz = mat->rows[2], .w = 0 };
	result.rows[3] = (V4f){ 0, 0, 0, 1 };
	return result;
}

V3f QuatToAxisTurns(Quat q, float *turns) {
	float len = QuatLength(q);
	if (len) {
		*turns = 2.0f * ArcTan2(len, q.w);
		len    = 1.0f / len;
		return V3fMulF(q.v.xyz, len);
	}

	// degenerate case, unit quaternion
	*turns = 0;
	return (V3f){ 0, 0, 1 };
}

M4f QuatToM4f(Quat q) {
	float i = q.x;
	float j = q.y;
	float k = q.z;
	float r = q.w;

	float ii = i * i;
	float jj = j * j;
	float kk = k * k;

	float ij = i * j;
	float jk = j * k;
	float kr = k * r;
	float jr = j * r;
	float ir = i * r;
	float ik = i * k;

	M4f m;

	m.m2[0][0] = 1 - 2 * (jj + kk);
	m.m2[0][1] = 2 * (ij - kr);
	m.m2[0][2] = 2 * (ik + jr);
	m.m2[0][3] = 0;

	m.m2[1][0] = 2 * (ij + kr);
	m.m2[1][1] = 1 - 2 * (ii + kk);
	m.m2[1][2] = 2 * (jk - ir);
	m.m2[1][3] = 0;

	m.m2[2][0] = 2 * (ik - jr);
	m.m2[2][1] = 2 * (jk + ir);
	m.m2[2][2] = 1 - 2 * (ii + jj);
	m.m2[2][3] = 0;

	m.m2[3][0] = 0;
	m.m2[3][1] = 0;
	m.m2[3][2] = 0;
	m.m2[3][3] = 1;

	return m;
}

V3f QuatToEuler(Quat q) {
	V3f euler;

	float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
	float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	euler.z         = ArcTan2(sinr_cosp, cosr_cosp);

	float sinp = 2.0f * (q.w * q.y - q.z * q.x);
	if (Abs(sinp) >= 1.0f) {
		// use 90 degrees if out of range
		euler.x = Sgn(sinp) / 4.0f;
	} else {
		euler.x = ArcSin(sinp);
	}

	float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
	float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	euler.y = ArcTan2(siny_cosp, cosy_cosp);

	return euler;
}

Quat M4fToQuat(M4f const *m) {
	Quat q;
	float trace = m->m2[0][0] + m->m2[1][1] + m->m2[2][2];
	if (trace > 0.0f) {
		float s = 0.5f / Sqrt(trace + 1.0f);
		q.w = 0.25f / s;
		q.x = (m->m2[2][1] - m->m2[1][2]) * s;
		q.y = (m->m2[0][2] - m->m2[2][0]) * s;
		q.z = (m->m2[1][0] - m->m2[0][1]) * s;
	} else {
		if (m->m2[0][0] > m->m2[1][1] && m->m2[0][0] > m->m2[2][2]) {
			float s = 2.0f * Sqrt(1.0f + m->m2[0][0] - m->m2[1][1] - m->m2[2][2]);
			q.w = (m->m2[2][1] - m->m2[1][2]) / s;
			q.x = 0.25f * s;
			q.y = (m->m2[0][1] + m->m2[1][0]) / s;
			q.z = (m->m2[0][2] + m->m2[2][0]) / s;
		} else if (m->m2[1][1] > m->m2[2][2]) {
			float s = 2.0f * Sqrt(1.0f + m->m2[1][1] - m->m2[0][0] - m->m2[2][2]);
			q.w = (m->m2[0][2] - m->m2[2][0]) / s;
			q.x = (m->m2[0][1] + m->m2[1][0]) / s;
			q.y = 0.25f * s;
			q.z = (m->m2[1][2] + m->m2[2][1]) / s;
		} else {
			float s = 2.0f * Sqrt(1.0f + m->m2[2][2] - m->m2[0][0] - m->m2[1][1]);
			q.w = (m->m2[1][0] - m->m2[0][1]) / s;
			q.x = (m->m2[0][2] + m->m2[2][0]) / s;
			q.y = (m->m2[1][2] + m->m2[2][1]) / s;
			q.z = 0.25f * s;
		}
	}
	return QuatNormalizeOrIdentity(q);
}

M2f M2fIdentity() { 
	M2f m = {
		.rows[0] = { 1, 0 },
		.rows[1] = { 0, 1 },
	};
	return m;
}

M2f M2fDiagonal(float x, float y) {
	M2f m = {
		.rows[0] = { x, 0 },
		.rows[1] = { 0, y },
	};
	return m;
}

M3f M3fIdentity() {
	M3f m = {
		.rows[0] = { 1, 0, 0 },
		.rows[1] = { 0, 1, 0 },
		.rows[2] = { 0, 0, 1 },
	};
	return m;
}

M3f M3fDiagonal(float x, float y, float z) {
	M3f m = {
		.rows[0] = { x, 0, 0 },
		.rows[1] = { 0, y, 0 },
		.rows[2] = { 0, 0, z },
	};
	return m;
}

M4f M4fIdentity() {
	M4f m = {
		.rows[0] = { 1, 0, 0, 0 },
		.rows[1] = { 0, 1, 0, 0 },
		.rows[2] = { 0, 0, 1, 0 },
		.rows[3] = { 0, 0, 0, 1 },
	};
	return m;
}

M4f M4fDiagonal(float x, float y, float z, float w) {
	M4f m = {
		.rows[0] = { x, 0, 0, 0 },
		.rows[1] = { 0, y, 0, 0 },
		.rows[2] = { 0, 0, z, 0 },
		.rows[2] = { 0, 0, 0, w },
	};
	return m;
}

M2f M2fRotate(V2f arm) {
	float c = arm.x;
	float s = arm.y;
	M2f mat = {
		.rows[0] = { c, -s },
		.rows[1] = { s,  c },
	};
	return mat;
}

M2f M2fRotateF(float turns) {
	return M2fRotate(Arm(turns));
}

M3f M3fScaleF(float x, float y) {
	return M3fDiagonal(x, y, 1);
}

M3f M3fScale(V2f s) {
	return M3fDiagonal(s.x, s.y, 1);
}

M3f M3fTranslateF(float x, float y) {
	M3f m = {
		.rows[0] = { 1, 0, x },
		.rows[1] = { 0, 1, y },
		.rows[2] = { 0, 0, 0 },
	};
	return m;
}

M3f M3fTranslate(V2f t) {
	return M3fTranslateF(t.x, t.y);
}

M3f M3fRotate(V2f arm) {
	float c = arm.x;
	float s = arm.y;

	M3f  m = {
		.rows[0] = { c, -s, 0 },
		.rows[1] = { s,  c, 0 },
		.rows[2] = { 0, 0,  1 },
	};
	return m;
}

M3f M3fRotateF(float turns) {
	return M3fRotate(Arm(turns));
}

M3f M3fLookAt(V2f from, V2f to, V2f forward) {
	V2f direction = V2fNormalize(V2fSub(to, from));
	float  cos_theta = V2fDot(direction, forward);
	float  sin_theta = Sqrt(1.0f - cos_theta * cos_theta);

	M3f  m = {
		.rows[0] = { cos_theta, -sin_theta, from.x },
		.rows[1] = { sin_theta, cos_theta, from.y },
		.rows[2] = { 0.0f, 0.0f, 1.0f },
	};
	return m;
}

M4f M4fScaleF(float x, float y, float z) {
	return M4fDiagonal(x, y, z, 1);
}

M4f M4fScale(V3f s) {
	return M4fDiagonal(s.x, s.y, s.z, 1);
}

M4f M4fTranslateF(float x, float y, float z) {
	M4f m = {
		.rows[0] = { 1, 0, 0, x },
		.rows[1] = { 0, 1, 0, y },
		.rows[2] = { 0, 0, 1, z },
		.rows[3] = { 0, 0, 0, 1 },
	};
	return m;
}

M4f M4fTranslate(V3f t) {
	return M4fTranslateF(t.x, t.y, t.z);
}

M4f M4fRotateX(V2f arm) {
	float c = arm.x;
	float s = arm.y;

	M4f m = {
		.rows[0] = { 1, 0,  0, 0 },
		.rows[1] = { 0, c, -s, 0 },
		.rows[2] = { 0, s,  c, 0 },
		.rows[3] = { 0, 0,  0, 1 },
	};
	return m;
}

M4f M4fRotateXF(float turns) {
	V2f arm = Arm(turns);
	return M4fRotateX(arm);
}

M4f M4RotateY(V2f arm) {
	float c = arm.x;
	float s = arm.y;

	M4f m = {
		.rows[0] = {  c, 0, s, 0 },
		.rows[1] = {  0, 1, 0, 0 },
		.rows[2] = { -s, 0, c, 0 },
		.rows[3] = {  0, 0, 0, 1 },
	};
	return m;
}

M4f M4fRotateYF(float turns) {
	V2f arm = Arm(turns);
	return M4RotateY(arm);
}

M4f M4fRotateZ(V2f arm) {
	float c = arm.x;
	float s = arm.y;

	M4f m = {
		.rows[0] = { c, -s, 0, 0 },
		.rows[1] = { s,  c, 0, 0 },
		.rows[2] = { 0,  0, 1, 0 },
		.rows[3] = { 0,  0, 0, 1 },
	};
	return m;
}

M4f M4fRotateZF(float turns) {
	V2f arm = Arm(turns);
	return M4fRotateZ(arm);
}

M4f M4fRotate(V3f axis, V2f arm) {
	float c = arm.x;
	float s = arm.y;

	float x = axis.x;
	float y = axis.y;
	float z = axis.z;

	float x2 = x * x;
	float xy = x * y;
	float zx = x * z;
	float y2 = y * y;
	float yz = y * z;
	float z2 = z * z;

	M4f  m = {
		.rows[0] = { c + x2 * (1 - c), xy * (1 - c) - z * s, zx * (1 - c) + y * s, 0 },
		.rows[1] = { xy * (1 - c) + z * s, c + y2 * (1 - c), yz * (1 - c) - x * s, 0 },
		.rows[2] = { zx * (1 - c) - y * s, yz * (1 - c) + x * s, c + z2 * (1 - c), 0 },
		.rows[3] = { 0, 0, 0, 1 },
	};
	return m;
}

M4f M4fFrotate(float x, float y, float z, float turns) {
	return M4fRotate((V3f){ x, y, z }, Arm(turns));
}

M4f M4fLookAt(V3f from, V3f to, V3f world_up) {
	V3f forward = V3fNormalize(V3fSub(from, to));
	V3f right   = V3fNormalize(V3fCross(world_up, forward));
	V3f up      = V3fCross(right, forward);

	M4f lookat = {
		.rows[0] = { right.x, up.x, forward.x, -from.x },
		.rows[1] = { right.y, up.y, forward.y, -from.y },
		.rows[2] = { right.z, up.z, forward.z, -from.z },
		.rows[3] = { 0, 0, 0, 1 },
	};

	return lookat;
}

M4f M4fLookTowards(V3f dir, V3f world_up) {
	V3f forward = V3fNormalize(dir);
	V3f right   = V3fNormalize(V3fCross(world_up, forward));
	V3f up      = V3fCross(right, forward);

	M4f lookat = {
		.rows[0] = { right.x, up.x, forward.x, 0 },
		.rows[1] = { right.y, up.y, forward.y, 0 },
		.rows[2] = { right.z, up.z, forward.z, 0 },
		.rows[3] = { 0, 0, 0, 1 },
	};

	return lookat;
}

M4f M4fOrthographic(float l, float r, float t, float b, float n, float f) {
	float iwidth  = 1 / (r - l);
	float iheight = 1 / (t - b);
	float range   = 1 / (f - n);

	M4f m = {
		.rows[0] = { 2 * iwidth, 0.0f, 0.0f, -(l + r) * iwidth },
		.rows[1] = { 0.0f, 2 * iheight, 0.0f, -(t + b) * iheight },
		.rows[2] = { 0.0f, 0.0f, range, -n * range },
		.rows[3] = { 0.0f, 0.0f, 0.0f, 1.0f },
	};
	return m;
}

M4f M4fPerspective(float fov, float aspect_ratio, float n, float f) {
	float height = 1.0f / Tan(fov * 0.5f);
	float width  = height / aspect_ratio;
	float range  = 1 / (f - n);

	M4f  m = {
		.rows[0] = { width, 0.0f, 0.0f, 0.0f },
		.rows[1] = { 0.0f, height, 0.0f, 0.0f },
		.rows[2] = { 0.0f, 0.0f, f * range, -1.0f * f * n * range },
		.rows[3] = { 0.0f, 0.0f, 1.0f, 0.0f },
	};
	return m;
}

Quat QuatIdentity() {
	return (Quat){ 0, 0, 0, 1 };
}

Quat QuatAxisTurns(V3f axis, float turns) {
	float r = Cos(turns * 0.5f);
	float s = Sin(turns * 0.5f);
	float i = s * axis.x;
	float j = s * axis.y;
	float k = s * axis.z;
	return (Quat){ i, j, k, r };
}

Quat QuatEuler(float pitch, float yaw, float roll) {
	float cy = Cos(roll * 0.5f);
	float sy = Sin(roll * 0.5f);
	float cp = Cos(yaw * 0.5f);
	float sp = Sin(yaw * 0.5f);
	float cr = Cos(pitch * 0.5f);
	float sr = Sin(pitch * 0.5f);

	Quat  q;
	q.w = cy * cp * cr + sy * sp * sr;
	q.x = cy * cp * sr - sy * sp * cr;
	q.y = sy * cp * sr + cy * sp * cr;
	q.z = sy * cp * cr - cy * sp * sr;
	return q;
}

Quat QuatBetweenV3f(V3f from, V3f to) {
	Quat q;
	q.v.w = 1.0f + V3fDot(from, to);

	if (q.v.w) {
		q.v.xyz = V3fCross(from, to);
		return q;
	} else {
		if (Abs(from.x) > Abs(from.z)) {
			q.v.xyz = (V3f){ .x = -from.y, .y = from.x, .z = 0.0f };
		} else {
			q.v.xyz = (V3f){ .x = 0.0f, .y = -from.z, .z = from.y };
		}
	}

	return QuatNormalizeOrIdentity(q);
}

Quat QuatBetween(Quat a, Quat b) {
	Quat t = QuatConjugate(a);
	float   d = QuatDot(t, t);
	if (d != 0.0f) {
		t = QuatMulF(t, 1.0f / d);
		return QuatMul(t, b);
	}
	return QuatIdentity();
}

Quat QuatLookAt(V3f from, V3f to, V3f world_forward) {
	V3f dir = V3fSub(to, from);
	return QuatBetweenV3f(world_forward, dir);
}

float Lerp(float from, float to, float t) {
	return (1 - t) * from + t * to;
}

V2f V2fLerp(V2f from, V2f to, float t) {
	V2f res;
	res.x = Lerp(from.x, to.x, t);
	res.y = Lerp(from.y, to.y, t);
	return res;
}

V3f V3fLerp(V3f from, V3f to, float t) {
	V3f res;
	res.x = Lerp(from.x, to.x, t);
	res.y = Lerp(from.y, to.y, t);
	res.z = Lerp(from.z, to.z, t);
	return res;
}

V4f V4fLerp(V4f from, V4f to, float t) {
	V4f res;
	res.x = Lerp(from.x, to.x, t);
	res.y = Lerp(from.y, to.y, t);
	res.z = Lerp(from.z, to.z, t);
	res.w = Lerp(from.w, to.w, t);
	return res;
}

Quat QuatLerp(Quat from, Quat to, float t) {
	return (Quat) {
		.v = V4fLerp(from.v, to.v,t)
	};
}

float BezierQuadratic(float a, float b, float c, float t) {
	float mt = 1 - t;
	float w1 = mt * mt;
	float w2 = 2 * mt * t;
	float w3 = t * t;
	return w1 * a + w2 * b + w3 * c;
}

V2f V2fBezierQuadratic(V2f a, V2f b, V2f c, float t) {
	V2f res;
	res.x = BezierQuadratic(a.x, b.x, c.x, t);
	res.y = BezierQuadratic(a.y, b.y, c.y, t);
	return res;
}

V3f V3fBezierQuadratic(V3f a, V3f b, V3f c, float t) {
	V3f res;
	res.x = BezierQuadratic(a.x, b.x, c.x, t);
	res.y = BezierQuadratic(a.y, b.y, c.y, t);
	res.z = BezierQuadratic(a.z, b.z, c.z, t);
	return res;
}

V4f V4fBezierQuadratic(V4f a, V4f b, V4f c, float t) {
	V4f res;
	res.x = BezierQuadratic(a.x, b.x, c.x, t);
	res.y = BezierQuadratic(a.y, b.y, c.y, t);
	res.z = BezierQuadratic(a.z, b.z, c.z, t);
	res.w = BezierQuadratic(a.w, b.w, c.w, t);
	return res;
}

Quat QuatBezierQuadratic(Quat a, Quat b, Quat c, float t) {
	return (Quat) {
		.v = V4fBezierQuadratic(a.v, b.v, c.v, t)
	};
}

float BezierCubic(float a, float b, float c, float d, float t) {
	float mt = 1.0f - t;
	float w1 = mt * mt * mt;
	float w2 = 3 * mt * mt * t;
	float w3 = 3 * mt * t * t;
	float w4 = t * t * t;
	return w1 * a + w2 * b + w3 * c + w4 * d;
}

V2f V2fBezierCubic(V2f a, V2f b, V2f c, V2f d, float t) {
	V2f res;
	res.x = BezierCubic(a.x, b.x, c.x, d.x, t);
	res.y = BezierCubic(a.y, b.y, c.y, d.y, t);
	return res;
}

V3f V3fBezierCubic(V3f a, V3f b, V3f c, V3f d, float t) {
	V3f res;
	res.x = BezierCubic(a.x, b.x, c.x, d.x, t);
	res.y = BezierCubic(a.y, b.y, c.y, d.y, t);
	res.z = BezierCubic(a.z, b.z, c.z, d.z, t);
	return res;
}

V4f V4fBezierCubic(V4f a, V4f b, V4f c, V4f d, float t) {
	V4f res;
	res.x = BezierCubic(a.x, b.x, c.x, d.x, t);
	res.y = BezierCubic(a.y, b.y, c.y, d.y, t);
	res.z = BezierCubic(a.z, b.z, c.z, d.z, t);
	res.w = BezierCubic(a.w, b.w, c.w, d.w, t);
	return res;
}

Quat QuatBezierCubic(Quat a, Quat b, Quat c, Quat d, float t) {
	return (Quat) {
		.v = V4fBezierCubic(a.v, b.v, c.v, d.v, t)
	};
}

void BuildBezierQuadratic(float a, float b, float c, float *points, int segments) {
	for (int seg_index = 0; seg_index <= segments; ++seg_index) {
		float t  = (float)seg_index / (float)segments;
		float np = BezierQuadratic(a, b, c, t);
		points[seg_index] = np;
	}
}

void V2fBuildBezierQuadratic(V2f a, V2f b, V2f c, V2f *points, int segments) {
	for (int seg_index = 0; seg_index <= segments; ++seg_index) {
		float t   = (float)seg_index / (float)segments;
		V2f np = V2fBezierQuadratic(a, b, c, t);
		points[seg_index] = np;
	}
}

void V3fBuildBezierQuadratic(V3f a, V3f b, V3f c, V3f *points, int segments) {
	for (int seg_index = 0; seg_index <= segments; ++seg_index) {
		float t   = (float)seg_index / (float)segments;
		V3f np = V3fBezierQuadratic(a, b, c, t);
		points[seg_index] = np;
	}
}

void V4fBuildBezierQuadratic(V4f a, V4f b, V4f c, V4f *points, int segments) {
	for (int seg_index = 0; seg_index <= segments; ++seg_index) {
		float t   = (float)seg_index / (float)segments;
		V4f np = V4fBezierQuadratic(a, b, c, t);
		points[seg_index] = np;
	}
}

void QuatBuildBezierQuadratic(Quat a, Quat b, Quat c, Quat *points, int segments) {
	for (int seg_index = 0; seg_index <= segments; ++seg_index) {
		float t    = (float)seg_index / (float)segments;
		Quat np = QuatBezierQuadratic(a, b, c, t);
		points[seg_index] = np;
	}
}

void BuildBezierCubic(float a, float b, float c, float d, float *points, int segments) {
	for (int seg_index = 0; seg_index <= segments; ++seg_index) {
		float t  = (float)seg_index / (float)segments;
		float np = BezierCubic(a, b, c, d, t);
		points[seg_index] = np;
	}
}

void V2fBuildBezierCubic(V2f a, V2f b, V2f c, V2f d, V2f *points, int segments) {
	for (int seg_index = 0; seg_index <= segments; ++seg_index) {
		float t   = (float)seg_index / (float)segments;
		V2f np = V2fBezierCubic(a, b, c, d, t);
		points[seg_index] = np;
	}
}

void V3fBuildBezierCubic(V3f a, V3f b, V3f c, V3f d, V3f *points, int segments) {
	for (int seg_index = 0; seg_index <= segments; ++seg_index) {
		float t   = (float)seg_index / (float)segments;
		V3f np = V3fBezierCubic(a, b, c, d, t);
		points[seg_index] = np;
	}
}

void V4fBuildBezierCubic(V4f a, V4f b, V4f c, V4f d, V4f *points, int segments) {
	for (int seg_index = 0; seg_index <= segments; ++seg_index) {
		float t   = (float)seg_index / (float)segments;
		V4f np = V4fBezierCubic(a, b, c, d, t);
		points[seg_index] = np;
	}
}

void QuatBuildBezierCubic(Quat a, Quat b, Quat c, Quat d, Quat *points, int segments) {
	for (int seg_index = 0; seg_index <= segments; ++seg_index) {
		float t    = (float)seg_index / (float)segments;
		Quat np = QuatBezierCubic(a, b, c, d, t);
		points[seg_index] = np;
	}
}

float Slerp(float from, float to, float turns, float t) {
	float s   = Sin(turns);
	float ts  = Sin(turns * t);
	float mts = Sin(turns * (1 - t));
	return (mts * from + ts * to) * (1.0f / s);
}

V2f V2fSlerp(V2f from, V2f to, float t) {
	float turns = V2fTurns(from, to);

	V2f res;
	res.x = Slerp(from.x, to.x, turns, t);
	res.y = Slerp(from.y, to.y, turns, t);
	return res;
}

V3f V3fSlerp(V3f from, V3f to, float t) {
	float dot   = V3fDot(from, to);
	dot         = Clamp(-1, 1, dot);
	float turns = ArcCos(dot);

	V3f res;
	res.x = Slerp(from.x, to.x, turns, t);
	res.y = Slerp(from.y, to.y, turns, t);
	res.z = Slerp(from.z, to.z, turns, t);
	return res;
}

Quat QuatSlerp(Quat from, Quat to, float t) {
	float dot = QuatDot(from, to);
	dot       = Clamp(-1, 1, dot);

	// use shorter path
	if (dot < 0.0f) {
		to  = QuatNeg(to);
		dot = -dot;
	}

	if (dot > 0.9999f) {
		Quat result = QuatLerp(from, to, t);
		return QuatNormalizeOrIdentity(result);
	}

	float theta_0     = ArcCos(dot);
	float theta       = theta_0 * t;
	float sin_theta   = Sin(theta);
	float sin_theta_0 = Sin(theta_0);

	float s0 = Cos(theta) - dot * sin_theta / sin_theta_0;
	float s1 = sin_theta / sin_theta_0;

	from = QuatMulF(from, s0);
	to   = QuatMulF(to, s1);

	return QuatAdd(from, to);
}

float Step(float edge, float val) {
	return val < edge ? 0.0f : 1.0f;
}

V2f V2fStep(V2f edge, V2f val) {
	V2f res;
	res.x = Step(edge.x, val.x);
	res.y = Step(edge.y, val.y);
	return res;
}

V3f V3fStep(V3f edge, V3f val) {
	V3f res;
	res.x = Step(edge.x, val.x);
	res.y = Step(edge.y, val.y);
	res.z = Step(edge.z, val.z);
	return res;
}

V4f V4fStep(V4f edge, V4f val) {
	V4f res;
	res.x = Step(edge.x, val.x);
	res.y = Step(edge.y, val.y);
	res.z = Step(edge.z, val.z);
	res.w = Step(edge.w, val.w);
	return res;
}

Quat QuatStep(Quat edge, Quat val) {
	return (Quat) {
		.v = V4fStep(edge.v, val.v)
	};
}

float SmoothStep(float a, float b, float v) {
	float div_distance = b - a;
	if (div_distance) {
		float t = (v - a) / div_distance;
		float x = Clamp(0.0f, 1.0f, t);
		return x * x * (3 - 2 * x);
	}
	return 1;
}

float V2fSmoothStep(V2f a, V2f b, V2f v) {
	float div_distance = V2fDistance(a, b);
	if (div_distance) {
		float t = V2fDistance(a, v) / div_distance;
		float x = Clamp(0.0f, 1.0f, t);
		return x * x * (3 - 2 * x);
	}
	return 1;
}

float V3fSmoothStep(V3f a, V3f b, V3f v) {
	float div_distance = V3fDistance(a, b);
	if (div_distance) {
		float t = V3fDistance(a, v) / div_distance;
		float x = Clamp(0.0f, 1.0f, t);
		return x * x * (3 - 2 * x);
	}
	return 1;
}

float V4fSmoothStep(V4f a, V4f b, V4f v) {
	float div_distance = V4fDistance(a, b);
	if (div_distance) {
		float t = V4fDistance(a, v) / div_distance;
		float x = Clamp(0.0f, 1.0f, t);
		return x * x * (3 - 2 * x);
	}
	return 1;
}

float QuatSmoothStep(Quat a, Quat b, Quat v) {
	return V4fSmoothStep(a.v, b.v, v.v);
}

float InverseSmoothStep(float x) {
	return 0.5f - Sin(ArcSin(1.0f - 2.0f * x) / 3.0f);
}

float MapRange(float in_a, float in_b, float out_a, float out_b, float v) {
	return (out_b - out_a) / (in_b - in_a) * (v - in_a) + out_a;
}

float MoveTowards(float from, float to, float factor) {
	if (factor) {
		float direction = to - from;
		float distance  = Abs(direction);
		if (distance < factor) {
			return to;
		}
		float t = factor / distance;
		return Lerp(from, to, t);
	}
	return from;
}

V2f V2fMoveTowards(V2f from, V2f to, float factor) {
	if (factor) {
		V2f direction = V2fSub(to, from);
		float  distance  = V2fLength(direction);
		if (distance < factor) {
			return to;
		}
		float t = factor / distance;
		return V2fLerp(from, to, t);
	}
	return from;
}

V3f V3fMoveTowards(V3f from, V3f to, float factor) {
	if (factor) {
		V3f direction = V3fSub(to, from);
		float  distance  = V3fLength(direction);
		if (distance < factor) {
			return to;
		}
		float t = factor / distance;
		return V3fLerp(from, to, t);
	}
	return from;
}

V4f V4fMoveTowards(V4f from, V4f to, float factor) {
	if (factor) {
		V4f direction = V4fSub(to, from);
		float  distance  = V4fLength(direction);
		if (distance < factor) {
			return to;
		}
		float t = factor / distance;
		return V4fLerp(from, to, t);
	}
	return from;
}

V2f V2fRotateAround(V2f point, V2f center, float turns) {
	float c = Cos(turns);
	float s = Sin(turns);
	V2f res;
	V2f p = V2fSub(point, center);
	res.x = p.x * c - p.y * s;
	res.y = p.x * s + p.y * c;
	res = V2fAdd(res, center);
	return res;
}

Quat QuatRotateTowards(Quat from, Quat to, float max_turns) {
	if (max_turns) {
		float dot = QuatDot(from, to);
		dot       = Clamp(-1.0f, 1.0f, dot);

		// use shorter path
		if (dot < 0.0f) {
			to  = QuatNeg(to);
			dot = -dot;
		}

		float theta_0 = ArcCos(dot);

		if (theta_0 < max_turns) {
			return to;
		}

		float t = max_turns / theta_0;

		theta_0 = max_turns ;
		float theta = theta_0 * t;
		float sin_theta = Sin(theta);
		float sin_theta_0 = Sin(theta_0);

		float s0 = Cos(theta) - dot * sin_theta / sin_theta_0;
		float s1 = sin_theta / sin_theta_0;

		from = QuatMulF(from, s0);
		to   = QuatMulF(to, s1);

		return QuatAdd(from, to);
	}
	else {
		return from;
	}
}

V2f V2fReflect(V2f d, V2f n) {
	float c = V2fDot(V2fNormalizeOrZero(d), n);
	float s = Sqrt(10.f - Sqr(c));
	V2f r;
	r.x = d.x * c - d.y * s;
	r.y = d.x * s + d.y * c;
	return r;
}

void UnpackRGBA(u32 c, u8 channels[4]) {
	channels[0] = (c >> 24) & 0xff;
	channels[1] = (c >> 16) & 0xff;
	channels[2] = (c >> 8)  & 0xff;
	channels[3] = (c >> 0)  & 0xff;
}

u32 PackRGBA(u8 r, u8 g, u8 b, u8 a) {
	return ((u32)r << 24) | ((u32)g << 16) | ((u32)b << 8) | (u32)a;
}

u32 PackV4f(V4f v) {
	u8 r = (u8)(255.0f * v.x);
	u8 g = (u8)(255.0f * v.y);
	u8 b = (u8)(255.0f * v.z);
	u8 a = (u8)(255.0f * v.w);
	return PackRGBA(r, g, b, a);
}

u32 PackV3f(V3f v) {
	u8 r = (u8)(255.0f * v.x);
	u8 g = (u8)(255.0f * v.y);
	u8 b = (u8)(255.0f * v.z);
	return PackRGBA(r, g, b, 0xff);
}

V4f UnpackV4f(u32 c) {
	V4f res;
	res.x = (float)((c >> 24) & 0xff) / 255.0f;
	res.y = (float)((c >> 16) & 0xff) / 255.0f;
	res.z = (float)((c >> 8) & 0xff) / 255.0f;
	res.w = (float)((c >> 0) & 0xff) / 255.0f;
	return res;
}

V3f UnpackV3f(u32 c) {
	V3f res;
	res.x = (float)((c >> 24) & 0xff) / 255.0f;
	res.y = (float)((c >> 16) & 0xff) / 255.0f;
	res.z = (float)((c >> 8) & 0xff) / 255.0f;
	return res;
}

// https://en.wikipedia.org/wiki/SRGB#Specification_of_the_transformation
V3f LinearToSRGB(V3f color) {
	const M3f transform = { 
		.rows[0] = { 0.4142f, 0.3576f, 0.1805f },
		.rows[1] = { 0.2126f, 0.7152f, 0.0722f },
		.rows[2] = { 0.0193f, 0.1192f, 0.9505f },
	};

	return M3fV3fMul(&transform, color);
}

V4f LinearToSRGBA(V4f color) {
	color.xyz = LinearToSRGB(color.xyz);
	return color;
}

V3f LinearToGammaSRGB(V3f color, float gamma) {
	float igamma = 1.0f / gamma;
	V3f res;
	res.x = Pow(color.x, igamma);
	res.y = Pow(color.y, igamma);
	res.z = Pow(color.z, igamma);
	return res;
}

V4f LinearToGammaSRGBA(V4f color, float gamma) {
	color.xyz = LinearToGammaSRGB(color.xyz, gamma);
	return color;
}

V3f SRGBToLinear(V3f color) {
	const M3f transform = {
		.rows[0] = { +3.2406f, -1.5372f, -0.4986f },
		.rows[1] = { -0.9689f, +1.8758f, +0.0415f },
		.rows[2] = { +0.0557f, -0.2040f, +1.0570f },
	};
	return M3fV3fMul(&transform, color);
}

V4f SRGBAToLinear(V4f color) {
	color.xyz = SRGBToLinear(color.xyz);
	return color;
}

V3f GammaSRGBToLinear(V3f color, float gamma) {
	V3f res;
	res.x = Pow(color.x, gamma);
	res.y = Pow(color.y, gamma);
	res.z = Pow(color.z, gamma);
	return res;
}

V4f GammaSRGBAToLinear(V4f color, float gamma) {
	color.xyz = GammaSRGBToLinear(color.xyz, gamma);
	return color;
}

// http://en.wikipedia.org/wiki/HSL_and_HSV
V3f HSVToRGB(V3f col) {
	V3f res;

	float h = col.x;
	float s = col.y;
	float v = col.z;

	if (s == 0.0f) {
		// gray
		res.x = res.y = res.z = v;
		return res;
	}

	h       = Mod(h, 1.0f) / (60.0f / 360.0f);
	int   i = (int)h;
	float f = h - (float)i;
	float p = v * (1.0f - s);
	float q = v * (1.0f - s * f);
	float t = v * (1.0f - s * (1.0f - f));

	switch (i) {
	case 0:
		res = (V3f){ v, t, p };
		break;
	case 1:
		res = (V3f){ q, v, p };
		break;
	case 2:
		res = (V3f){ p, v, t };
		break;
	case 3:
		res = (V3f){ p, q, v };
		break;
	case 4:
		res = (V3f){ t, p, v };
		break;
	case 5:
	default:
		res = (V3f){ v, p, q };
		break;
	}

	return res;
}

// http://lolengine.net/blog/2013/01/13/fast-rgb-to-hsv
V3f RGBToHSV(V3f c) {
	float r = c.x;
	float g = c.y;
	float b = c.z;

	float k = 0.f;
	if (g < b) {
		float t = b;
		b = g;
		g = t;
		k = -1.f;
	}
	if (r < g) {
		float t = g;
		g = r;
		r = t;
		k = -2.f / 6.f - k;
	}

	V3f res;
	float chroma = r - (g < b ? g : b);
	res.x = Abs(k + (g - b) / (6.f * chroma + 1e-20f));
	res.y = chroma / (r + 1e-20f);
	res.z = r;
	return res;
}

V4f HSVToRGBA(V4f c) {
	c.xyz = HSVToRGB(c.xyz);
	return c;
}

V4f RGBAToHSV(V4f c) {
	c.xyz = RGBToHSV(c.xyz);
	return c;
}

void RearrangeBufferedForFFT(Complex *output, Complex *input, uint count) {
	uint target = 0;
	for (uint pos = 0; pos < count; ++pos) {
		output[pos]  = input[target];
		uint mask = count;
		while (target & (mask >>= 1)) {
			target &= ~mask;
		}
		target |= mask;
	}
}

void RearrangeForFFT(Complex *data, uint count) {
	uint target = 0;
	for (uint pos = 0; pos < count; ++pos) {
		if (target > pos) {
			Complex t = data[pos];
			data[pos]    = data[target];
			data[target] = t;
		}
		uint mask = count;
		while (target & (mask >>= 1)) {
			target &= ~mask;
		}
		target |= mask;
	}
}

extern const float    TwiddleFactorLookup[];
extern const unsigned TwiddleFactorLookupCount;

Complex TwiddleFactor(uint turns) {
	if (turns < TwiddleFactorLookupCount) {
		return ComplexRect(TwiddleFactorLookup[turns],
						TwiddleFactorLookup[turns + 1]);
	}
	return ComplexPolar(1.0f, -1.0f / (float)turns);
}

Complex InverseTwiddleFactor(uint turns) {
	if (turns < TwiddleFactorLookupCount) {
		return ComplexRect(TwiddleFactorLookup[turns],
						-TwiddleFactorLookup[turns + 1]);
	}
	return ComplexPolar(1.0f, 1.0f / (float)turns);
}

void ForwardFFT(Complex *data, uint count) {
	Assert((count & (count - 1)) == 0);

	// radix = 2
	for (uint index = 0; index < count; index += 2) {
		Complex p       = data[index + 0];
		Complex q       = data[index + 1];
		data[index + 0] = V2fAdd(p, q);
		data[index + 1] = V2fSub(p, q);
	}

	// radix = 4
	for (uint index = 0; index < count; index += 4) {
		Complex p = data[index + 0];
		Complex r = data[index + 1];
		Complex q = data[index + 2];
		Complex s = data[index + 3];

		s = (Complex) { s.im, -s.re };

		data[index + 0] = V2fAdd(p, q);
		data[index + 1] = V2fAdd(r, s);
		data[index + 2] = V2fSub(p, q);
		data[index + 3] = V2fSub(r, s);
	}

	for (uint step = 4; step < count; step <<= 1) {
		uint jump  = step << 1;
		Complex tw = TwiddleFactor(jump);
		Complex w  = ComplexRect(1.0f, 0.0f);
		for (uint block = 0; block < step; ++block) {
			for (uint index = block; index < count; index += jump) {
				uint next   = index + step;
				Complex a   = data[index];
				Complex b   = ComplexMul(w, data[next]);
				data[index] = V2fAdd(a, b);
				data[next]  = V2fSub(a, b);
			}
			w = ComplexMul(w, tw);
		}
	}
}

void ReverseFFT(Complex *data, uint count) {
	Assert((count & (count - 1)) == 0);

	// radix = 2
	for (uint index = 0; index < count; index += 2) {
		Complex p       = data[index + 0];
		Complex q       = data[index + 1];
		data[index + 0] = V2fAdd(p, q);
		data[index + 1] = V2fSub(p, q);
	}

	// radix = 4
	for (uint index = 0; index < count; index += 4) {
		Complex p = data[index + 0];
		Complex r = data[index + 1];
		Complex q = data[index + 2];
		Complex s = data[index + 3];

		s = (Complex ) { -s.im, s.re };

		data[index + 0] = V2fAdd(p, q);
		data[index + 1] = V2fAdd(r, s);
		data[index + 2] = V2fSub(p, q);
		data[index + 3] = V2fSub(r, s);
	}

	for (uint step = 4; step < count; step <<= 1) {
		uint jump  = step << 1;
		Complex tw = InverseTwiddleFactor(jump);
		Complex w  = ComplexRect(1.0f, 0.0f);
		for (uint block = 0; block < step; ++block) {
			for (uint index = block; index < count; index += jump) {
				uint next   = index + step;
				Complex a   = data[index];
				Complex b   = ComplexMul(w, data[next]);
				data[index] = V2fAdd(a, b);
				data[next]  = V2fSub(a, b);
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

void BufferedFFT(Complex *output, Complex *input, uint count) {
	RearrangeBufferedForFFT(output, input, count);
	ForwardFFT(output, count);
}

void BufferedIFFT(Complex *output, Complex *input, uint count) {
	RearrangeBufferedForFFT(output, input, count);
	ReverseFFT(output, count);
}

void FFT(Complex *data, uint count) {
	RearrangeForFFT(data, count);
	ForwardFFT(data, count);
}

void IFFT(Complex *data, uint count) {
	RearrangeForFFT(data, count);
	ReverseFFT(data, count);
}
