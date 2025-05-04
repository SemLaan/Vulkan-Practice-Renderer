#pragma once
#include "defines.h"
#include "marching_cubes/terrain_density_functions.h"
#include "renderer/renderer_types.h"

typedef struct WorldGenParameters
{
	BezierDensityFuncSettings bezierDensityFuncSettings;

	i64 densityMapResolution;
	i64 blurIterations;
	i64 blurKernelSize;
	i64 blurKernelSizeOptions[POSSIBLE_BLUR_KERNEL_SIZES_COUNT];
} WorldGenParameters;

typedef struct World
{
    f32* terrainDensityMap;
    GPUMesh marchingCubesGpuMesh;
	mat4 terrainModelMatrix;
    u32 terrainSeed;
} World;

void WorldGenerationInit();
void WorldGenerationUpdate();
void WorldGenerationShutdown();

void WorldGenerationDrawWorld();

