#pragma once
#include "math/lin_alg.h"
#include "math/random_utils.h"
#include "containers/darray.h"
#include "core/asserts.h"
#include "core/meminc.h"


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
		//vec2 pointOnDisc = RandomPointInUnitDisc(&seed);
		//sphereCenters[i] = vec3_add_vec3(vec3_mul_f32(vec3_create(pointOnDisc.x, 0, pointOnDisc.y), spheresRadius), spheresCenter);
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



