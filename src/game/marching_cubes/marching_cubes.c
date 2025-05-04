#include "marching_cubes.h"
#include "containers/darray.h"
#include "core/asserts.h"
#include "core/meminc.h"
#include "marching_cubes_lut.h"
#include "math/lin_alg.h"
#include "renderer/renderer.h"
#include "core/engine.h"

#define INITIAL_VERT_RESERVATION 1000


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


MeshData MarchingCubesGenerateMesh(f32* densityMap, u32 densityMapWidth, u32 densityMapHeight, u32 densityMapDepth)
{
	ArenaMarker marker = ArenaGetMarker(global->frameArena);

	u32 reserved = INITIAL_VERT_RESERVATION;
	VertexT2* vertArray = ArenaAlloc(global->frameArena, sizeof(*vertArray) * INITIAL_VERT_RESERVATION);
    u32 numberOfVertices = 0;

	u32 densityMapHeightTimesDepth = densityMapHeight * densityMapDepth;

    // Looping over every cube in the density map
    for (u32 x = 0; x < densityMapWidth - 1; x++)
    {
        for (u32 y = 0; y < densityMapHeight - 1; y++)
        {
            for (u32 z = 0; z < densityMapDepth - 1; z++)
            {
                // Applying the marching cubes algorithm to this cube to determine what triangles are needed (if any).

                // Putting the values of the current cube in a small array because in the big array they are not contiguous in memory
                f32 cubeValues[8];

                cubeValues[0] = GetDensityValueRaw(densityMap, densityMapHeightTimesDepth, densityMapDepth, x, y, z);
                cubeValues[1] = GetDensityValueRaw(densityMap, densityMapHeightTimesDepth, densityMapDepth, x + 1, y, z);
                cubeValues[2] = GetDensityValueRaw(densityMap, densityMapHeightTimesDepth, densityMapDepth, x + 1, y, z + 1);
                cubeValues[3] = GetDensityValueRaw(densityMap, densityMapHeightTimesDepth, densityMapDepth, x, y, z + 1);
                cubeValues[4] = GetDensityValueRaw(densityMap, densityMapHeightTimesDepth, densityMapDepth, x, y + 1, z);
                cubeValues[5] = GetDensityValueRaw(densityMap, densityMapHeightTimesDepth, densityMapDepth, x + 1, y + 1, z);
                cubeValues[6] = GetDensityValueRaw(densityMap, densityMapHeightTimesDepth, densityMapDepth, x + 1, y + 1, z + 1);
                cubeValues[7] = GetDensityValueRaw(densityMap, densityMapHeightTimesDepth, densityMapDepth, x, y + 1, z + 1);

                u32 cubeIndex = 0;

                // Determining which corners are in the contour of the density function and which are outside of it
                // and storing the booleans in the first 8 bits of an int (cubeIndex)
                for (int i = 0; i < 8; i++)
                {
                    if (cubeValues[i] < 0)
                        cubeIndex += 1 << i;
                }

                // If the cube is fully inside or fully outside of the contour, we skip this if block
                if (!(cubeIndex == 0 || cubeIndex == 255))
                {
                    // Looping through all the vertices for all the triangles that are required for this cube based on which corners are inside or outside of the contour.
                    // We use a lookup table with a row for every possible configuration of corners that are inside/outside of the contour (apart from the configurations where every corner is inside or every corner is outside).
                    // Each row tells you which triangles are needed by giving three edges that need to be connected and in what order (for normals to be correct).
                    for (i32 i = 0; triTable[cubeIndex][i] != -1; i++)
                    {
                        // Getting the edge that the current vertex needs to be on by first getting its index and then using a lookup table
                        // to get the corresponding position of the center of the edge relative to the origin of the cube (which is in one of the corners not the center).
                        i32 edgeIndex = triTable[cubeIndex][i];
                        vertArray[numberOfVertices].position = edgeIndexToPositionTable[edgeIndex];

                        // Interpolating the vertex position based on the two density points connected to the edge that this vertex is on
                        f32 value1 = cubeValues[edgeToCornerTable[edgeIndex][0]];
                        f32 value2 = cubeValues[edgeToCornerTable[edgeIndex][1]] - value1;
                        f32 surfaceLevel = -value1;
                        surfaceLevel /= value2;

                        // The vertex only gets interpolated along one direction so we need to check which dimension of the vert position needs to be changed to the interpolated value
                        if (vertArray[numberOfVertices].position.x == 0.5f)
							vertArray[numberOfVertices].position.x = surfaceLevel;
                        if (vertArray[numberOfVertices].position.y == 0.5f)
							vertArray[numberOfVertices].position.y = surfaceLevel;
                        if (vertArray[numberOfVertices].position.z == 0.5f)
							vertArray[numberOfVertices].position.z = surfaceLevel;

                        // Calculating the vertex position relative to the mesh origin rather than the cube origin
                        vertArray[numberOfVertices].position.x += x;
                        vertArray[numberOfVertices].position.y += y;
                        vertArray[numberOfVertices].position.z += z;

                        // Adding the vertex
                        numberOfVertices++;
						if (numberOfVertices >= reserved)
						{
							reserved += INITIAL_VERT_RESERVATION;
							ArenaAlloc(global->frameArena, sizeof(*vertArray) * INITIAL_VERT_RESERVATION);
						}

                        // If a triangle was completed this loop calculate and set the normal for all verts of that triangle
                        if (i % 3 == 2)
                        {
                            // Taking the cross product of two of the edges of the triangle to calculate the normal
                            // (because the cross product calculates a vector that is orthogonal to the two vectors that are suplied this vector wil always be the normal of a triangle,
                            // it points to the ouside of the triangle as long as we supply the correct edges)
                            vec3 edgeA = vec3_sub_vec3(vertArray[numberOfVertices - 2].position, vertArray[numberOfVertices - 1].position);
                            vec3 edgeB = vec3_sub_vec3(vertArray[numberOfVertices - 3].position, vertArray[numberOfVertices - 1].position);
                            vec3 normal = vec3_cross_vec3(edgeA, edgeB);
							normal = vec3_normalize(normal);

                            // Setting the normal for the three most recently added verts
                            vertArray[numberOfVertices - 1].normal = normal;
                            vertArray[numberOfVertices - 2].normal = normal;
                            vertArray[numberOfVertices - 3].normal = normal;
                        }
                    }
                }
            }
        }
    }

	GRASSERT_MSG(numberOfVertices > 0, "Marching cubes density function produced no vertices");
	
	// Creating the index buffer
    u32* indices = AlignedAlloc(global->largeObjectAllocator, sizeof(*indices) * numberOfVertices, CACHE_ALIGN);

    for (int i = 0; i < numberOfVertices; i++)
    {
        indices[i] = i;
    }

	// Copying the vertex buffer to a permanent allocation
	VertexT2* vertices = AlignedAlloc(global->largeObjectAllocator, sizeof(*vertices) * numberOfVertices, CACHE_ALIGN);
	MemoryCopy(vertices, vertArray, sizeof(*vertices) * numberOfVertices);

	MeshData meshData = {};
	meshData.vertices = vertices;
	meshData.vertexCount = numberOfVertices;
	meshData.vertexStride = sizeof(*vertices);
	meshData.indices = indices;
	meshData.indexCount = numberOfVertices;

	// "Freeing" the memory from the temporary vert array, because they could be quite large and this function might be run multiple times per frame
	ArenaFreeMarker(global->frameArena, marker);

	return meshData;
}


