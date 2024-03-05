#pragma once
#include "core/asserts.h"
#include "math_types.h"

#include <math.h>

static inline f32 TempK(i32 n, f32 h)
{
    f32 result = n + h / 60.f;
    f32 temp = floor(result / 6);
    return result - (temp * 6);
}

static inline f32 TempF(i32 n, f32 h, f32 s, f32 b)
{
    f32 k = TempK(n, h);
    f32 value = 1;
    if (k < value)
        value = k;
    if (4 - k < value)
        value = 4 - k;
    if (0 > value)
        value = 0;

    return b * (1 - s * value);
}

// The range of the input parameters is H: [0, 360], S: [0, 100], B: [0, 100].
// The range of all output values is [0, 255].
vec3i HSB_to_RGB(f32 h, f32 s, f32 b)
{
    s /= 100;
    b /= 100;

    vec3i result;
    result.x = 255 * TempF(5, h, s, b);
    result.y = 255 * TempF(3, h, s, b);
    result.z = 255 * TempF(1, h, s, b);
    return result;
}