#include "terrain_density_functions.h"

#include "core/engine.h"
#include "game.h"

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


void DensityFuncSphereHole(f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth)
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

#define SAMPLES_PER_BEZIER 20

void DensityFuncBezierCurveHole(u32* seed, BezierDensityFuncSettings* generationSettings, f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth)
{
    u32 mapHeightTimesDepth = mapHeight * mapDepth;

    vec3 sphereCenter = vec3_from_float(mapWidth / 2);
    f32 sphereRadius = 20;

	// Generating random bezier curves
	u64 totalBezierCurvePoints = generationSettings->bezierTunnelControlPoints * generationSettings->bezierTunnelCount;

	vec3* bezierCurvePoints = ArenaAlloc(global->frameArena, totalBezierCurvePoints * sizeof(*bezierCurvePoints));

	for (int i = 0; i < generationSettings->bezierTunnelCount; i++)
	{
		for (int j = 0; j < generationSettings->bezierTunnelControlPoints; j++)
		{
			if (j == 0 || j + 1 == generationSettings->bezierTunnelControlPoints)
				bezierCurvePoints[i * generationSettings->bezierTunnelControlPoints + j] = vec3_add_vec3(vec3_mul_f32(RandomPointOnUnitSphere(seed), sphereRadius), sphereCenter);
			else
				bezierCurvePoints[i * generationSettings->bezierTunnelControlPoints + j] = vec3_add_vec3(vec3_mul_f32(RandomPointInUnitSphere(seed), sphereRadius), sphereCenter);
		}
	}

	// Sampling the bezier curves
	u64 totalBezierSamples = generationSettings->bezierTunnelCount * SAMPLES_PER_BEZIER;
	vec3* bezierSamples = ArenaAlloc(global->frameArena, totalBezierSamples * sizeof(*bezierSamples));
	vec3* interpolatedCurvePoints = ArenaAlloc(global->frameArena, generationSettings->bezierTunnelControlPoints * sizeof(*interpolatedCurvePoints));;

	for (int i = 0; i < generationSettings->bezierTunnelCount; i++)
	{
		for (int j = 0; j < SAMPLES_PER_BEZIER; j++)
		{
			f32 progress = (f32)j / (f32)(SAMPLES_PER_BEZIER - 1);
			MemoryCopy(interpolatedCurvePoints, bezierCurvePoints + (i * generationSettings->bezierTunnelControlPoints), generationSettings->bezierTunnelControlPoints * sizeof(*interpolatedCurvePoints));
			for (int curvePointCount = generationSettings->bezierTunnelControlPoints; curvePointCount > 1; curvePointCount--)
			{
				for (int k = 0; k < curvePointCount - 1; k++)
					interpolatedCurvePoints[k] = vec3_lerp(interpolatedCurvePoints[k], interpolatedCurvePoints[k + 1], progress);
			}

			bezierSamples[i * SAMPLES_PER_BEZIER + j] = interpolatedCurvePoints[0];
		}
	}

	// Generating random sphere holes
	vec3* sphereHoleCenters = ArenaAlloc(global->frameArena, generationSettings->sphereHoleCount * sizeof(*sphereHoleCenters));

	for (int i = 0; i < generationSettings->sphereHoleCount; i++)
	{
		sphereHoleCenters[i] = vec3_add_vec3(vec3_mul_f32(RandomPointInUnitSphere(seed), sphereRadius), sphereCenter);
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
                if (sphereValue >= 0)
				{
					*GetDensityValueRef(densityMap, mapHeightTimesDepth, mapDepth, x, y, z) = 1;
					continue;
				}
                if (sphereValue <= -2)
                    sphereValue = -2;

				f32 closestSphereDistanceSquared = 100000000000;
				for (u32 i = 0; i < generationSettings->sphereHoleCount; i++)
				{
					f32 distanceSquared = vec3_distance_squared(currentPoint, sphereHoleCenters[i]);
					if (distanceSquared < closestSphereDistanceSquared)
						closestSphereDistanceSquared = distanceSquared;
				}

				f32 closestSphereDistance = sqrtf(closestSphereDistanceSquared);
				closestSphereDistance -= generationSettings->sphereHoleRadius;
				if (closestSphereDistance <= -2)
				{
					*GetDensityValueRef(densityMap, mapHeightTimesDepth, mapDepth, x, y, z) = 1 + sphereValue - closestSphereDistance;
					continue;
				}

                f32 closestBezierDistanceSquared = 100000000000;
                for (u64 sampleIndex = 0; sampleIndex < totalBezierSamples; sampleIndex++)
                {
                    f32 distanceSquared = vec3_distance_squared(currentPoint, bezierSamples[sampleIndex]);
                    if (distanceSquared < closestBezierDistanceSquared)
                    {
                        closestBezierDistanceSquared = distanceSquared;
                    }
                }

                f32 closestBezierDistance = sqrt(closestBezierDistanceSquared) - generationSettings->bezierTunnelRadius;

                if (closestBezierDistance <= -2)
				{
					*GetDensityValueRef(densityMap, mapHeightTimesDepth, mapDepth, x, y, z) = 1 + sphereValue - closestBezierDistance;
					continue;
				}
				
                if (closestBezierDistance >= 0)
                    closestBezierDistance = 0;

				f32 closestAirDistance = fmin(closestSphereDistance, closestBezierDistance);

                // Calculating the density value
                *GetDensityValueRef(densityMap, mapHeightTimesDepth, mapDepth, x, y, z) = 1 + sphereValue - closestAirDistance;
            }
        }
    }
}

#define RANDOM_SPHERES_COUNT 1050
void DensityFuncRandomSpheres(f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth)
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

void BlurDensityMapGaussian(u32 iterations, u32 kernelSize, f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth)
{
	ArenaMarker marker = ArenaGetMarker(global->frameArena);

	if (iterations == 0)
		return;

	GRASSERT_DEBUG(kernelSize & 1);

	f32* originalDensityMap = densityMap;

	u32 densityMapHeightTimesDepth = mapHeight * mapDepth;
    u32 densityMapValueCount = mapWidth * mapHeight * mapDepth;

	// Bluring the density map
	// Generating the kernel
    u32 kernelSizeMinusOne = kernelSize - 1;
    u32 kernelSizeSquared = kernelSize * kernelSize;
    u32 kernelSizeCubed = kernelSizeSquared * kernelSize;

    f32* kernel = ArenaAlloc(global->frameArena, kernelSizeCubed * sizeof(*kernel));
    f32 kernelTotal = 0;

    vec3 kernelCenter = vec3_from_float(kernelSizeMinusOne / 2);
    f32 maximumEuclideanDistanceSquaredPlusOne = 1 + vec3_distance_squared(vec3_from_float(0), kernelCenter);

    for (u32 x = 0; x < kernelSize; x++)
    {
        for (u32 y = 0; y < kernelSize; y++)
        {
            for (u32 z = 0; z < kernelSize; z++)
            {
                // ========= Kernel version one (kernel value is the maximum manhattan distance from the center of the kernel squared minus the manhattan distance of the current element from the center of the kernel squared)
                // u32 value = 1 << ((x < kernelSizeMinusOne - x ? x : kernelSizeMinusOne - x) + (y < kernelSizeMinusOne - y ? y : kernelSizeMinusOne - y) + (z < kernelSizeMinusOne - z ? z : kernelSizeMinusOne - z));

                // ========= Kernel version two (kernel value is one plus the maximum euclidean distance from the center of the kernel squared minus the euclidean distance of the current element from the center of the kernel squared)
                f32 value = maximumEuclideanDistanceSquaredPlusOne - vec3_distance_squared(kernelCenter, vec3_create(x, y, z));

                kernel[x * kernelSizeSquared + y * kernelSize + z] = value;
                kernelTotal += value;
            }
        }
    }

    u32 padding = (kernelSize - 1) / 2;

	// Convolving the density map using the kernel to blur the density map
    f32* nonBlurredDensityMap = densityMap;
    densityMap = ArenaAlloc(global->frameArena, densityMapValueCount * sizeof(*densityMap));
	MemoryCopy(densityMap, nonBlurredDensityMap, densityMapValueCount * sizeof(*densityMap));

    // Looping over every kernel sized area in the density map
    for (u32 i = 0; i < iterations; i++)
    {
        for (u32 x = padding; x < mapWidth - padding; x++)
        {
            for (u32 y = padding; y < mapHeight - padding; y++)
            {
                for (u32 z = padding; z < mapDepth - padding; z++)
                {
                    f32 sum = 0;

                    // Looping over the kernel and multiplying each element of the kernel with its respective element of the kernel sized area in the density map
                    for (u32 kx = 0; kx < kernelSize; kx++)
                    {
                        for (u32 ky = 0; ky < kernelSize; ky++)
                        {
                            for (u32 kz = 0; kz < kernelSize; kz++)
                            {
                                //                                        [                      x                      ]   [            y              ]   [       z        ]
                                f32 preBlurDensity = nonBlurredDensityMap[(x + kx - padding) * densityMapHeightTimesDepth + (y + ky - padding) * mapDepth + (z + kz - padding)];
                                sum += preBlurDensity * kernel[kx * kernelSizeSquared + ky * kernelSize + kz];
                            }
                        }
                    }

                    *GetDensityValueRef(densityMap, densityMapHeightTimesDepth, mapDepth, x, y, z) = sum / kernelTotal;
                }
            }
        }

        // Switching the nonBlurredDensityMap and the density map if there is another blur iteration
		// The data in the densityMap (after the switch) doesn't have to be changed because it will be entirely overwritten and not read.
		// Because during the blurring process only the nonBlurredDensity map gets read, which has just gone through a blur iteration.
        if (i != iterations - 1)
        {
            f32* temp = nonBlurredDensityMap;
            nonBlurredDensityMap = densityMap;
            densityMap = temp;
        }
    }

	if (originalDensityMap != densityMap)
	{
		MemoryCopy(originalDensityMap, densityMap, densityMapValueCount * sizeof(*densityMap));
	}

	// "Freeing" the kernel and temp density map because these allocations can be quite large
	ArenaFreeMarker(global->frameArena, marker);
}

void BlurDensityMapBokeh(u32 iterations, u32 kernelSize, f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth)
{
	if (iterations == 0)
		return;

	ArenaMarker marker = ArenaGetMarker(global->frameArena);

	GRASSERT_DEBUG(kernelSize & 1);

	f32* originalDensityMap = densityMap;

	u32 densityMapHeightTimesDepth = mapHeight * mapDepth;
    u32 densityMapValueCount = mapWidth * mapHeight * mapDepth;

	// Bluring the density map
	// Generating the kernel
    u32 kernelSizeSquared = kernelSize * kernelSize;
    u32 kernelSizeCubed = kernelSizeSquared * kernelSize;

    f32* kernel = ArenaAlloc(global->frameArena, kernelSizeCubed * sizeof(*kernel));
    f32 kernelTotal = 0;

    for (u32 x = 0; x < kernelSize; x++)
    {
        for (u32 y = 0; y < kernelSize; y++)
        {
            for (u32 z = 0; z < kernelSize; z++)
            {
                
                f32 value = 1;

                kernel[x * kernelSizeSquared + y * kernelSize + z] = value;
                kernelTotal += value;
            }
        }
    }

    u32 padding = (kernelSize - 1) / 2;

	// Convolving the density map using the kernel to blur the density map
    f32* nonBlurredDensityMap = densityMap;
    densityMap = ArenaAlloc(global->frameArena, densityMapValueCount * sizeof(*densityMap));
	MemoryCopy(densityMap, nonBlurredDensityMap, densityMapValueCount * sizeof(*densityMap));

    // Looping over every kernel sized area in the density map
    for (u32 i = 0; i < iterations; i++)
    {
        for (u32 x = padding; x < mapWidth - padding; x++)
        {
            for (u32 y = padding; y < mapHeight - padding; y++)
            {
                for (u32 z = padding; z < mapDepth - padding; z++)
                {
                    f32 sum = 0;

                    // Looping over the kernel and multiplying each element of the kernel with its respective element of the kernel sized area in the density map
                    for (u32 kx = 0; kx < kernelSize; kx++)
                    {
                        for (u32 ky = 0; ky < kernelSize; ky++)
                        {
                            for (u32 kz = 0; kz < kernelSize; kz++)
                            {
                                //                                        [                      x                      ]   [            y              ]   [       z        ]
                                f32 preBlurDensity = nonBlurredDensityMap[(x + kx - padding) * densityMapHeightTimesDepth + (y + ky - padding) * mapDepth + (z + kz - padding)];
                                sum += preBlurDensity * kernel[kx * kernelSizeSquared + ky * kernelSize + kz];
                            }
                        }
                    }

                    *GetDensityValueRef(densityMap, densityMapHeightTimesDepth, mapDepth, x, y, z) = sum / kernelTotal;
                }
            }
        }

        // Switching the nonBlurredDensityMap and the density map if there is another blur iteration
		// The data in the densityMap (after the switch) doesn't have to be changed because it will be entirely overwritten and not read.
		// Because during the blurring process only the nonBlurredDensity map gets read, which has just gone through a blur iteration.
        if (i != iterations - 1)
        {
            f32* temp = nonBlurredDensityMap;
            nonBlurredDensityMap = densityMap;
            densityMap = temp;
        }
    }

	if (originalDensityMap != densityMap)
	{
		MemoryCopy(originalDensityMap, densityMap, densityMapValueCount * sizeof(*densityMap));
	}

	// "Freeing" the kernel and temp density map because these allocations can be quite large
	ArenaFreeMarker(global->frameArena, marker);
}


