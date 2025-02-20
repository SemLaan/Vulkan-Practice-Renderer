#pragma once
#include "defines.h"

#include "font_loader.h"
#include "math/lin_alg.h"
#include "text_renderer.h"

#define MAX_BEZIER_INSTANCE_COUNT 20000
#define MAX_SDF_DISTANCE 0.05f

#define FLOAT_ERROR 0.0001f

static inline bool FloatEqual(f32 a, f32 b)
{
    f32 temp = a - b;
    return (temp > -FLOAT_ERROR && temp < FLOAT_ERROR);
}

static inline bool FloatGreaterThan(f32 a, f32 b)
{
    f32 temp = a - b;
    return (temp > -FLOAT_ERROR);
}

static inline bool FloatLessThan(f32 a, f32 b)
{
    f32 temp = a - b;
    return (temp < FLOAT_ERROR);
}

typedef struct Roots
{
    u32 rootCount;
    f32 roots[3];
} Roots;

static inline Roots CubicGetRoots(f32 cubicCoef, f32 quadraticCoef, f32 linearCoef, f32 constantCoef)
{
    f32 a = quadraticCoef;
    f32 b = linearCoef;
    f32 c = constantCoef;

    // Dividing by the coefficient of the cubic term
    if (cubicCoef != 0)
    {
        a = a / cubicCoef;
        b = b / cubicCoef;
        c = c / cubicCoef;
    }

    f32 p = b / 3.f - a * a / 9.f;
    f32 q = a * a * a / 27.f - a * b / 6.f + c / 2.f;
    f32 D = p * p * p + q * q;

    Roots roots = {};

    if (FloatGreaterThan(D, 0))
    {
        if (FloatLessThan(D, 0))
            D = 0.001f;
        f32 temp1 = sqrtf(D);
        f32 temp2 = -q + temp1;
        f32 r = cbrtf(temp2);
        // f32 r = powf(-q + sqrtf(D), 1.f / 3.f);
        if (FloatLessThan(D, 0))
        {
            roots.rootCount = 2;
            roots.roots[0] = 2.f * r;
            roots.roots[1] = -r;
        }
        else
        {
            f32 s = cbrtf(-q - sqrtf(D));
            roots.rootCount = 1;
            roots.roots[0] = r + s;
        }
    }
    else
    {
        roots.rootCount = 3;
        if (FloatGreaterThan(p, 0))
            p = -0.0000000001f;
        f32 ang = acosf(-q / sqrtf(-p * p * p));
        f32 r = 2.f * sqrtf(-p);
        for (int k = -1; k <= 1; k++)
        {
            f32 theta = (ang - 2.f * PI * k) / 3.f;
            roots.roots[k + 1] = r * cosf(theta);
        }
    }

    for (int rootIndex = 0; rootIndex < roots.rootCount; rootIndex++)
    {
        roots.roots[rootIndex] -= a / 3.f;
    }

    return roots;
}

typedef struct QuadraticBezier
{
    vec2 beginPoint;
    vec2 midPoint;
    vec2 endPoint;
} QuadraticBezier;

static inline vec2 LineSegmentEvalTangent(vec2 beginPoint, vec2 endPoint)
{
    return vec2_sub_vec2(endPoint, beginPoint);
}

static inline vec2 LineSegmentEvalPosition(vec2 beginPoint, vec2 endPoint, f32 t)
{
    return vec2_add_vec2(vec2_mul_f32(beginPoint, 1.f - t), vec2_mul_f32(endPoint, t));
}

static inline vec2 QuadraticBezierEvalPosition(QuadraticBezier bezier, f32 t)
{
    vec2 p1 = vec2_sub_vec2(bezier.midPoint, bezier.beginPoint);
    vec2 p2 = vec2_sub_vec2(vec2_add_vec2(bezier.beginPoint, bezier.endPoint), vec2_mul_f32(bezier.midPoint, 2));

    f32 tSquared = t * t;

    return vec2_add_vec2(bezier.beginPoint, vec2_add_vec2(vec2_mul_f32(p1, t * 2.f), vec2_mul_f32(p2, tSquared)));
}

static inline vec2 QuadraticBezierEvalTangent(QuadraticBezier bezier, f32 t)
{
    vec2 p1 = vec2_sub_vec2(bezier.midPoint, bezier.beginPoint);
    vec2 p2 = vec2_sub_vec2(vec2_add_vec2(bezier.beginPoint, bezier.endPoint), vec2_mul_f32(bezier.midPoint, 2));

    return vec2_add_vec2(vec2_mul_f32(p1, 2.f), vec2_mul_f32(p2, 2.f * t));
}

static inline f32 ClampFloat(f32 value, f32 min, f32 max)
{
    if (value < min)
        value = min;
    if (value > max)
        value = max;
    return value;
}

static inline f32 AbsoluteFloat(f32 value)
{
    if (value < 0)
        value = -value;
    return value;
}

void CreateGlyphSDF(u8* textureData, u32 textureChannels, u32 textureWidth, u32 textureHeight, Font* font, GlyphData* glyphData, u32 glyphIndex, vec2i bottomLeftTextureCoord, vec2i topRightTextureCoord, f32 padding)
{
    f32 maxDistance = MAX_SDF_DISTANCE;
    f32 distanceRange = maxDistance * 2;
    u32 horizontalPixelCount = topRightTextureCoord.x - bottomLeftTextureCoord.x + 1;
    u32 verticalPixelCount = topRightTextureCoord.y - bottomLeftTextureCoord.y + 1;
    u32 charValue = font->renderableCharacters[glyphIndex];

    QuadraticBezier outlineData[MAX_BEZIER_INSTANCE_COUNT] = {};
    u32 bezierCount = 0;

    // ================================================== Calculating the bezier curves for this glyph
    for (int contour = 0; contour < glyphData->contourCounts[charValue]; contour++)
    {
        u32 previousContourEnd;
        if (contour == 0)
            previousContourEnd = -1; // -1 because 0 is the start of the current contour
        else
            previousContourEnd = glyphData->endPointsOfContours[charValue][contour - 1];
        u32 currentContourStart = previousContourEnd + 1;
        u32 contourPointCount = glyphData->endPointsOfContours[charValue][contour] - previousContourEnd;

        // If the first point is on curve then the first off curve point is point 1 and otherwise point 0 is the first off curve point
        i32 firstOffCurvePoint = glyphData->firstPointOnCurve[charValue][contour] ? 1 : 0;

        for (int point = firstOffCurvePoint; point < contourPointCount; point += 2)
        {
            int previousPointIndex = ((point + contourPointCount - 1) % contourPointCount) + currentContourStart;
            int currentPoint = point + currentContourStart;
            int nextPointIndex = ((point + 1) % contourPointCount) + currentContourStart;

            outlineData[bezierCount].beginPoint = glyphData->pointArrays[charValue][previousPointIndex];
            outlineData[bezierCount].midPoint = glyphData->pointArrays[charValue][currentPoint];
            outlineData[bezierCount].endPoint = glyphData->pointArrays[charValue][nextPointIndex];
            bezierCount++;
        }
    }

    // ====================================================== Calculating signed distances for each texel
    // Calculating how to transform local glyph texel coordinates to glyph space coordinates
    vec2 paddedGlyphAnchorValue = glyphData->glyphBottomLeftAnchor[charValue];
    paddedGlyphAnchorValue.x -= padding;
    paddedGlyphAnchorValue.y -= padding;
    vec2 paddedGlyphSize = glyphData->glyphSizes[charValue];
    paddedGlyphSize.x += padding * 2;
    paddedGlyphSize.y += padding * 2;

    // Looping over every texel
    for (u32 x = 0; x < horizontalPixelCount; x++)
    {
        for (u32 y = 0; y < verticalPixelCount; y++)
        {
            // transforming local glyph texel coordinates to glyph space coordinates
            vec2 pixelCenterWorldSpace = vec2_create(((f32)x + 0.5f) / (f32)horizontalPixelCount, ((f32)y + 0.5f) / (f32)verticalPixelCount);
            vec2 pixelCenterFontSpace = vec2_create(
                pixelCenterWorldSpace.x * paddedGlyphSize.x + paddedGlyphAnchorValue.x,
                pixelCenterWorldSpace.y * paddedGlyphSize.y + paddedGlyphAnchorValue.y);

            // Setting the closest distance and orthogonality that we've found so far to an arbitrarily high number so any edge will be closer than this.
            f32 closestDistance = 10000000000000.f;
            f32 closestDistanceOrthogonality = 10000000000000.f;
            f32 closestDistanceSign = 1;

            // Looping over all bezier curves
            for (u32 bezierIndex = 0; bezierIndex < bezierCount; bezierIndex++)
            {
                QuadraticBezier bezier = outlineData[bezierIndex];

                // Calculating some helper values
                vec2 p0 = vec2_sub_vec2(pixelCenterFontSpace, bezier.beginPoint);
                vec2 p1 = vec2_sub_vec2(bezier.midPoint, bezier.beginPoint);
                vec2 p2 = vec2_sub_vec2(vec2_add_vec2(bezier.beginPoint, bezier.endPoint), vec2_mul_f32(bezier.midPoint, 2));

                // Calculating the coefficients of the cubic
                f32 cubicCoef = vec2_dot_vec2(p2, p2);                                // cubic coefficient
                f32 quadraticCoef = 3.f * vec2_dot_vec2(p1, p2);                      // quadruatic coefficient
                f32 linearCoef = 2.f * vec2_dot_vec2(p1, p1) - vec2_dot_vec2(p0, p2); // linear coefficient
                f32 constantCoef = -vec2_dot_vec2(p0, p1);                            // constant coefficient

                // If the bezier curve is linear, get the closest point on the line and check if it's the closest so far =====================================================================================================================
                if (FloatEqual(cubicCoef, 0) && FloatEqual(quadraticCoef, 0))
                {
                    // If the bezier curve is linear, we treat it as a line between its begin and endpoint, so we need to recalculate p1 based on endpoint instead of midpoint
                    p1 = vec2_sub_vec2(bezier.endPoint, bezier.beginPoint);

                    // Calculating the value for t at the closest point of the line (the value for t where the tangent and vector from the line at t to the point are orthogonal)
                    f32 t = vec2_dot_vec2(p0, p1) / vec2_dot_vec2(p1, p1);
                    t = ClampFloat(t, 0, 1);

                    // Evaluating the line at t
                    vec2 closestPoint = LineSegmentEvalPosition(bezier.beginPoint, bezier.endPoint, t);
                    f32 distance = vec2_distance(closestPoint, pixelCenterFontSpace);

                    // If distance is closer than closest distance (or almost closer to account for floating point error)
                    if (FloatLessThan(distance, closestDistance))
                    {
                        vec2 normalizedTangent = vec2_normalize(LineSegmentEvalTangent(bezier.beginPoint, bezier.endPoint));
                        vec2 normalizedClosestPointToPixelCenter = vec2_normalize(vec2_sub_vec2(pixelCenterFontSpace, closestPoint));
                        f32 signedOrthogonality = vec2_cross_vec2(normalizedTangent, normalizedClosestPointToPixelCenter);
                        f32 orthogonality = AbsoluteFloat(signedOrthogonality);

                        // If the distance is equal to closest distance, the orthogonality decides which one is closer, if
                        if (FloatEqual(distance, closestDistance) && orthogonality < closestDistanceOrthogonality)
                            continue;

                        closestDistance = distance;
                        closestDistanceOrthogonality = orthogonality;
                        closestDistanceSign = signedOrthogonality / orthogonality;
                    }
                }
                else // If the bezier curve is not linear, solve closest distances using cubic formula and check if any of the distances are closest so far =====================================================================================
                {
                    // Get the t values where the
                    Roots cubicRoots = CubicGetRoots(cubicCoef, quadraticCoef, linearCoef, constantCoef);

                    // Adding the possible closest points to an array to evaluate them, the begin and enpoints are always candidates in case the
                    vec2 closestPointCandidates[5] = {};
                    closestPointCandidates[0] = bezier.beginPoint;
                    closestPointCandidates[1] = bezier.endPoint;
					
					f32 candidatesTValues[5] = {};
					candidatesTValues[0] = 0;
					candidatesTValues[1] = 1;

                    u32 candidateIndex = 2;
					
                    // Checking all the roots and adding the positions they correspond to if they fall within (0, 1)
                    for (int rootIndex = 0; rootIndex < cubicRoots.rootCount; rootIndex++)
                    {
                        if (cubicRoots.roots[rootIndex] > 0 && 1 > cubicRoots.roots[rootIndex])
                        {
                            closestPointCandidates[candidateIndex] = QuadraticBezierEvalPosition(bezier, cubicRoots.roots[rootIndex]);
							candidatesTValues[candidateIndex] = cubicRoots.roots[rootIndex];
                            candidateIndex++;
                        }
                    }

                    for (int i = 0; i < candidateIndex; i++)
                    {
                        f32 distance = vec2_distance(closestPointCandidates[i], pixelCenterFontSpace);
                        if (FloatLessThan(distance, closestDistance))
                        {
                            vec2 normalizedTangent = vec2_normalize(QuadraticBezierEvalTangent(bezier, candidatesTValues[i]));
                            vec2 normalizedClosestPointToPixelCenter = vec2_normalize(vec2_sub_vec2(pixelCenterFontSpace, closestPointCandidates[i]));
                            f32 signedOrthogonality = vec2_cross_vec2(normalizedTangent, normalizedClosestPointToPixelCenter);
                            f32 orthogonality = AbsoluteFloat(signedOrthogonality);

                            // If the distance is equal to closest distance, the orthogonality decides which one is closer, if
                            if (FloatEqual(distance, closestDistance) && orthogonality < closestDistanceOrthogonality)
                                continue;

                            closestDistance = distance;
                            closestDistanceOrthogonality = orthogonality;
                            closestDistanceSign = signedOrthogonality / orthogonality;
                        }
                    }
                }
            }

			f32 signedDistance = closestDistance * closestDistanceSign;
            i64 u8EncodedDistance = nearbyintf(((signedDistance / distanceRange) + 0.5f) * 255.f);
            if (u8EncodedDistance > 255)
                u8EncodedDistance = 255;
            if (u8EncodedDistance < 0)
                u8EncodedDistance = 0;

            u32 pixelIndex = ((x + bottomLeftTextureCoord.x) * textureChannels) + ((y + bottomLeftTextureCoord.y) * textureWidth * textureChannels);
            textureData[pixelIndex] = u8EncodedDistance;
            // textureData[pixelIndex + 1] = 0;
            // textureData[pixelIndex + 2] = 0;
        }
    }
}

u32 Partition(u32* indices, vec2i* objectSizes, u32 low, u32 high)
{
	i32 partitionValue = objectSizes[indices[high]].y;
	
	i32 i = low - 1;
	i32 j = low;
	for (; j < high; j++)
	{
		if (objectSizes[indices[j]].y > partitionValue)
		{
			i++;
			u32 temp = indices[i];
			indices[i] = indices[j];
			indices[j] = temp;
		}
	}

	i++;
	u32 temp = indices[i];
	indices[i] = indices[j];
	indices[j] = temp;

	return i;
}

void QuickSort(u32* indices, vec2i* objectSizes, u32 low, u32 high)
{
	// If there is more than one element in the given range
	if (low < high)
	{
		u32 partitionIndex = Partition(indices, objectSizes, low, high);

		if (partitionIndex != 0)
			QuickSort(indices, objectSizes, low, partitionIndex - 1);
		if (partitionIndex + 1 != high)
			QuickSort(indices, objectSizes, partitionIndex + 1, high);
	}
}


#define MAX_BIN_PACKING_OBJECTS 200
#define MAX_BIN_COUNT 10

/// @brief Calculates a 2d bin packing for the given objects
/// @param vec2i* objectPositions 
/// @param vec2i* objectSizes
/// @param u32 objectCount
/// @returns u32 that represents the height required for the packed layout
u32 Calculate2DBinPacking(vec2i* objectPositions, vec2i* objectSizes, u32 objectCount, u32 binWidth)
{
	GRASSERT_DEBUG(objectCount <= MAX_BIN_PACKING_OBJECTS);

	// Sort the objects on decreasing height
	u32 sortedIndices[MAX_BIN_PACKING_OBJECTS] = {};
	for (u32 i = 0; i < objectCount; i++)
		sortedIndices[i] = i;

	QuickSort(sortedIndices, objectSizes, 0, objectCount-1);

	u32 binCount = 0;
	u32 binsSpaceLeft[MAX_BIN_COUNT] = {};
	i32 binHeights[MAX_BIN_COUNT+1] = {};
	
	// Adding the first object and initializing the first bin
	objectPositions[sortedIndices[0]].x = 0;
	objectPositions[sortedIndices[0]].y = 0;
	binsSpaceLeft[binCount] = binWidth - objectSizes[sortedIndices[0]].x;
	binHeights[binCount + 1] = objectSizes[sortedIndices[0]].y;
	binCount++;

	for (u32 i = 1; i < objectCount; i++)
	{
		vec2i objectSize = objectSizes[sortedIndices[i]];
		bool objectPlaced = false;

		// Finding the first bin in which the object fits and puttin it there
		for (u32 j = 0; j < binCount; j++)
		{
			if (objectSize.x <= binsSpaceLeft[j])
			{
				objectPositions[sortedIndices[i]].x = binWidth - binsSpaceLeft[j];
				objectPositions[sortedIndices[i]].y = binHeights[j];
				binsSpaceLeft[j] -= objectSize.x;

				objectPlaced = true;
				break;
			}
		}

		// If the object didn't fit into any existing bin, create a new one
		if (!objectPlaced)
		{
			// Initializing the values of the new bin
			binHeights[binCount + 1] = binHeights[binCount] + objectSize.y;
			binsSpaceLeft[binCount] = binWidth;

			objectPositions[sortedIndices[i]].x = binWidth - binsSpaceLeft[binCount];
			objectPositions[sortedIndices[i]].y = binHeights[binCount];
			binsSpaceLeft[binCount] -= objectSizes[sortedIndices[i]].x;
			
			binCount++;
		}
	}

	return (u32)binHeights[binCount];
}


