#include "obj_loader.h"

#include "containers/darray.h"
#include "core/logger.h"
#include "math/math_types.h"
#include <stdio.h>
#include <stdlib.h>

#define DARRAY_START_CAPACITY 100


typedef struct ObjVertex
{
    vec3 position;
    vec3 normal;
    vec2 uvCoord;
} ObjVertex;

typedef struct ObjVertexIndices
{
    // 0 = position, 1 = uvCoord, 2 = normal;
    i32 indices[3];
} ObjVertexIndices;

DEFINE_DARRAY_TYPE(u32);
DEFINE_DARRAY_TYPE(ObjVertex);
DEFINE_DARRAY_TYPE(ObjVertexIndices);

bool LoadObj(const char* filename, VertexBuffer* out_vb, IndexBuffer* out_ib, bool flipWindingOrder)
{
    //  ================================== Opening the obj file and loading the entire text into memory =========================================
    FILE* file = fopen(filename, "r");

    if (file == NULL)
    {
        _ERROR("Failed to open file");
        return false;
    }

    fseek(file, 0L, SEEK_END);

    u64 fileSize = ftell(file);
    char* text = AlignedAlloc(GetGlobalAllocator(), fileSize, 64, MEM_TAG_RENDERER_SUBSYS);

    rewind(file);
    fread(text, sizeof(*text), fileSize, file);
    fclose(file);

    // ====================================== Creating index arrays of the starting indices of all the v, vn, vt, and f lines =============================================
    u32Darray* positionIndices = u32DarrayCreate(DARRAY_START_CAPACITY, GetGlobalAllocator());
    u32Darray* normalIndices = u32DarrayCreate(DARRAY_START_CAPACITY, GetGlobalAllocator());
    u32Darray* uvCoordIndices = u32DarrayCreate(DARRAY_START_CAPACITY, GetGlobalAllocator());
    u32Darray* faceIndices = u32DarrayCreate(DARRAY_START_CAPACITY, GetGlobalAllocator());

    // Adding a null element in the 0th position of these arrays because obj indices start at 1
    u32 nullElement = 0;
    u32DarrayPushback(positionIndices, &nullElement);
    u32DarrayPushback(normalIndices, &nullElement);
    u32DarrayPushback(uvCoordIndices, &nullElement);

    for (u32 i = 0; i < fileSize - 3 /*minus 3 to stop 3 characters short of the end of the file*/; i++)
    {
        if (text[i] == '\n')
        {
            if (MemoryCompare(text + i, "\nv ", 3))
            {
                u32DarrayPushback(positionIndices, &i);
            }

            if (MemoryCompare(text + i, "\nvt ", 4))
            {
                u32DarrayPushback(uvCoordIndices, &i);
            }

            if (MemoryCompare(text + i, "\nvn ", 4))
            {
                u32DarrayPushback(normalIndices, &i);
            }

            if (MemoryCompare(text + i, "\nf ", 3))
            {
                u32DarrayPushback(faceIndices, &i);
            }
        }
    }

    // ============================== Creating a list of all the unique vertices and creating the final indices array ===============================================
    // TODO: replace with map/dictionary for quick lookups
    ObjVertexIndicesDarray* uniqueVertices = ObjVertexIndicesDarrayCreate(DARRAY_START_CAPACITY, GetGlobalAllocator());

    u32Darray* objIndices = u32DarrayCreate(DARRAY_START_CAPACITY, GetGlobalAllocator());

    for (int i = 0; i < faceIndices->size; i++)
    {
        char* face = text + faceIndices->data[i];

        // Looping through face vertices
        for (int j = 0; j < 3; j++)
        {
            ObjVertexIndices vertexIndices = {};

            // Going to the first character after the first space
            while (*face != ' ')
                face += 1;
            face += 1;

            u32 vertexComponentIndex = 0;

            while (*face != ' ' && *face != '\n')
            {
                if (*face == '/')
                {
                    vertexComponentIndex++;
                }
                else // if the current character is a number
                {
                    vertexIndices.indices[vertexComponentIndex] = vertexIndices.indices[vertexComponentIndex] * 10 + (*face - 48);
                }
                face++;
            }

            // Checking if the vertex already exists and getting its index if it does
            u32 vertexIndex = UINT32_MAX;

            for (int k = 0; k < uniqueVertices->size; k++)
            {
                if (MemoryCompare(uniqueVertices->data + k, &vertexIndices, sizeof(vertexIndices)))
                {
                    vertexIndex = k;
                    break;
                }
            }

            // Adding the vertex to the array if it doesn't exist yet
            if (vertexIndex == UINT32_MAX)
            {
                vertexIndex = uniqueVertices->size;
                ObjVertexIndicesDarrayPushback(uniqueVertices, &vertexIndices);
            }

            // Adding the index to the indices array
            u32DarrayPushback(objIndices, &vertexIndex);
        }
    }

    // ============================== Creating the final vertices array ===============================================
    ObjVertexDarray* objVerticesDarray = ObjVertexDarrayCreateWithSize(uniqueVertices->size, GetGlobalAllocator());

    for (int i = 0; i < uniqueVertices->size; i++)
    {
        char* positionString = text + positionIndices->data[uniqueVertices->data[i].indices[0]];
        char* normalString = text + normalIndices->data[uniqueVertices->data[i].indices[2]];
        char* uvCoordString = text + uvCoordIndices->data[uniqueVertices->data[i].indices[1]];

        // Positions
        positionString += 3;
        char* positionXString = positionString;
        while (*positionString != ' ' && *positionString != '\0') positionString++;
        *positionString = '\0';
        positionString++;
        char* positionYString = positionString;
        while (*positionString != ' ' && *positionString != '\0') positionString++;
        *positionString = '\0';
        positionString++;
        char* positionZString = positionString;
        while (*positionString != '\n' && *positionString != '\0') positionString++;
        *positionString = '\0';

        objVerticesDarray->data[i].position.x = atof(positionXString);
        objVerticesDarray->data[i].position.y = atof(positionYString);
        objVerticesDarray->data[i].position.z = atof(positionZString);

        // Normals
        normalString += 4;
        char* normalXString = normalString;
        while (*normalString != ' ' && *normalString != '\0') normalString++;
        *normalString = '\0';
        normalString++;
        char* normalYString = normalString;
        while (*normalString != ' ' && *normalString != '\0') normalString++;
        *normalString = '\0';
        normalString++;
        char* normalZString = normalString;
        while (*normalString != '\n' && *normalString != '\0') normalString++;
        *normalString = '\0';

        objVerticesDarray->data[i].normal.x = atof(normalXString);
        objVerticesDarray->data[i].normal.y = atof(normalYString);
        objVerticesDarray->data[i].normal.z = atof(normalZString);

        // uv coords
        uvCoordString += 4;
        char* uvCoordXString = uvCoordString;
        while (*uvCoordString != ' ' && *uvCoordString != '\0') uvCoordString++;
        *uvCoordString = '\0';
        uvCoordString++;
        char* uvCoordYString = uvCoordString;
        while (*uvCoordString != '\n' && *uvCoordString != '\0') uvCoordString++;
        *uvCoordString = '\0';

        objVerticesDarray->data[i].uvCoord.x = atof(uvCoordXString);
        objVerticesDarray->data[i].uvCoord.y = atof(uvCoordYString);
    }

    // ============================== Flipping winding order if necessary ===============================================
    if (flipWindingOrder)
    {
        for (int i = 0; i < objIndices->size - 1; i += 3)
        {
            u32 temp = objIndices->data[i];
            objIndices->data[i] = objIndices->data[i + 2];
            objIndices->data[i + 2] = temp;
        }
    }

    // ============================== Uploading the vertices and indices to the GPU ===============================================
    *out_vb = VertexBufferCreate(objVerticesDarray->data, objVerticesDarray->stride * objVerticesDarray->size);
    *out_ib = IndexBufferCreate(objIndices->data, objIndices->size);

    // ============================== Cleanup ===============================================
    DarrayDestroy(objVerticesDarray);
    DarrayDestroy(objIndices);

    DarrayDestroy(uniqueVertices);

    DarrayDestroy(positionIndices);
    DarrayDestroy(normalIndices);
    DarrayDestroy(uvCoordIndices);
    DarrayDestroy(faceIndices);

    Free(GetGlobalAllocator(), text);

    return true;
}
