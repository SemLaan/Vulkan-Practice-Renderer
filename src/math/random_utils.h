#pragma once
#include "math_types.h"

static inline u32 PCG_Hash(u32 input)
{
    u32 state = input * 747796405u + 2891336453u;
    u32 word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

static inline f32 RandomFloat(u32* seed)
{
    *seed = PCG_Hash(*seed);
    return (f32)*seed / (f32)UINT32_MAX;
}