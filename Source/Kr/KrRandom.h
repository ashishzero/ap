#pragma once

#include "KrBase.h"

u32   RandomEx(RandomSource *rng);
u32   RandomBoundEx(RandomSource *rng, u32 bound);
u32   RandomRangeEx(RandomSource *rng, u32 min, u32 max);
float RandomFloatUnitEx(RandomSource *rng);
float RandomFloatBoundEx(RandomSource *rng, float bound);
float RandomFloatRangeEx(RandomSource *rng, float min, float max);
float RandomFloatEx(RandomSource *rng);
void  RandomSourceSeedEx(RandomSource *rng, u64 state, u64 seq);

u32   Random();
u32   RandomBound(u32 bound);
u32   RandomRange(u32 min, u32 max);
float RandomFloatUnit();
float RandomFloatBound(float bound);
float RandomFloatRange(float min, float max);
float RandomFloat();
void  RandomSourceSeed(u64 state, u64 seq);
