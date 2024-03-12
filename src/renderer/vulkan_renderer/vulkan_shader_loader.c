#include "vulkan_shader_loader.h"

#include "core/asserts.h"
#include "core/logger.h"
// TODO: make file io system so stdio doesn't have to be used
#include "vulkan_types.h"
#include <stdio.h>

#define VEC2_SIZE 8
#define VEC2_ALIGNMENT 8
#define VEC3_SIZE 12
#define VEC3_ALIGNMENT 16
#define VEC4_SIZE 16
#define VEC4_ALIGNMENT 16
#define MAT4_SIZE 64
#define MAT4_ALIGNMENT 16


static bool BracketBeforeSemicolon(const char* text)
{
    while (*text != '{' && *text != ';') text++;
    return *text == '{';
}

void GetPropertyDataFromShader(const char* filename, UniformPropertiesData* out_propertyData)
{
    // Reading in the text file
    FILE* file = fopen(filename, "r");

    if (file == NULL)
        GRASSERT_MSG(false, "Failed to open raw vertex shader file.");

    fseek(file, 0L, SEEK_END);

    u64 fileSize = ftell(file);
    char* text = AlignedAlloc(vk_state->rendererAllocator, fileSize, 64, MEM_TAG_RENDERER_SUBSYS);

    rewind(file);
    fread(text, sizeof(*text), fileSize, file);
    fclose(file);

    // Parsing the text
    char* uniformStart = nullptr;

    // Finding the uniform block
    for (u32 i = 0; i < fileSize - 100 /*minus one hundred to not check past the end of the file after finding an enter in the last line*/; i++)
    {
        if (text[i] == '\n')
        {
            if (MemoryCompare(text + i, "\nlayout(set = 1, binding = ", 27) && MemoryCompare(text + i + 28, ") uniform", 9) && BracketBeforeSemicolon(text + i + 28))
            {
                out_propertyData->bindingIndex = *(text + i + 27) - 48;

                // Setting uniform start to the actual start of the first white spaces before the actual uniform data
                uniformStart = text + i + 37;
                while (*uniformStart != '{')
                    uniformStart++;
                uniformStart++;
            }
        }
    }

    // Exiting out if there is no uniform buffer
    if (uniformStart == nullptr)
    {
        out_propertyData->propertyCount = 0;
        return;
    }

    // Counting the properties
    char* uniformStartCopy = uniformStart;
    while (*uniformStartCopy != '}') // The closing bracket would indicate the end of the uniform block
    {
        if (*uniformStartCopy == ';') // Every semicolon indicates a line that contains a property
            out_propertyData->propertyCount++;
        uniformStartCopy++;
    }

    out_propertyData->propertyStringsMemory = Alloc(vk_state->rendererAllocator, PROPERTY_MAX_NAME_LENGTH * out_propertyData->propertyCount, MEM_TAG_RENDERER_SUBSYS);
    out_propertyData->propertyNameArray = Alloc(vk_state->rendererAllocator, sizeof(*out_propertyData->propertyNameArray) * out_propertyData->propertyCount, MEM_TAG_RENDERER_SUBSYS);
    out_propertyData->propertyOffsets = Alloc(vk_state->rendererAllocator, sizeof(*out_propertyData->propertyOffsets) * out_propertyData->propertyCount, MEM_TAG_RENDERER_SUBSYS);
    out_propertyData->propertySizes = Alloc(vk_state->rendererAllocator, sizeof(*out_propertyData->propertySizes) * out_propertyData->propertyCount, MEM_TAG_RENDERER_SUBSYS);

    for (int i = 0; i < out_propertyData->propertyCount; i++)
    {
        out_propertyData->propertyNameArray[i] = out_propertyData->propertyStringsMemory + PROPERTY_MAX_NAME_LENGTH * i;
    }

    // Getting all the relevent data about each property
    u32 currentProperty = 0;
    while (*uniformStart != '}')
    {
        while (*uniformStart == ' ' || *uniformStart == '\t' || *uniformStart == '\n')
            uniformStart++;

        if (MemoryCompare(uniformStart, "mat4", 4))
        {
            // Putting uniformStart past the type and at the start of the property name
            uniformStart += 5;

            // ===== Saving information about the property
            // Making sure the matrix is properly aligned
            u32 alignmentPadding = (MAT4_ALIGNMENT - (out_propertyData->uniformBufferSize % MAT4_ALIGNMENT)) % MAT4_ALIGNMENT;
            out_propertyData->uniformBufferSize += alignmentPadding;

            out_propertyData->propertyOffsets[currentProperty] = out_propertyData->uniformBufferSize;
            out_propertyData->propertySizes[currentProperty] = MAT4_SIZE;
            out_propertyData->uniformBufferSize += MAT4_SIZE;
        }

        if (MemoryCompare(uniformStart, "vec2", 4))
        {
            // Putting uniformStart past the type and at the start of the property name
            uniformStart += 5;

            // Making sure the vector is properly aligned
            u32 alignmentPadding = (VEC2_ALIGNMENT - (out_propertyData->uniformBufferSize % VEC2_ALIGNMENT)) % VEC2_ALIGNMENT;
            out_propertyData->uniformBufferSize += alignmentPadding;

            out_propertyData->propertyOffsets[currentProperty] = out_propertyData->uniformBufferSize;
            out_propertyData->propertySizes[currentProperty] = VEC2_SIZE;
            out_propertyData->uniformBufferSize += VEC2_SIZE;
        }

        // ===== Getting the property name
        uniformStartCopy = uniformStart;
        u32 nameLength = 0;
        while (*uniformStart != ';')
        {
            nameLength++;
            uniformStart++;
        }

        MemoryCopy(out_propertyData->propertyNameArray[currentProperty], uniformStartCopy, nameLength);
        out_propertyData->propertyNameArray[currentProperty][nameLength] = '\0';

        //_DEBUG("CurrentProperty: %i, Name: %s, Size: %i, Offset: %i", currentProperty, shader->propertyNameArray[currentProperty], shader->propertySizes[currentProperty], shader->propertyOffsets[currentProperty]);

        // Preparing for the next property
        currentProperty++;

        while (*uniformStart != '\n')
            uniformStart++;
        uniformStart++;
    }

    //_DEBUG("Properties: %i, Uniform size: %i", shader->propertyCount, shader->uniformBufferSize);

    Free(vk_state->rendererAllocator, text);
}

void FreePropertyData(UniformPropertiesData* propertyData)
{
    if (propertyData->propertyCount > 0)
    {
        Free(vk_state->rendererAllocator, propertyData->propertyNameArray);
        Free(vk_state->rendererAllocator, propertyData->propertyStringsMemory);
        Free(vk_state->rendererAllocator, propertyData->propertyOffsets);
        Free(vk_state->rendererAllocator, propertyData->propertySizes);
    }
}

bool ReadFile(const char* filename, MemTag tag, char** out_data, u64* out_fileSize)
{
    FILE* file = fopen(filename, "rb");

    if (file == NULL)
    {
        _ERROR("Failed to open file");
        return false;
    }

    fseek(file, 0L, SEEK_END);

    *out_fileSize = ftell(file);
    *out_data = (char*)AlignedAlloc(vk_state->rendererAllocator, *out_fileSize, 64, tag);

    rewind(file);
    fread(*out_data, 1, *out_fileSize, file);
    fclose(file);

    return true;
}

bool CreateShaderModule(const char* filename, RendererState* state, VkShaderModule* out_shaderModule)
{
    char* fileData;
    u64 fileSize;
    ReadFile(filename, MEM_TAG_RENDERER_SUBSYS, &fileData, &fileSize);

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    // This cast from char* to u32* is possible because darray's are aligned on 64B
    createInfo.pCode = (u32*)fileData;

    if (VK_SUCCESS != vkCreateShaderModule(state->device, &createInfo, state->vkAllocator, out_shaderModule))
    {
        Free(vk_state->rendererAllocator, fileData);
        _ERROR("Shader module creation failed");
        return false;
    }

    Free(vk_state->rendererAllocator, fileData);

    return true;
}
