#pragma once
#include "defines.h"
#include "math/math_types.h"

#define MAX_CONTOURS 10

typedef struct GlyphData
{
    u32 pointCounts[255];                       // Amount of points in the glyph, index from 0 - 255
    u32 contourCounts[255];                     // Amount of contours in the glyph, index from 0 - 255
    u32 endPointsOfContours[255][MAX_CONTOURS]; // Arrays of indices of contour ends
    vec2* pointArrays[255];                     // Array of points in the glyph, index from 0 - 255
    // Boolean that says whether the first point is on curve or not (one per contour)
    // after the first point the points alternate between on and off curve, index from 0 - 255
    bool firstPointOnCurve[255][MAX_CONTOURS];
    f32 advanceWidths[255];    // Offset from this glyph to the next one, index from 0 - 255
    f32 leftSideBearings[255]; // The point moved by the offset, index from 0 - 255
    vec2 glyphSizes[255];      // specific glyph size divided by global max glyph sizes
	vec2 glyphBottomLeftAnchor[255];
} GlyphData;

GlyphData* LoadFont(const char* filename);
void FreeGlyphData(GlyphData* glyphData); // TODO: implement
