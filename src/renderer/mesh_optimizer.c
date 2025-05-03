#include "mesh_optimizer.h"

#include "math/lin_alg.h"

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

	// Calculating cross product for each triangle
	u32 triangleCount = originalMesh.indexCount / 3;
	vec3* triangleCrossProducts = ArenaAlignedAlloc(global->frameArena, sizeof(*triangleCrossProducts) * triangleCount, CACHE_ALIGN);
	for (u32 i = 0; i < triangleCount; i++)
	{
		u32 triangleStartIndex = i * 3;
		vec3 v1 = *((vec3*)(vertices + positionOffset + originalMesh.vertexStride * newMesh.indices[triangleStartIndex]));
		vec3 v2 = *((vec3*)(vertices + positionOffset + originalMesh.vertexStride * newMesh.indices[triangleStartIndex + 1]));
		vec3 v3 = *((vec3*)(vertices + positionOffset + originalMesh.vertexStride * newMesh.indices[triangleStartIndex + 2]));
		vec3 edge1 = vec3_sub_vec3(v2, v3);
		vec3 edge2 = vec3_sub_vec3(v1, v3);
		triangleCrossProducts[i] = vec3_cross_vec3(edge1, edge2);
	}

	// Finding indices of duplicate vertices, 
	// adding all the normals of the triangles connected to the vertices that are left over
	// making a mapping from the old vertex indices to the new vertex indices
	u32* duplicateVertexIndices = ArenaAlignedAlloc(global->frameArena, sizeof(*duplicateVertexIndices) * originalMesh.vertexCount, CACHE_ALIGN);
	u32* originalVertexIndexToNewVertexIndex = ArenaAlignedAlloc(global->frameArena, sizeof(*originalVertexIndexToNewVertexIndex) * originalMesh.vertexCount, CACHE_ALIGN);
	u32 duplicateVertexCount = 0;
	originalVertexIndexToNewVertexIndex[0] = 0;
	for (u32 i = 1; i < originalMesh.vertexCount; i++)
	{
		vec3* iPosition = (vec3*)(vertices + positionOffset + originalMesh.vertexStride * i);
		originalVertexIndexToNewVertexIndex[i] = i - duplicateVertexCount;

		for (u32 j = 0; j < i; j++)
		{
			vec3* jPosition = ((vec3*)(vertices + positionOffset + originalMesh.vertexStride * j));
			if (MemoryCompare(iPosition, jPosition, sizeof(vec3)))
			{
				duplicateVertexIndices[duplicateVertexCount] = i;
				duplicateVertexCount++;

				originalVertexIndexToNewVertexIndex[i] = originalVertexIndexToNewVertexIndex[j];

				vec3* newVertexNormalTotal = (vec3*)(vertices + normalOffset + originalMesh.vertexStride * j);
				//*newVertexNormalTotal = vec3_add_vec3(*newVertexNormalTotal, triangleCrossProducts[i / 3]);
				//vec3* oldVertexNormal =  (vec3*)(vertices + normalOffset + originalMesh.vertexStride * i);
				//newVertexNormalTotal->x += oldVertexNormal->x;
				//newVertexNormalTotal->y += oldVertexNormal->y;
				//newVertexNormalTotal->z += oldVertexNormal->z;

				break;
			}
		}
	}

	// Mapping the old indices to the new indices
	for (u32 i = 0; i < newMesh.indexCount; i++)
	{
		newMesh.indices[i] = originalVertexIndexToNewVertexIndex[newMesh.indices[i]];
	}

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

	// Readjusting the allocation of the new mesh's vertices depending on how many duplicate verts were found
	newMesh.vertexCount = originalMesh.vertexCount - removedVertexCount;
	newMesh.vertices = Realloc(global->largeObjectAllocator, newMesh.vertices, newMesh.vertexCount * newMesh.vertexStride);

	for (u32 i = 0; i < newMesh.vertexCount; i++)
	{
		vec3* v = (vec3*)(vertices + normalOffset + originalMesh.vertexStride * i);
		*v = vec3_create(0, 0, 0);
	}

	for (u32 i = 0; i < triangleCount; i++)
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

	// "Freeing" the memory from the temporary vert and indices array, because they could be quite large and this function might be run multiple times per frame
	ArenaFreeMarker(global->frameArena, marker);

	return newMesh;
}
