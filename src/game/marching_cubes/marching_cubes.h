#pragma once
#include "renderer/material.h"
#include "core/engine.h"


// Vertices in the mesh data generated have a position and a normal, vertices are not shared and normals are just the face normals
MeshData MarchingCubesGenerateMesh(f32* densityMap, u32 densityMapWidth, u32 densityMapHeight, u32 densityMapDepth);

inline static void MarchingCubesFreeMeshData(MeshData meshData)
{
	Free(global->largeObjectAllocator, meshData.vertices);
	Free(global->largeObjectAllocator, meshData.indices);
}

