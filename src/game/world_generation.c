#include "world_generation.h"

#include "marching_cubes/marching_cubes.h"
#include "renderer/ui/debug_ui.h"
#include "game_rendering.h"
#include "core/input.h"
#include "renderer/mesh_optimizer.h"
#include "core/profiler.h"

#define DEFAULT_DENSITY_MAP_RESOLUTION 100

// World data (meshes and related data)
static World world;
static WorldGenParameters worldGenParams;
static DebugMenu* worldGenParamDebugMenu;


static inline void GenerateMarchingCubesWorld();
static inline void DestroyMarchingCubesWorld();


void WorldGenerationInit()
{
	i64 blurKernelSizeOptions[POSSIBLE_BLUR_KERNEL_SIZES_COUNT] = POSSIBLE_BLUR_KERNEL_SIZES;
	MemoryCopy(worldGenParams.blurKernelSizeOptions, blurKernelSizeOptions, sizeof(blurKernelSizeOptions));
	worldGenParams.densityMapResolution = 50;

	worldGenParamDebugMenu = DebugUICreateMenu("World Gen Parameters");
	DebugUIAddSliderInt(worldGenParamDebugMenu, "Density map resolution", 10, 200, &worldGenParams.densityMapResolution);
	DebugUIAddSliderInt(worldGenParamDebugMenu, "Blur Iterations", MIN_BLUR_ITERATIONS, MAX_BLUR_ITERATIONS, &worldGenParams.blurIterations);
	DebugUIAddSliderDiscrete(worldGenParamDebugMenu, "Blur Kernel Size", worldGenParams.blurKernelSizeOptions, POSSIBLE_BLUR_KERNEL_SIZES_COUNT, &worldGenParams.blurKernelSize);
	DebugUIAddSliderInt(worldGenParamDebugMenu, "Bezier tunnel count", MIN_BEZIER_TUNNEL_COUNT, MAX_BEZIER_TUNNEL_COUNT, &worldGenParams.bezierDensityFuncSettings.bezierTunnelCount);
	DebugUIAddSliderFloat(worldGenParamDebugMenu, "Bezier tunnel radius", MIN_BEZIER_TUNNEL_RADIUS, MAX_BEZIER_TUNNEL_RADIUS, &worldGenParams.bezierDensityFuncSettings.bezierTunnelRadius);
	DebugUIAddSliderInt(worldGenParamDebugMenu, "Bezier tunnel control points", MIN_BEZIER_TUNNEL_CONTROL_POINTS, MAX_BEZIER_TUNNEL_CONTROL_POINTS, &worldGenParams.bezierDensityFuncSettings.bezierTunnelControlPoints);
	DebugUIAddSliderInt(worldGenParamDebugMenu, "Sphere hole count", MIN_SPHERE_HOLE_COUNT, MAX_SPHERE_HOLE_COUNT, &worldGenParams.bezierDensityFuncSettings.sphereHoleCount);
	DebugUIAddSliderFloat(worldGenParamDebugMenu, "Sphere hole radius", MIN_SPHERE_HOLE_RADIUS, MAX_SPHERE_HOLE_RADIUS, &worldGenParams.bezierDensityFuncSettings.sphereHoleRadius);

	// Generating marching cubes terrain
	world.terrainSeed = 0;
	GenerateMarchingCubesWorld();
}

void WorldGenerationUpdate()
{
	if (GetButtonDown(BUTTON_RIGHTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_RIGHTMOUSEBTN))
	{
		START_SCOPE("Destroy marching cubes world");
		DestroyMarchingCubesWorld();
		END_SCOPE();
		START_SCOPE("Create marching cubes world");
		GenerateMarchingCubesWorld();
		END_SCOPE();
	}
}

void WorldGenerationShutdown()
{
	// Destroying debug menu for world gen parameters
	DebugUIDestroyMenu(worldGenParamDebugMenu);

	// Destroying world data
	DestroyMarchingCubesWorld();
}

void WorldGenerationDrawWorld()
{
	Draw(1, &world.marchingCubesGpuMesh.vertexBuffer, world.marchingCubesGpuMesh.indexBuffer, &world.terrainModelMatrix, 1);
}

MeshData WorldGenerationGetColliderMesh()
{
	return world.colliderMesh;
}

mat4 WorldGenerationGetModelMatrix()
{
	// Calculating the model matrix to center 
	mat4 scale = mat4_3Dscale(vec3_from_float(DEFAULT_DENSITY_MAP_RESOLUTION / (f32)worldGenParams.densityMapResolution));
	mat4 translation = mat4_3Dtranslate(vec3_from_float(-DEFAULT_DENSITY_MAP_RESOLUTION * 0.5f));
	world.terrainModelMatrix =  mat4_mul_mat4(translation, scale);
	return world.terrainModelMatrix;
}

static inline void GenerateMarchingCubesWorld()
{
	START_SCOPE("Allocate memory");
	// Allocating memory for the density map
	u32 densityMapValueCount = worldGenParams.densityMapResolution * worldGenParams.densityMapResolution * worldGenParams.densityMapResolution;
	world.terrainDensityMap = Alloc(GetGlobalAllocator(), sizeof(*world.terrainDensityMap) * densityMapValueCount);
	END_SCOPE();

	// Calculating the model matrix to center 
	mat4 scale = mat4_3Dscale(vec3_from_float(DEFAULT_DENSITY_MAP_RESOLUTION / (f32)worldGenParams.densityMapResolution));
	mat4 translation = mat4_3Dtranslate(vec3_from_float(-DEFAULT_DENSITY_MAP_RESOLUTION * 0.5f));
	world.terrainModelMatrix =  mat4_mul_mat4(translation, scale);

	// Generating the density map
	BezierDensityFuncSettings densitySettingsCopy = worldGenParams.bezierDensityFuncSettings;
	densitySettingsCopy.baseSphereRadius = 0.4f * worldGenParams.densityMapResolution;
	densitySettingsCopy.bezierTunnelRadius = densitySettingsCopy.bezierTunnelRadius * worldGenParams.densityMapResolution / DEFAULT_DENSITY_MAP_RESOLUTION;
	densitySettingsCopy.sphereHoleRadius = densitySettingsCopy.sphereHoleRadius * worldGenParams.densityMapResolution / DEFAULT_DENSITY_MAP_RESOLUTION;

	START_SCOPE("Generating voxel data");
	DensityFuncBezierCurveHole(&world.terrainSeed, &densitySettingsCopy, world.terrainDensityMap, worldGenParams.densityMapResolution);
	END_SCOPE();
	START_SCOPE("Blurring voxel data");
	BlurDensityMapGaussian(worldGenParams.blurIterations, worldGenParams.blurKernelSize, world.terrainDensityMap, worldGenParams.densityMapResolution, worldGenParams.densityMapResolution, worldGenParams.densityMapResolution);
	END_SCOPE();

	// Generating the mesh
	START_SCOPE("Generating mesh with marching cubes");
	MeshData mcMeshData = MarchingCubesGenerateMesh(world.terrainDensityMap, worldGenParams.densityMapResolution, worldGenParams.densityMapResolution, worldGenParams.densityMapResolution);
	END_SCOPE();

	// This is for smoothing the mesh normals and removing duplicate vertices, used for raycasting
	START_SCOPE("Merge normals");
	world.colliderMesh = MeshOptimizerMergeNormals(mcMeshData, offsetof(VertexT2, position), offsetof(VertexT2, normal));
	END_SCOPE();

	// Uploading the mesh
	START_SCOPE("Upload mesh and free cpu data");
	world.marchingCubesGpuMesh.vertexBuffer = VertexBufferCreate(mcMeshData.vertices, mcMeshData.vertexStride * mcMeshData.vertexCount);
	world.marchingCubesGpuMesh.indexBuffer = IndexBufferCreate(mcMeshData.indices, mcMeshData.indexCount);

	MarchingCubesFreeMeshData(mcMeshData);
	END_SCOPE();
}

static inline void DestroyMarchingCubesWorld()
{
	MeshOptimizerFreeMeshData(world.colliderMesh);
	Free(GetGlobalAllocator(), world.terrainDensityMap);
	VertexBufferDestroy(world.marchingCubesGpuMesh.vertexBuffer);
	IndexBufferDestroy(world.marchingCubesGpuMesh.indexBuffer);
}

