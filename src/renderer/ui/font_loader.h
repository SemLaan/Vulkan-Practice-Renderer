#pragma once
#include "defines.h"
#include "math/math_types.h"



typedef struct GlyphData
{
    u32 pointCounts[255];           // Amount of points in the glyph, index from 0 - 255
    vec2* pointsArrays[255];        // Array of points in the glyph, index from 0 - 255
    bool* onCurveArrays[255];        // Array of booleans that say whether the point is on curve or not, index from 0 - 255
    f32 advanceWidths[255];             // Offset from this glyph to the next one, index from 0 - 255
    f32 leftSideBearings[255];    // The point moved by the offset, index from 0 - 255
} GlyphData;


GlyphData* LoadFont(const char* filename);
void FreeGlyphArray(GlyphData* glyphArray);//TODO: implement
