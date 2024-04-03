#pragma once
#include "defines.h"
#include "math/math_types.h"

typedef struct GlyphData
{
    u32 pointCount;
    vec2* points;
} GlyphData;


GlyphData LoadFont(const char* filename);

