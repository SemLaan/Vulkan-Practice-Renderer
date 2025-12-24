#include "mesh_optimizer.h"

#include "math/lin_alg.h"
#include "core/profiler.h"

#define HASH_BACKING_ARRAY_SIZE_FACTOR 1.6f
#define VEC3_BYTE_COUNT 12

MeshData MeshOptimizerMergeNormals(MeshData originalMesh, u32 positionOffset, u32 normalOffset)
{
	// Creating a new mesh that can be edited without destroying the original
	MeshData newMesh = {};
	newMesh.indexCount = originalMesh.indexCount;
	newMesh.indices = AlignedAlloc(global->largeObjectAllocator, sizeof(*newMesh.indices) * originalMesh.indexCount, CACHE_ALIGN);
	MemoryCopy(newMesh.indices, originalMesh.indices, sizeof(*newMesh.indices) * originalMesh.indexCount);
	newMesh.vertices = AlignedAlloc(global->largeObjectAllocator, originalMesh.vertexCount * originalMesh.vertexStride, CACHE_ALIGN);
	MemoryCopy(newMesh.vertices, originalMesh.vertices, originalMesh.vertexCount * originalMesh.vertexStride);
	newMesh.vertexStride = originalMesh.vertexStride;

	u8* vertices = newMesh.vertices;
	u64 verticesMaxAddress = (u64)vertices + originalMesh.vertexCount * originalMesh.vertexStride;

	ArenaMarker marker = ArenaGetMarker(global->frameArena);

	START_SCOPE("Merge normals - Creating a map from old verts to new verts");
	// Finding indices of duplicate vertices, 
	// making a mapping from the old vertex indices to the new vertex indices
	u32* duplicateVertexIndices = ArenaAlignedAlloc(global->frameArena, sizeof(*duplicateVertexIndices) * originalMesh.vertexCount, CACHE_ALIGN);
	u32* originalVertexIndexToNewVertexIndex = ArenaAlignedAlloc(global->frameArena, sizeof(*originalVertexIndexToNewVertexIndex) * originalMesh.vertexCount, CACHE_ALIGN);
	u32* hashToOldVertexIndex = ArenaAlignedAlloc(global->frameArena, sizeof(*hashToOldVertexIndex) * originalMesh.vertexCount * HASH_BACKING_ARRAY_SIZE_FACTOR, CACHE_ALIGN);
	MemorySet(hashToOldVertexIndex, INT32_MAX, sizeof(*hashToOldVertexIndex) * originalMesh.vertexCount * HASH_BACKING_ARRAY_SIZE_FACTOR);
	u32 duplicateVertexCount = 0;
	originalVertexIndexToNewVertexIndex[0] = 0;
	for (u32 i = 0; i < originalMesh.vertexCount; i++)
	{
		bool duplicate = false;
		vec3* iPosition = (vec3*)(vertices + positionOffset + originalMesh.vertexStride * i);
		originalVertexIndexToNewVertexIndex[i] = i - duplicateVertexCount;

		// Calculating jenkins one at a time hash of the vec3
		u32 hash = 0;
		u8* key = (u8*)iPosition;
		for (u32 i = 0; i < VEC3_BYTE_COUNT; i++)
		{
			hash += key[i];
			hash += hash << 10;
			hash ^= hash >> 6;
		}
		hash += hash << 3;
		hash ^= hash >> 11;
		hash += hash << 15;
		hash %= (size_t)(originalMesh.vertexCount * HASH_BACKING_ARRAY_SIZE_FACTOR);

		while (hashToOldVertexIndex[hash] != UINT32_MAX)
		{
			vec3* possibleDuplicatePosition = (vec3*)(vertices + positionOffset + originalMesh.vertexStride * hashToOldVertexIndex[hash]);
			if (MemoryCompare(iPosition, possibleDuplicatePosition, sizeof(vec3)))
			{
				duplicate = true;
				duplicateVertexIndices[duplicateVertexCount] = i;
				duplicateVertexCount++;

				originalVertexIndexToNewVertexIndex[i] = originalVertexIndexToNewVertexIndex[hashToOldVertexIndex[hash]];

				break;
			}
			hash++;
			hash %= (size_t)(originalMesh.vertexCount * HASH_BACKING_ARRAY_SIZE_FACTOR);
		}

		if (!duplicate)
		{
			hashToOldVertexIndex[hash] = i;
		}
	}
	END_SCOPE();

	START_SCOPE("Merge normals - Mapping vertices");
	// Mapping the old indices to the new indices
	for (u32 i = 0; i < newMesh.indexCount; i++)
	{
		newMesh.indices[i] = originalVertexIndexToNewVertexIndex[newMesh.indices[i]];
	}
	END_SCOPE();

	START_SCOPE("Merge normals - Removing obsolete vertices");
	// Removing obsolete vertices
	u32 removedVertexCount = 0;
	u32 nextDuplicateVertexIndex = duplicateVertexIndices[0];
	for (u32 i = duplicateVertexIndices[0]; i < originalMesh.vertexCount; i++)
	{
		if (i == nextDuplicateVertexIndex)
		{
			removedVertexCount++;
			if (removedVertexCount < duplicateVertexCount)
				nextDuplicateVertexIndex = duplicateVertexIndices[removedVertexCount];
		}
		else
		{
			GRASSERT_DEBUG((u64)vertices + newMesh.vertexStride * i < verticesMaxAddress);
			MemoryCopy(vertices + newMesh.vertexStride * (i - removedVertexCount), vertices + newMesh.vertexStride * i, newMesh.vertexStride);
		}
	}
	END_SCOPE();

	START_SCOPE("Merge normals - Freeing excess memory now that verts have been deduplicated");
	// Readjusting the allocation of the new mesh's vertices depending on how many duplicate verts were found
	newMesh.vertexCount = originalMesh.vertexCount - removedVertexCount;
	newMesh.vertices = Realloc(global->largeObjectAllocator, newMesh.vertices, newMesh.vertexCount * newMesh.vertexStride);
	END_SCOPE();

	START_SCOPE("Merge normals - Recalculating normals");
	// ======================================== Recalculating normals
	// Zeroing the normals
	for (u32 i = 0; i < newMesh.vertexCount; i++)
	{
		vec3* v = (vec3*)(vertices + normalOffset + originalMesh.vertexStride * i);
		*v = vec3_create(0, 0, 0);
	}

	// Accumulating cross products of all triangles connected to each vertex
	for (u32 i = 0; i < newMesh.indexCount / 3; i++)
	{
		u32 triangleStartIndex = i * 3;
		vec3 v1 = *((vec3*)(vertices + positionOffset + originalMesh.vertexStride * newMesh.indices[triangleStartIndex]));
		vec3 v2 = *((vec3*)(vertices + positionOffset + originalMesh.vertexStride * newMesh.indices[triangleStartIndex + 1]));
		vec3 v3 = *((vec3*)(vertices + positionOffset + originalMesh.vertexStride * newMesh.indices[triangleStartIndex + 2]));
		vec3 edge1 = vec3_sub_vec3(v2, v3);
		vec3 edge2 = vec3_sub_vec3(v1, v3);
		vec3 crossProduct = vec3_cross_vec3(edge1, edge2);
		vec3* n1 = (vec3*)(vertices + normalOffset + originalMesh.vertexStride * newMesh.indices[triangleStartIndex]);
		vec3* n2 = (vec3*)(vertices + normalOffset + originalMesh.vertexStride * newMesh.indices[triangleStartIndex + 1]);
		vec3* n3 = (vec3*)(vertices + normalOffset + originalMesh.vertexStride * newMesh.indices[triangleStartIndex + 2]);
		*n1 = vec3_add_vec3(*n1, crossProduct);
		*n2 = vec3_add_vec3(*n2, crossProduct);
		*n3 = vec3_add_vec3(*n3, crossProduct);
	}

	// normalizing vertex normals
	for (u32 i = 0; i < newMesh.vertexCount; i++)
	{
		vec3* newVertexNormal = (vec3*)(vertices + normalOffset + originalMesh.vertexStride * i);
		*newVertexNormal = vec3_normalize(*newVertexNormal);
	}
	END_SCOPE();

	// "Freeing" the memory from the temporary vert and indices array, because they could be quite large and this function might be run multiple times per frame
	ArenaFreeMarker(global->frameArena, marker);

	return newMesh;
}
