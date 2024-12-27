#include "marching_cubes.h"
#include "containers/darray.h"
#include "core/asserts.h"
#include "core/meminc.h"
#include "marching_cubes_lut.h"
#include "math/lin_alg.h"
#include "renderer/renderer.h"

#define DENSITY_MAP_SIZE 100
#define VERTICES_START_CAPACITY 100

typedef struct MCVert
{
	vec3 pos;
	vec3 normal;
} MCVert;

typedef struct MCData
{
    f32* densityMap;
    u32 densityMapWidth;
    u32 densityMapHeigth;
    u32 densityMapDepth;
    u32 densityMapValueCount;
	VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
} MCData;

static MCData* mcdata = nullptr;

static inline f32* GetDensityValueRef(u32 x, u32 y, u32 z)
{
    GRASSERT_DEBUG(mcdata);
    GRASSERT_DEBUG(mcdata->densityMap);

    return &mcdata->densityMap[x * mcdata->densityMapHeigth * mcdata->densityMapDepth + y * mcdata->densityMapDepth + z];
}

static inline f32 GetDensityValueRaw(u32 x, u32 y, u32 z)
{
    GRASSERT_DEBUG(mcdata);
    GRASSERT_DEBUG(mcdata->densityMap);

    return mcdata->densityMap[x * mcdata->densityMapHeigth * mcdata->densityMapDepth + y * mcdata->densityMapDepth + z];
}

void MCGenerateDensityMap()
{
    mcdata = Alloc(GetGlobalAllocator(), sizeof(*mcdata), MEM_TAG_TEST);

    mcdata->densityMapWidth = DENSITY_MAP_SIZE;
    mcdata->densityMapHeigth = DENSITY_MAP_SIZE;
    mcdata->densityMapDepth = DENSITY_MAP_SIZE;
    mcdata->densityMapValueCount = mcdata->densityMapWidth * mcdata->densityMapHeigth * mcdata->densityMapDepth;

    mcdata->densityMap = Alloc(GetGlobalAllocator(), mcdata->densityMapValueCount * sizeof(*mcdata->densityMap), MEM_TAG_TEST);

	vec3 sphere1Center = vec3_from_float(DENSITY_MAP_SIZE / 2);
	f32 sphere1Radius = 20;
	vec3 sphere2Center = vec3_from_float(DENSITY_MAP_SIZE / 2);
	sphere2Center.x -= 13;
	f32 sphere2Radius = 8;

    for (u32 x = 0; x < mcdata->densityMapWidth; x++)
    {
        for (u32 y = 0; y < mcdata->densityMapHeigth; y++)
        {
            for (u32 z = 0; z < mcdata->densityMapDepth; z++)
            {
				f32 sphereValue = vec3_distance(vec3_create(x, y, z), sphere1Center) - sphere1Radius;
				f32 sphereHoleValue = vec3_distance(vec3_create(x, y, z), sphere2Center) - sphere2Radius;
				if (sphereHoleValue >= 0)
					sphereHoleValue = 0;
				else if (sphereHoleValue <= -1)
					sphereValue = 0;
				else
				{
					f32 t = -sphereHoleValue;
					sphereValue = sphereValue + (sphereHoleValue - sphereValue) * t;
					sphereHoleValue = 0;
				}
				*GetDensityValueRef(x, y, z) = sphereValue - sphereHoleValue;
            }
        }
    }
}

void MCGenerateMesh()
{
    MCVert* verticesDarray = DarrayCreate(sizeof(*verticesDarray), VERTICES_START_CAPACITY, GetGlobalAllocator(), MEM_TAG_TEST);
    u32 numberOfVertices = 0;

    for (u32 x = 0; x < mcdata->densityMapWidth - 1; x++)
    {
        for (u32 y = 0; y < mcdata->densityMapHeigth - 1; y++)
        {
            for (u32 z = 0; z < mcdata->densityMapDepth - 1; z++)
            {
                f32 cubeValues[8];

                cubeValues[0] = GetDensityValueRaw(x, y, z);
                cubeValues[1] = GetDensityValueRaw(x + 1, y, z);
                cubeValues[2] = GetDensityValueRaw(x + 1, y, z + 1);
                cubeValues[3] = GetDensityValueRaw(x, y, z + 1);
                cubeValues[4] = GetDensityValueRaw(x, y + 1, z);
                cubeValues[5] = GetDensityValueRaw(x + 1, y + 1, z);
                cubeValues[6] = GetDensityValueRaw(x + 1, y + 1, z + 1);
                cubeValues[7] = GetDensityValueRaw(x, y + 1, z + 1);

                u32 cubeIndex = 0;

                for (int i = 0; i < 8; i++)
                {
                    if (cubeValues[i] < 0)
                        cubeIndex += 1 << i;
                }

                if (!(cubeIndex == 0 || cubeIndex == 255))
                {
                    for (i32 i = 0; triTable[cubeIndex][i] != -1; i++)
                    {
                        i32 edgeIndex = triTable[cubeIndex][i];
						MCVert vert = {};
                        vert.pos = edgeIndexToPositionTable[edgeIndex];

						// Interpolating the vertex position based on the two density points connected to the edge that this vertex is on
						f32 value1 = cubeValues[edgeToCornerTable[edgeIndex][0]];
                		f32 value2 = cubeValues[edgeToCornerTable[edgeIndex][1]] - value1;
                		f32 surfaceLevel = -value1;
                		surfaceLevel /= value2;

						if (vert.pos.x == 0.5f)
                    		vert.pos.x = surfaceLevel;
                		if (vert.pos.y == 0.5f)
                    		vert.pos.y = surfaceLevel;
                		if (vert.pos.z == 0.5f)
                    		vert.pos.z = surfaceLevel;

                        vert.pos.x += x;
                        vert.pos.y += y;
                        vert.pos.z += z;

                        verticesDarray = DarrayPushback(verticesDarray, &vert);
                        numberOfVertices++;

						// If a triangle was completed this loop calculate and set the normal for all verts of that triangle
						if (i % 3 == 2)
						{
							vec3 edgeA = vec3_sub_vec3(verticesDarray[numberOfVertices - 2].pos, verticesDarray[numberOfVertices - 1].pos);
							vec3 edgeB = vec3_sub_vec3(verticesDarray[numberOfVertices - 3].pos, verticesDarray[numberOfVertices - 1].pos);
							vec3 normal = vec3_cross_vec3(edgeA, edgeB);

							verticesDarray[numberOfVertices - 1].normal = normal;
							verticesDarray[numberOfVertices - 2].normal = normal;
							verticesDarray[numberOfVertices - 3].normal = normal;
						}
                    }
                }
            }
        }
    }

    u32* triangles = Alloc(GetGlobalAllocator(), sizeof(*triangles) * numberOfVertices, MEM_TAG_TEST);

    for (int i = 0; i < numberOfVertices; i++)
    {
        triangles[i] = i;
    }

	// Generate mesh
	mcdata->vertexBuffer = VertexBufferCreate(verticesDarray, sizeof(*verticesDarray) * numberOfVertices);
	mcdata->indexBuffer = IndexBufferCreate(triangles, numberOfVertices);
}

void MCRenderWorld()
{
	mat4 model = mat4_identity();
	Draw(1, &mcdata->vertexBuffer, mcdata->indexBuffer, &model, 1);
}

void MCDestroyMeshAndDensityMap()
{
	IndexBufferDestroy(mcdata->indexBuffer);
	VertexBufferDestroy(mcdata->vertexBuffer);

    Free(GetGlobalAllocator(), mcdata->densityMap);
    Free(GetGlobalAllocator(), mcdata);
}
