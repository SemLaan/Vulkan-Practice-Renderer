#pragma once
#include "containers/darray.h"
#include "core/asserts.h"
#include "core/meminc.h"
#include "math/lin_alg.h"
#include "math/random_utils.h"

// Indexes into a densityMap
static inline f32* GetDensityValueRef(f32* densityMap, u32 mapHeightTimesDepth, u32 mapDepth, u32 x, u32 y, u32 z)
{
    GRASSERT_DEBUG(densityMap);

    return &densityMap[x * mapHeightTimesDepth + y * mapDepth + z];
}

// Indexes into a densityMap
static inline f32 GetDensityValueRaw(f32* densityMap, u32 mapHeightTimesDepth, u32 mapDepth, u32 x, u32 y, u32 z)
{
    GRASSERT_DEBUG(densityMap);

    return densityMap[x * mapHeightTimesDepth + y * mapDepth + z];
}

static inline void DensityFuncSphereHole(f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth)
{
    u32 mapHeightTimesDepth = mapHeight * mapDepth;

    // Spheres for calculating density
    vec3 sphere1Center = vec3_from_float(mapWidth / 2);
    f32 sphere1Radius = 20;
    vec3 sphere2Center = vec3_from_float(mapWidth / 2);
    sphere2Center.x -= 13;
    f32 sphere2Radius = 8;

    // Looping over every density point and calculating the density.
    for (u32 x = 0; x < mapWidth; x++)
    {
        for (u32 y = 0; y < mapHeight; y++)
        {
            for (u32 z = 0; z < mapDepth; z++)
            {
                // Calculating whether the current point is in the sphere or in the hole sphere
                f32 sphereValue = vec3_distance(vec3_create(x, y, z), sphere1Center) - sphere1Radius;
                if (sphereValue <= -2)
                    sphereValue = -2;
                if (sphereValue >= 0)
                    sphereValue = 0;
                f32 sphereHoleValue = vec3_distance(vec3_create(x, y, z), sphere2Center) - sphere2Radius;
                if (sphereHoleValue <= -2)
                    sphereHoleValue = -2;
                if (sphereHoleValue >= 0)
                    sphereHoleValue = 0;

                // Calculating the density value
                *GetDensityValueRef(densityMap, mapHeightTimesDepth, mapDepth, x, y, z) = 1 + sphereValue - sphereHoleValue;
            }
        }
    }
}

static u32 seed1 = 10;

static inline void DensityFuncBezierCurveHole(f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth)
{
    u32 mapHeightTimesDepth = mapHeight * mapDepth;

    vec3 sphereCenter = vec3_from_float(mapWidth / 2);
    f32 sphereRadius = 20;

// Generating a random bezier curve
#define BEZIER_DEGREE_PLUS_ONE (50 + 1)

    vec3 bezierCurvePoints[BEZIER_DEGREE_PLUS_ONE] = {};

    bezierCurvePoints[0] = vec3_add_vec3(vec3_mul_f32(RandomPointOnUnitSphere(&seed1), sphereRadius), sphereCenter);
    bezierCurvePoints[BEZIER_DEGREE_PLUS_ONE - 1] = vec3_add_vec3(vec3_mul_f32(RandomPointOnUnitSphere(&seed1), sphereRadius), sphereCenter);

    for (u32 i = 1; i < BEZIER_DEGREE_PLUS_ONE - 1; i++)
    {
        bezierCurvePoints[i] = vec3_add_vec3(vec3_mul_f32(RandomPointInUnitSphere(&seed1), sphereRadius), sphereCenter);
    }

    f32 bezierRadius = 8;

#define CURVE_POINTS (1 << 9)
#define INITIAL_BEZIER_SAMPLE_POINTS_PLUS_ONE (1 << 8)
    u32 distanceBetweenSamplePoints = CURVE_POINTS / INITIAL_BEZIER_SAMPLE_POINTS_PLUS_ONE;
    vec3 bezierCurveSamples[CURVE_POINTS] = {};

    for (u32 i = 0; i < CURVE_POINTS; i++)
    {
        f32 progress = (f32)i / (f32)(CURVE_POINTS - 1);
        vec3 interpolatedCurvePoints[BEZIER_DEGREE_PLUS_ONE] = {};
		MemoryCopy(interpolatedCurvePoints, bezierCurvePoints, sizeof(interpolatedCurvePoints));
        for (u32 curvePointCount = BEZIER_DEGREE_PLUS_ONE; curvePointCount > 1; curvePointCount--)
        {
            for (u32 j = 0; j < curvePointCount - 1; j++)
            {
				interpolatedCurvePoints[j] = vec3_lerp(interpolatedCurvePoints[j],interpolatedCurvePoints[j + 1], progress);;
            }
        }

        bezierCurveSamples[i] = interpolatedCurvePoints[0];
    }

    // Looping over every density point and calculating the density.
    for (u32 x = 0; x < mapWidth; x++)
    {
        for (u32 y = 0; y < mapHeight; y++)
        {
            for (u32 z = 0; z < mapDepth; z++)
            {
                vec3 currentPoint = vec3_create(x, y, z);

                // Calculating whether the current point is in the sphere or in the bezier curve hole
                f32 sphereValue = vec3_distance(currentPoint, sphereCenter) - sphereRadius;
                if (sphereValue <= -2)
                    sphereValue = -2;
                if (sphereValue >= 0)
                    sphereValue = 0;

                f32 closestBezierDistanceSquared = 100000000000;
                for (u32 sampleIndex = distanceBetweenSamplePoints - 1; sampleIndex < CURVE_POINTS; sampleIndex += distanceBetweenSamplePoints)
                {
                    f32 distanceSquared = vec3_distance_squared(currentPoint, bezierCurveSamples[sampleIndex]);
                    if (distanceSquared < closestBezierDistanceSquared)
                    {
                        closestBezierDistanceSquared = distanceSquared;
                    }
                }

                f32 closestBezierDistance = sqrt(closestBezierDistanceSquared) - bezierRadius;

                if (closestBezierDistance <= -2)
                    closestBezierDistance = -2;
                if (closestBezierDistance >= 0)
                    closestBezierDistance = 0;

                // Calculating the density value
                *GetDensityValueRef(densityMap, mapHeightTimesDepth, mapDepth, x, y, z) = 1 + sphereValue - closestBezierDistance;
            }
        }
    }
}

#define RANDOM_SPHERES_COUNT 1050
static inline void DensityFuncRandomSpheres(f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth)
{
    u32 mapHeightTimesDepth = mapHeight * mapDepth;

    // Spheres for calculating density
    vec3 spheresCenter = vec3_from_float(mapWidth / 2);
    f32 spheresRadius = mapWidth / 8;
    f32 sphereRadius = 2;

    // Generating random positions in a sphere
    vec3 sphereCenters[RANDOM_SPHERES_COUNT] = {};
    u32 seed = 10;

    for (u32 i = 0; i < RANDOM_SPHERES_COUNT; i++)
    {
        // vec2 pointOnDisc = RandomPointInUnitDisc(&seed);
        // sphereCenters[i] = vec3_add_vec3(vec3_mul_f32(vec3_create(pointOnDisc.x, 0, pointOnDisc.y), spheresRadius), spheresCenter);
        sphereCenters[i] = vec3_add_vec3(vec3_mul_f32(RandomPointInUnitSphere(&seed), spheresRadius), spheresCenter);
        _DEBUG("x: %f, y: %f, z: %f", sphereCenters[i].x, sphereCenters[i].y, sphereCenters[i].z);
    }

    // Looping over every density point and calculating the density.
    for (u32 x = 0; x < mapWidth; x++)
    {
        for (u32 y = 0; y < mapHeight; y++)
        {
            for (u32 z = 0; z < mapDepth; z++)
            {
                // Find distance to closest sphere
                f32 closestDistance = 0;
                for (u32 i = 0; i < RANDOM_SPHERES_COUNT; i++)
                {
                    f32 distance = vec3_distance(vec3_create(x, y, z), sphereCenters[i]) - sphereRadius;
                    if (distance <= closestDistance)
                    {
                        closestDistance = distance;
                    }
                }

                if (closestDistance <= -2)
                    closestDistance = -2;

                // Calculating the density value
                *GetDensityValueRef(densityMap, mapHeightTimesDepth, mapDepth, x, y, z) = 1 + closestDistance;
            }
        }
    }
}
