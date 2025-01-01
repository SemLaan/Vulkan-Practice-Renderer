#include "marching_cubes.h"
#include "containers/darray.h"
#include "core/asserts.h"
#include "core/meminc.h"
#include "marching_cubes_lut.h"
#include "marching_cubes_density_functions.h"
#include "math/lin_alg.h"
#include "renderer/renderer.h"

#define DENSITY_MAP_SIZE 100        // width, heigth and depth of the density map
#define VERTICES_START_CAPACITY 100 // Start capacity of the array of vertices

/// @brief Vertex struct for the vertices of the marching cubes mesh
typedef struct MCVert
{
    vec3 pos;    // Position
    vec3 normal; // Normal
} MCVert;

/// @brief Struct to store all the data the marching cubes algorithm generates
typedef struct MCData
{
    f32* densityMap;      // 3d array of density values
    u32 densityMapWidth;  //
    u32 densityMapHeigth; //
    u32 densityMapDepth;  //
    u32 densityMapHeightTimesDepth;
    u32 densityMapValueCount;  // Total number of values in the densityMap
    MCVert* verticesDarray;    // Darray with all the vertices
    u32* indices;              // Array with all the indices
    VertexBuffer vertexBuffer; // Vertex buffer for the mesh that will be generated by marching cubes
    IndexBuffer indexBuffer;   // Index buffer for the mesh that will be generated by marching cubes
} MCData;

// Marching cubes data
static MCData* mcdata = nullptr;


void MCGenerateDensityMap()
{
    mcdata = Alloc(GetGlobalAllocator(), sizeof(*mcdata), MEM_TAG_TEST);

    mcdata->densityMapWidth = DENSITY_MAP_SIZE;
    mcdata->densityMapHeigth = DENSITY_MAP_SIZE;
    mcdata->densityMapDepth = DENSITY_MAP_SIZE;
    mcdata->densityMapHeightTimesDepth = mcdata->densityMapHeigth * mcdata->densityMapDepth;
    mcdata->densityMapValueCount = mcdata->densityMapWidth * mcdata->densityMapHeigth * mcdata->densityMapDepth;

    // Allocating the densityMap
    mcdata->densityMap = Alloc(GetGlobalAllocator(), mcdata->densityMapValueCount * sizeof(*mcdata->densityMap), MEM_TAG_TEST);

    // ======================== Calculating the density values and filling in the density map
	//DensityFuncSphereHole(mcdata->densityMap, mcdata->densityMapWidth, mcdata->densityMapHeigth, mcdata->densityMapDepth);
	DensityFuncRandomSpheres(mcdata->densityMap, mcdata->densityMapWidth, mcdata->densityMapHeigth, mcdata->densityMapDepth);
    

    // Bluring the density map
    u32 kernelSize = 3;
    u32 kernelSizeMinusOne = kernelSize - 1;
    u32 kernelSizeSquared = kernelSize * kernelSize;
    u32 kernelSizeCubed = kernelSize * kernelSize * kernelSize;

    f32* kernel = Alloc(GetGlobalAllocator(), kernelSizeCubed * sizeof(*kernel), MEM_TAG_TEST);
    f32 kernelTotal = 0;

    vec3 kernelCenter = vec3_from_float(kernelSizeMinusOne / 2);
    f32 maximumEuclideanDistanceSquaredPlusOne = 1 + vec3_distance_squared(vec3_from_float(0), kernelCenter);

    for (u32 x = 0; x < kernelSize; x++)
    {
        for (u32 y = 0; y < kernelSize; y++)
        {
            for (u32 z = 0; z < kernelSize; z++)
            {
                // ========= Kernel version one (kernel value is the maximum manhattan distance from the center of the kernel squared minus the manhattan distance of the current element from the center of the kernel squared)
                // u32 value = 1 << ((x < kernelSizeMinusOne - x ? x : kernelSizeMinusOne - x) + (y < kernelSizeMinusOne - y ? y : kernelSizeMinusOne - y) + (z < kernelSizeMinusOne - z ? z : kernelSizeMinusOne - z));

                // ========= Kernel version two (kernel value is one plus the maximum euclidean distance from the center of the kernel squared minus the euclidean distance of the current element from the center of the kernel squared)
                f32 value = maximumEuclideanDistanceSquaredPlusOne - vec3_distance_squared(kernelCenter, vec3_create(x, y, z));

                kernel[x * kernelSizeSquared + y * kernelSize + z] = value;
                kernelTotal += value;
            }
        }
    }

    u32 padding = (kernelSize - 1) / 2;

    u32 iterations = 0;

    f32* nonBlurredDensityMap = mcdata->densityMap;
	if (iterations != 0)
    	mcdata->densityMap = Alloc(GetGlobalAllocator(), mcdata->densityMapValueCount * sizeof(*mcdata->densityMap), MEM_TAG_TEST);

    // Looping over every kernel sized area in the density map
    for (u32 i = 0; i < iterations; i++)
    {
        for (u32 x = padding; x < mcdata->densityMapWidth - padding; x++)
        {
            for (u32 y = padding; y < mcdata->densityMapHeigth - padding; y++)
            {
                for (u32 z = padding; z < mcdata->densityMapDepth - padding; z++)
                {
                    f32 sum = 0;

                    // Looping over the kernel and multiplying each element of the kernel with its respective element of the kernel sized area in the density map
                    for (u32 kx = 0; kx < kernelSize; kx++)
                    {
                        for (u32 ky = 0; ky < kernelSize; ky++)
                        {
                            for (u32 kz = 0; kz < kernelSize; kz++)
                            {
                                //                                        [                          x                          ]   [                    y                     ]   [       z        ]
                                f32 preBlurDensity = nonBlurredDensityMap[(x + kx - padding) * mcdata->densityMapHeightTimesDepth + (y + ky - padding) * mcdata->densityMapDepth + (z + kz - padding)];
                                sum += preBlurDensity * kernel[kx * kernelSizeSquared + ky * kernelSize + kz];
                            }
                        }
                    }

                    *GetDensityValueRef(mcdata->densityMap, mcdata->densityMapHeightTimesDepth, mcdata->densityMapDepth, x, y, z) = sum / kernelTotal;
                }
            }
        }

        // Switching the nonBlurredDensityMap and the density map if there is another blur iteration
		// The data in the densityMap (after the switch) doesn't have to be changed because it will be entirely overwritten and not read.
		// Because during the blurring process only the nonBlurredDensity map gets read, which has just gone through a blur iteration.
        if (i != iterations - 1)
        {
            f32* temp = nonBlurredDensityMap;
            nonBlurredDensityMap = mcdata->densityMap;
            mcdata->densityMap = temp;
        }
    }

	if (iterations != 0)
	    Free(GetGlobalAllocator(), nonBlurredDensityMap);
    Free(GetGlobalAllocator(), kernel);
}

void MCGenerateMesh()
{
    // Creating a dynamic array for the vertices because we don't know how many verts or tris we will need
    MCVert* verticesDarray = DarrayCreate(sizeof(*verticesDarray), VERTICES_START_CAPACITY, GetGlobalAllocator(), MEM_TAG_TEST);
    u32 numberOfVertices = 0;

    // Looping over every cube in the density map
    for (u32 x = 0; x < mcdata->densityMapWidth - 1; x++)
    {
        for (u32 y = 0; y < mcdata->densityMapHeigth - 1; y++)
        {
            for (u32 z = 0; z < mcdata->densityMapDepth - 1; z++)
            {
                // Applying the marching cubes algorithm to this cube to determine what triangles are needed (if any).

                // Putting the values of the current cube in a small array because in the big array they are not contiguous in memory
                f32 cubeValues[8];

                cubeValues[0] = GetDensityValueRaw(mcdata->densityMap, mcdata->densityMapHeightTimesDepth, mcdata->densityMapDepth, x, y, z);
                cubeValues[1] = GetDensityValueRaw(mcdata->densityMap, mcdata->densityMapHeightTimesDepth, mcdata->densityMapDepth, x + 1, y, z);
                cubeValues[2] = GetDensityValueRaw(mcdata->densityMap, mcdata->densityMapHeightTimesDepth, mcdata->densityMapDepth, x + 1, y, z + 1);
                cubeValues[3] = GetDensityValueRaw(mcdata->densityMap, mcdata->densityMapHeightTimesDepth, mcdata->densityMapDepth, x, y, z + 1);
                cubeValues[4] = GetDensityValueRaw(mcdata->densityMap, mcdata->densityMapHeightTimesDepth, mcdata->densityMapDepth, x, y + 1, z);
                cubeValues[5] = GetDensityValueRaw(mcdata->densityMap, mcdata->densityMapHeightTimesDepth, mcdata->densityMapDepth, x + 1, y + 1, z);
                cubeValues[6] = GetDensityValueRaw(mcdata->densityMap, mcdata->densityMapHeightTimesDepth, mcdata->densityMapDepth, x + 1, y + 1, z + 1);
                cubeValues[7] = GetDensityValueRaw(mcdata->densityMap, mcdata->densityMapHeightTimesDepth, mcdata->densityMapDepth, x, y + 1, z + 1);

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
                        MCVert vert = {};
                        vert.pos = edgeIndexToPositionTable[edgeIndex];

                        // Interpolating the vertex position based on the two density points connected to the edge that this vertex is on
                        f32 value1 = cubeValues[edgeToCornerTable[edgeIndex][0]];
                        f32 value2 = cubeValues[edgeToCornerTable[edgeIndex][1]] - value1;
                        f32 surfaceLevel = -value1;
                        surfaceLevel /= value2;

                        // The vertex only gets interpolated along one direction so we need to check which dimension of the vert position needs to be changed to the interpolated value
                        if (vert.pos.x == 0.5f)
                            vert.pos.x = surfaceLevel;
                        if (vert.pos.y == 0.5f)
                            vert.pos.y = surfaceLevel;
                        if (vert.pos.z == 0.5f)
                            vert.pos.z = surfaceLevel;

                        // Calculating the vertex position relative to the mesh origin rather than the cube origin
                        vert.pos.x += x;
                        vert.pos.y += y;
                        vert.pos.z += z;

                        // Adding the vertex
                        verticesDarray = DarrayPushback(verticesDarray, &vert);
                        numberOfVertices++;

                        // If a triangle was completed this loop calculate and set the normal for all verts of that triangle
                        if (i % 3 == 2)
                        {
                            // Taking the cross product of two of the edges of the triangle to calculate the normal
                            // (because the cross product calculates a vector that is orthogonal to the two vectors that are suplied this vector wil always be the normal of a triangle,
                            // it points to the ouside of the triangle as long as we supply the correct edges)
                            vec3 edgeA = vec3_sub_vec3(verticesDarray[numberOfVertices - 2].pos, verticesDarray[numberOfVertices - 1].pos);
                            vec3 edgeB = vec3_sub_vec3(verticesDarray[numberOfVertices - 3].pos, verticesDarray[numberOfVertices - 1].pos);
                            vec3 normal = vec3_cross_vec3(edgeA, edgeB);

                            // Setting the normal for the three most recently added verts
                            verticesDarray[numberOfVertices - 1].normal = normal;
                            verticesDarray[numberOfVertices - 2].normal = normal;
                            verticesDarray[numberOfVertices - 3].normal = normal;
                        }
                    }
                }
            }
        }
    }

    // Making the index buffer
    u32* indices = Alloc(GetGlobalAllocator(), sizeof(*indices) * numberOfVertices, MEM_TAG_TEST);

    for (int i = 0; i < numberOfVertices; i++)
    {
        indices[i] = i;
    }

	GRASSERT_MSG(numberOfVertices > 0, "Marching cubes density function produced no vertices");

    mcdata->verticesDarray = verticesDarray;
    mcdata->indices = indices;

    // Generate mesh
    mcdata->vertexBuffer = VertexBufferCreate(verticesDarray, sizeof(*verticesDarray) * numberOfVertices);
    mcdata->indexBuffer = IndexBufferCreate(indices, numberOfVertices);
}

void MCRenderWorld()
{
    mat4 model = mat4_identity();
    Draw(1, &mcdata->vertexBuffer, mcdata->indexBuffer, &model, 1);
}

void MCDestroyMeshAndDensityMap()
{
    DarrayDestroy(mcdata->verticesDarray);
    Free(GetGlobalAllocator(), mcdata->indices);

    IndexBufferDestroy(mcdata->indexBuffer);
    VertexBufferDestroy(mcdata->vertexBuffer);

    Free(GetGlobalAllocator(), mcdata->densityMap);
    Free(GetGlobalAllocator(), mcdata);
}
