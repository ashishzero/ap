#pragma once

#include "KrPlatform.h"

#include <math.h>

#define PI        (3.1415926535f)
#define INV_PI    (1.0f / PI)
#define TAU       (2.0f * PI)
#define INV_TAU   (1.0f / TAU)

#define V2fFmt    "(%f, %f)"
#define V3fFmt    "(%f, %f, %f)"
#define V4fFmt    "(%f, %f, %f, %f)"
#define V2iFmt    "(%d, %d)"
#define V3iFmt    "(%d, %d, %d)"
#define V4iFmt    "(%d, %d, %d, %d)"
#define M2fFmt    V2fFmt " " V2fFmt
#define M3fFmt    V3fFmt " " V3fFmt " " V3fFmt
#define M4fFmt    V4fFmt " " V4fFmt " " V4fFmt " " V4fFmt

#define V2Arg(v)  (v).x, (v).y
#define V3Arg(v)  (v).x, (v).y, (v).z
#define V4Arg(v)  (v).x, (v).y, (v).z, (v).w

#define M2Arg(m)  V2Arg((m).rows[0]), V2Arg((m).rows[1])
#define M3Arg(m)  V3Arg((m).rows[0]), V3Arg((m).rows[1]), V3Arg((m).rows[2])
#define M4Arg(m)  V4Arg((m).rows[0]), V4Arg((m).rows[1]), V4Arg((m).rows[2]), V4Arg((m).rows[3])

#define Deg2Rad   (PI / 180)
#define Dad2Deg   (180 / PI)
#define Deg2Turn  (1.0f / 360.0f)
#define Turn2Deg  (360.0f)
#define Rad2Turn  (1.0f / TAU)
#define Turn2Rad  (TAU)

typedef union V2f {
	struct { float x, y;   };
	struct { float m[2];   };
	struct { float re, im; };
} V2f;

typedef V2f Complex;

typedef union V3f {
	struct { float x, y, z;          };
	struct { float m[3];             };
	struct { V2f xy; float last;  };
	struct { float first; V2f yz; };
} V3f;

typedef union V4f {
	struct { float x, y, z, w;        };
	struct { float m[4];              };
	struct { V2f xy; V2f zw;    };
	struct { V3f xyz; float last;  };
	struct { float first; V3f yzw; };
} V4f;

typedef union V2i {
	struct { int x, y; };
	struct { int m[2]; };
} V2i;

typedef union V3i {
	struct { int x, y, z;          };
	struct { int m[3];             };
	struct { V2i xy; int last;  };
	struct { int first; V2i yz; };
} V3i;

typedef union V4i {
	struct { int x, y, z, w;        };
	struct { int m[4];              };
	struct { V2i xy; V2i zw;  };
	struct { V3i xyz; int last;  };
	struct { int first; V3i yzw; };
} V4i;

typedef union M2f {
	V2f rows[2];
	float  m[4];
	float  m2[2][2];
} M2f;

typedef union M3f {
	V3f rows[3];
	float  m[9];
	float  m2[3][3];
} M3f;

typedef union M4f {
	V4f rows[4];
	float  m[16];
	float  m2[4][4];
} M4f;

typedef union Quat {
	V4f v;
	struct { float x, y, z, w; };
} Quat;

#define Sgn(x)          (float) ((0 < (x)) - ((x) < 0))
#define Abs(x)          fabsf(x)
#define Sqrt(x)         sqrtf(x)
#define Pow(x, y)       powf(x, y)
#define Mod(x, y)       fmodf(x, y)
#define Sqr(x)          ((x) * (x))
#define Floor(x)        floorf(x)
#define Round(x)        roundf(x)
#define Ceil(x)         ceilf(x)
#define Sin(turns)      sinf((turns) * TAU)
#define Cos(turns)      cosf((turns) * TAU)
#define Tan(turns)      tanf((turns) * TAU)
#define ArcSin(y)       (asinf(y) * INV_TAU)
#define ArcCos(y)       (acosf(y) * INV_TAU)
#define ArcTan2(x, y)   (atan2f(y, x) * INV_TAU)
#define Arm(turns)      (V2f){ Cos(turns), Sin(turns) }
#define RevArm(turns)   (V2f){ Sin(turns), Cos(turns) }

bool    AlmostEqual(float a, float b, float delta);
float   Wrap(float min, float max, float a);
float   WrapPhase(float turns);

V2f     ComplexMul(V2f a, V2f b);
V2f     ComplexPolar(float amp, float turns);
V2f     ComplexRect(float re, float im);
V2f     ComplexConjugate(V2f c);
float   ComplexAmplitudeSq(V2f c);
float   ComplexAmplitude(V2f c);
float   ComplexPhase(V2f c);

V2f     V2fNeg(V2f a);
V3f     V3fNeg(V3f a);
V4f     V4fNeg(V4f a);
V2i     V2iNeg(V2i a);
V3i     V3iNeg(V3i a);
V4i     V4iNeg(V4i a);
V2f     V2fAdd(V2f a, V2f b);
V2f     V2fSub(V2f a, V2f b);
V2f     V2fMul(V2f a, V2f b);
V2f     V2fDiv(V2f a, V2f b);
V2f     V2fMulF(V2f a, float b);
V2f     V2fDivF(V2f a, float b);
void    V2fAddScaled(V2f *r, float s, V2f o);
void    V2fSubScaled(V2f *r, float s, V2f o);
V3f     V3fAdd(V3f a, V3f b);
V3f     V3fSub(V3f a, V3f b);
V3f     V3fMul(V3f a, V3f b);
V3f     V3fDiv(V3f a, V3f b);
V3f     V3fMulF(V3f a, float b);
V3f     V3fDivF(V3f a, float b);
void    V3fAddScaled(V3f *r, float s, V3f o);
void    V3fSubScaled(V3f *r, float s, V3f o);
V4f     V4fAdd(V4f a, V4f b);
V4f     V4fSub(V4f a, V4f b);
V4f     V4fMul(V4f a, V4f b);
V4f     V4fDiv(V4f a, V4f b);
V4f     V4fMulF(V4f a, float b);
V4f     V4fDivF(V4f a, float b);
void    V4fAddScaled(V4f *r, float s, V4f o);
void    V4fSubScaled(V4f *r, float s, V4f o);
V2i     V2iAdd(V2i a, V2i b);
V2i     V2iSub(V2i a, V2i b);
V2i     V2iMul(V2i a, V2i b);
V2i     V2iDiv(V2i a, V2i b);
V2i     V2iMulI(V2i a, int b);
V2i     V2iDivI(V2i a, int b);
void    V2iAddScaled(V2i *r, int s, V2i o);
void    V2iSubScaled(V2i *r, int s, V2i o);
V3i     V3iAdd(V3i a, V3i b);
V3i     V3iSub(V3i a, V3i b);
V3i     V3iMul(V3i a, V3i b);
V3i     V3iDiv(V3i a, V3i b);
V3i     V3iMulI(V3i a, int b);
V3i     V3iDivI(V3i a, int b);
void    V3iAddScaled(V3i *r, int s, V3i o);
void    V3iSubScaled(V3i *r, int s, V3i o);
V4i     V4iAdd(V4i a, V4i b);
V4i     V4iSub(V4i a, V4i b);
V4i     V4iMul(V4i a, V4i b);
V4i     V4iDiv(V4i a, V4i b);
V4i     V4iMulI(V4i a, int b);
V4i     V4iDivI(V4i a, int b);
void    V4iAddScaled(V4i *r, int s, V4i o);
void    V4iSubScaled(V4i *r, int s, V4i o);
float   V2fDot(V2f a, V2f b);
float   V3fDot(V3f a, V3f b);
float   V4fDot(V4f a, V4f b);
float   V2fDet(V2f a, V2f b);
int     V2iDot(V2i a, V2i b);
int     V3iDot(V3i a, V3i b);
int     V4iDot(V4i a, V4i b);
int     V2iDet(V2i a, V2i b);
float   V2fCross(V2f a, V2f b);
V3f     V3fCross(V3f a, V3f b);
int     V2iCross(V2i a, V2i b);
V3i     V3iCross(V3i a, V3i b);
V2f     V2fTriple(V2f a, V2f b, V2f c);
V3f     V3fTriple(V3f a, V3f b, V3f c);
V2i     V2iTriple(V2i a, V2i b, V2i c);
V3i     V3iTriple(V3i a, V3i b, V3i c);
float   V2fLengthSq(V2f a);
float   V2fLength(V2f a);
float   V3fLengthSq(V3f a);
float   V3fLength(V3f a);
float   V4fLengthSq(V4f a);
float   V4fLength(V4f a);
int     V2iLengthSq(V2i a);
int     V2iLength(V2i a);
int     V3iLengthSq(V3i a);
int     V3iLength(V3i a);
int     V4iLengthSq(V4i a);
int     V4iLength(V4i a);
float   V2fDistance(V2f a, V2f b);
float   V3fDistance(V3f a, V3f b);
float   V4fDistance(V4f a, V4f b);
int     V2iDistance(V2i a, V2i b);
int     V3iDistance(V3i a, V3i b);
int     V4iDistance(V4i a, V4i b);
V2f     V2fNormalizeOrZero(V2f a);
V2f     V2fNormalize(V2f a);
V3f     V3fNormalizeOrZero(V3f a);
V3f     V3fNormalize(V3f a);
V4f     V4fNormalizeOrZero(V4f a);
V4f     V4fNormalize(V4f a);
V2i     V2iNormalizeOrZero(V2i a);
V2i     V2iNormalize(V2i a);
V3i     V3iNormalizeOrZero(V3i a);
V3i     V3iNormalize(V3i a);
V4i     V4iNormalizeOrZero(V4i a);
V4i     V4iNormalize(V4i a);
void    V3fOrthoNormalBasis(V3f *r, V3f *u, V3f *f);
void    V3iOrthoNormalBasis(V3i *r, V3i *u, V3i *f);
float   V2fTurns(V2f a, V2f b);
float   V3fTurns(V3f a, V3f b, V3f norm);
float   V2iTurns(V2i a, V2i b);
float   V3iTurns(V3i a, V3i b, V3i norm);
M2f     M2fNeg(M2f const *a);
M3f     M3fNeg(M3f const *a);
M4f     M4fNeg(M4f const *a);
M2f     M2fAdd(M2f const *a, M2f const *b);
M3f     M3fAdd(M3f const *a, M3f const *b);
M4f     M4fAdd(M4f const *a, M4f const *b);
M2f     M2fSub(M2f const *a, M2f const *b);
M3f     M3fSub(M3f const *a, M3f const *b);
M4f     M4fSub(M4f const *a, M4f const *b);
M2f     M2fMulF(M2f const *a, float b);
M3f     M3fMulF(M3f const *a, float b);
M4f     M4fMulF(M4f const *a, float b);
M2f     M2fDivF(M2f const *a, float b);
M3f     M3fDivF(M3f const *a, float b);
M4f     M4fDivF(M4f const *a, float b);

float   M2fDet(M2f const *mat);
M2f     M2fInverse(M2f const *mat);
M2f     M2fTranspose(M2f const *m);
float   M3fDet(M3f const *mat);
M3f     M3fInverse(M3f const *mat);
M3f     M3fTranspose(M3f const *m);
float   M4fDet(M4f const *mat);
M4f     M4fInverse(M4f const *mat);
M4f     M4fTranspose(M4f const *m);

M2f     M2fMul(M2f const *a, M2f const *b);
M3f     M3fMul(M3f const *a, M3f const *b);
M4f     M4fMul(M4f const *a, M4f const *b);

V2f     V2fM2fMul(V2f vec, M2f const *mat);
V2f     M2fV2fMul(M2f const *mat, V2f vec);
V3f     V3fM3fMul(V3f vec, M3f const *mat);
V3f     M3fV3fMul(M3f const *mat, V3f vec);
V4f     V4fM4fMul(V4f vec, M4f const *mat);
V4f     M4fV4fMul(M4f const *mat, V4f vec);

void    M2fTranform(M2f *mat, M2f const *t);
void    M3fTranform(M3f *mat, M3f const *t);
void    M4fTranform(M4f *mat, M4f const *t);

Quat    QuatNeg(Quat q);
Quat    QuatAdd(Quat r1, Quat r2);
Quat    QuatSub(Quat r1, Quat r2);
Quat    QuatMulF(Quat q, float s);
Quat    QuatDivF(Quat q, float s);
void    QuatAddScaled(Quat *q, float s, Quat o);
void    QuatSubScaled(Quat *q, float s, Quat o);
float   QuatDot(Quat a, Quat b);
float   QuatLengthSq(Quat a);
float   QuatLength(Quat a);
Quat    QuatNormalizeOrIdentity(Quat q);
Quat    QuatNormalize(Quat q);
Quat    QuatConjugate(Quat q);
Quat    QuatMul(Quat q1, Quat q2);
V3f     QuatMulV3f(Quat q, V3f v);

V3f     M4fRight(M4f const *m);
V3f     M4fUp(M4f const *m);
V3f     M4fForward(M4f const *m);
V3f     QuatRight(Quat q);
V3f     QuatUp(Quat q);
V3f     QuatForward(Quat q);

M2f     M3fToM2f(M3f const *mat);
M3f     M2fToM3f(M2f const *mat);
M3f     M4fToM3f(M4f const *mat);
M4f     M3fToM4f(M3f const *mat);
V3f     QuatToAxisTurns(Quat q, float *turns);
M4f     QuatToM4f(Quat q);
V3f     QuatToEuler(Quat q);
Quat    M4fToQuat(M4f const *m);

M2f     M2fIdentity();
M2f     M2fDiagonal(float x, float y);
M3f     M3fIdentity();
M3f     M3fDiagonal(float x, float y, float z);
M4f     M4fIdentity();
M4f     M4fDiagonal(float x, float y, float z, float w);
M2f     M2fRotate(V2f arm);
M2f     M2fRotateF(float turns);
M3f     M3fScaleF(float x, float y);
M3f     M3fScale(V2f s);
M3f     M3fTranslateF(float x, float y);
M3f     M3fTranslate(V2f t);
M3f     M3fRotate(V2f arm);
M3f     M3fRotateF(float turns);
M3f     M3fLookAt(V2f from, V2f to, V2f forward);
M4f     M4fScaleF(float x, float y, float z);
M4f     M4fScale(V3f s);
M4f     M4fTranslateF(float x, float y, float z);
M4f     M4fTranslate(V3f t);
M4f     M4fRotateX(V2f arm);
M4f     M4fRotateXF(float turns);
M4f     M4RotateY(V2f arm);
M4f     M4fRotateYF(float turns);
M4f     M4fRotateZ(V2f arm);
M4f     M4fRotateZF(float turns);
M4f     M4fRotate(V3f axis, V2f arm);
M4f     M4fFrotate(float x, float y, float z, float turns);
M4f     M4fLookAt(V3f from, V3f to, V3f world_up);
M4f     M4fLookTowards(V3f dir, V3f world_up);
M4f     M4fOrthographic(float l, float r, float t, float b, float n, float f);
M4f     M4fPerspective(float fov, float aspect_ratio, float n, float f);

Quat    QuatIdentity();
Quat    QuatAxisTurns(V3f axis, float turns);
Quat    QuatEuler(float pitch, float yaw, float roll);
Quat    QuatBetweenV3f(V3f from, V3f to);
Quat    QuatBetween(Quat a, Quat b);
Quat    QuatLookAt(V3f from, V3f to, V3f world_forward);

float   Lerp(float from, float to, float t);
V2f     V2fLerp(V2f from, V2f to, float t);
V3f     V3fLerp(V3f from, V3f to, float t);
V4f     V4fLerp(V4f from, V4f to, float t);
Quat    QuatLerp(Quat from, Quat to, float t);

float   BezierQuadratic(float a, float b, float c, float t);
V2f     V2fBezierQuadratic(V2f a, V2f b, V2f c, float t);
V3f     V3fBezierQuadratic(V3f a, V3f b, V3f c, float t);
V4f     V4fBezierQuadratic(V4f a, V4f b, V4f c, float t);
Quat    QuatBezierQuadratic(Quat a, Quat b, Quat c, float t);
float   BezierCubic(float a, float b, float c, float d, float t);
V2f     V2fBezierCubic(V2f a, V2f b, V2f c, V2f d, float t);
V3f     V3fBezierCubic(V3f a, V3f b, V3f c, V3f d, float t);
V4f     V4fBezierCubic(V4f a, V4f b, V4f c, V4f d, float t);
Quat    QuatBezierCubic(Quat a, Quat b, Quat c, Quat d, float t);

void    BuildBezierQuadratic(float a, float b, float c, float *points, int segments);
void    V2fBuildBezierQuadratic(V2f a, V2f b, V2f c, V2f *points, int segments);
void    V3fBuildBezierQuadratic(V3f a, V3f b, V3f c, V3f *points, int segments);
void    V4fBuildBezierQuadratic(V4f a, V4f b, V4f c, V4f *points, int segments);
void    QuatBuildBezierQuadratic(Quat a, Quat b, Quat c, Quat *points, int segments);
void    BuildBezierCubic(float a, float b, float c, float d, float *points, int segments);
void    V2fBuildBezierCubic(V2f a, V2f b, V2f c, V2f d, V2f *points, int segments);
void    V3fBuildBezierCubic(V3f a, V3f b, V3f c, V3f d, V3f *points, int segments);
void    V4fBuildBezierCubic(V4f a, V4f b, V4f c, V4f d, V4f *points, int segments);
void    QuatBuildBezierCubic(Quat a, Quat b, Quat c, Quat d, Quat *points, int segments);

float   Slerp(float from, float to, float turns, float t);
V2f     V2fSlerp(V2f from, V2f to, float t);
V3f     V3fSlerp(V3f from, V3f to, float t);
Quat    QuatSlerp(Quat from, Quat to, float t);

float   Step(float edge, float val);
V2f     V2fStep(V2f edge, V2f val);
V3f     V3fStep(V3f edge, V3f val);
V4f     V4fStep(V4f edge, V4f val);
Quat    QuatStep(Quat edge, Quat val);

float   SmoothStep(float a, float b, float v);
float   V2fSmoothStep(V2f a, V2f b, V2f v);
float   V3fSmoothStep(V3f a, V3f b, V3f v);
float   V4fSmoothStep(V4f a, V4f b, V4f v);
float   QuatSmoothStep(Quat a, Quat b, Quat v);
float   InverseSmoothStep(float x);

float   MapRange(float in_a, float in_b, float out_a, float out_b, float v);

float   MoveTowards(float from, float to, float factor);
V2f     V2fMoveTowards(V2f from, V2f to, float factor);
V3f     V3fMoveTowards(V3f from, V3f to, float factor);
V4f     V4fMoveTowards(V4f from, V4f to, float factor);
V2f     V2fRotateAround(V2f point, V2f center, float turns);
Quat    QuatRotateTowards(Quat from, Quat to, float max_turns);
V2f     V2fReflect(V2f d, V2f n);

void    UnpackRGBA(u32 c, u8 channels[4]);
u32     PackRGBA(u8 r, u8 g, u8 b, u8 a);
u32     PackV4f(V4f v);
u32     PackV3f(V3f v);
V4f     UnpackV4f(u32 c);
V3f     UnpackV3f(u32 c);

V3f     LinearToSRGB(V3f color);
V4f     LinearToSRGBA(V4f color);
V3f     LinearToGammaSRGB(V3f color, float gamma);
V4f     LinearToGammaSRGBA(V4f color, float gamma);
V3f     SRGBToLinear(V3f color);
V4f     SRGBAToLinear(V4f color);
V3f     GammaSRGBToLinear(V3f color, float gamma);
V4f     GammaSRGBAToLinear(V4f color, float gamma);
V3f     HSVToRGB(V3f col);
V3f     RGBToHSV(V3f c);
V4f     HSVToRGBA(V4f c);
V4f     RGBAToHSV(V4f c);

Complex TwiddleFactor(uint turns);
Complex InverseTwiddleFactor(uint turns);
void    RearrangeBufferedForFFT(Complex *output, Complex *input, uint count);
void    RearrangeForFFT(Complex *data, uint count);
void    ForwardFFT(Complex *data, uint count);
void    ReverseFFT(Complex *data, uint count);
void    BufferedFFT(Complex *output, Complex *input, uint count);
void    BufferedIFFT(Complex *output, Complex *input, uint count);
void    FFT(Complex *data, uint count);
void    IFFT(Complex *data, uint count);
