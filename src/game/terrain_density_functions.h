#pragma once
#include "containers/darray.h"
#include "core/asserts.h"
#include "core/meminc.h"
#include "math/lin_alg.h"
#include "math/random_utils.h"



void DensityFuncSphereHole(f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth);


typedef struct BezierDensityFuncSettings
{

	
} BezierDensityFuncSettings;

void DensityFuncBezierCurveHole(u32* seed, BezierDensityFuncSettings* generationSettings, f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth);

void DensityFuncRandomSpheres(f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth);
void BlurDensityMap(u32 iterations, u32 kernelSize, f32* densityMap, u32 mapWidth, u32 mapHeight, u32 mapDepth);

