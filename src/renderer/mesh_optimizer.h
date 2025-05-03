#pragma once
#include "defines.h"
#include "renderer_types.h"
#include "core/engine.h"

MeshData MeshOptimizerMergeNormals(MeshData originalMesh, u32 positionOffset, u32 normalOffset);

static inline void MeshOptimizerFreeMeshData(MeshData meshData)
{
	Free(global->largeObjectAllocator, meshData.vertices);
	Free(global->largeObjectAllocator, meshData.indices);
}

