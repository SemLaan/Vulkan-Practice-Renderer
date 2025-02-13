#pragma once
#include "containers/darray.h"
#include "core/asserts.h"
#include "core/meminc.h"
#include "math/lin_alg.h"
#include "math/random_utils.h"



void DensityFuncSphereHole(f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth);

#define MIN_BEZIER_TUNNEL_COUNT 0
#define MAX_BEZIER_TUNNEL_COUNT 10
#define MIN_BEZIER_TUNNEL_RADIUS 1
#define MAX_BEZIER_TUNNEL_RADIUS 10
#define MIN_BEZIER_TUNNEL_CONTROL_POINTS 3
#define MAX_BEZIER_TUNNEL_CONTROL_POINTS 10
#define MIN_SPHERE_HOLE_COUNT 0
#define MAX_SPHERE_HOLE_COUNT 5
#define MIN_SPHERE_HOLE_RADIUS 1
#define MAX_SPHERE_HOLE_RADIUS 10

typedef struct BezierDensityFuncSettings
{
	i64 bezierTunnelCount;
	f32 bezierTunnelRadius;
	i64 bezierTunnelControlPoints;

	i64 sphereHoleCount;
	f32 sphereHoleRadius;
} BezierDensityFuncSettings;


void DensityFuncBezierCurveHole(u32* seed, BezierDensityFuncSettings* generationSettings, f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth);

void DensityFuncRandomSpheres(f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth);


#define MIN_BLUR_ITERATIONS 0
#define MAX_BLUR_ITERATIONS 20
#define POSSIBLE_BLUR_KERNEL_SIZES {3, 5, 7}
#define POSSIBLE_BLUR_KERNEL_SIZES_COUNT 3
void BlurDensityMapGaussian(u32 iterations, u32 kernelSize, f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth);
void BlurDensityMapBokeh(u32 iterations, u32 kernelSize, f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth);

