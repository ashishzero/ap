/*
 * PCG Random Number Generation for C.
 *
 * Copyright 2014 Melissa O'Neill <oneill@pcg-random.org>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * For additional information about the PCG random number generation scheme,
 * including its license and other licensing options, visit
 *
 *       http://www.pcg-random.org
 */

#include "KrRandom.h"
#include <math.h>

u32 RandomEx(RandomSource *rng) {
	u64 oldstate   = rng->state;
	rng->state     = oldstate * 6364136223846793005ULL + rng->inc;
	u32 xorshifted = (u32)(((oldstate >> 18u) ^ oldstate) >> 27u);
	u32 rot        = oldstate >> 59u;
	return (xorshifted >> rot) | (xorshifted << ((rot & (~rot + 1u)) & 31));
}

u32 RandomBoundEx(RandomSource *rng, u32 bound) {
	u32 threshold = (bound & (~bound + 1u)) % bound;
	for (;;) {
		u32 r = RandomEx(rng);
		if (r >= threshold)
			return r % bound;
	}
}

u32 RandomRangeEx(RandomSource *rng, u32 min, u32 max) {
	return min + RandomBoundEx(rng, max - min);
}

float RandomFloatUnitEx(RandomSource *rng) {
	return (float)ldexpf((float)RandomEx(rng), -32);
}

float RandomFloatBoundEx(RandomSource *rng, float bound) {
	return RandomFloatUnitEx(rng) * bound;
}

float RandomFloatRangeEx(RandomSource *rng, float min, float max) {
	return min + RandomFloatUnitEx(rng) * (max - min);
}

float RandomFloatEx(RandomSource *rng) {
	return RandomFloatRangeEx(rng, -1, 1);
}

void RandomSourceSeedEx(RandomSource *rng, u64 state, u64 seq) {
	rng->state = 0U;
	rng->inc   = (seq << 1u) | 1u;
	RandomEx(rng);
	rng->state += state;
	RandomEx(rng);
}

u32 Random() {
	return RandomEx(&Thread.Random);
}

u32 RandomBound(u32 bound) {
	return RandomBoundEx(&Thread.Random, bound);
}

u32 RandomRange(u32 min, u32 max) {
	return RandomRangeEx(&Thread.Random, min, max);
}

float RandomFloatUnit() {
	return RandomFloatUnitEx(&Thread.Random);
}

float RandomFloatBound(float bound) {
	return RandomFloatBoundEx(&Thread.Random, bound);
}

float RandomFloatRange(float min, float max) {
	return RandomFloatRangeEx(&Thread.Random, min, max);
}

float RandomFloat() {
	return RandomFloatEx(&Thread.Random);
}

void RandomSourceSeed(u64 state, u64 seq) {
	RandomSourceSeedEx(&Thread.Random, state, seq);
}
