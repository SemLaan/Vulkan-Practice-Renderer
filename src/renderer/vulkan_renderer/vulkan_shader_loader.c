#include "vulkan_shader_loader.h"

#include "core/asserts.h"
#include "core/logger.h"
// TODO: make file io system so stdio doesn't have to be used
#include "vulkan_types.h"
#include <stdio.h>

#define SCALAR_ALIGNMENT 4
#define SCALAR_SIZE 4
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
    while (*text != '{' && *text != ';')
        text++;
    return *text == '{';
}

void GetUniformDataFromShader(const char* filename, UniformPropertiesData* ref_propertyData, UniformTexturesData* ref_textureData)
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

    // ============================================== Getting uniform property data ============================================================
    {
        // Parsing the text
        char* uniformStart = nullptr;


#define LAYOUT_TEXT "\nlayout(BIND "
#define LAYOUT_TEXT_SIZE 14
#define UNIFORM_TEXT ") uniform"
#define UNIFORM_TEXT_SIZE 10
        // Finding the uniform block
        for (u32 i = 0; i < fileSize - 100 /*minus one hundred to not check past the end of the file after finding an enter in the last line*/; i++)
        {
            if (text[i] == '\n')
            {
                if (MemoryCompare(text + i, LAYOUT_TEXT, LAYOUT_TEXT_SIZE - 1) && MemoryCompare(text + i + LAYOUT_TEXT_SIZE, UNIFORM_TEXT, UNIFORM_TEXT_SIZE - 1) && BracketBeforeSemicolon(text + i + LAYOUT_TEXT_SIZE))
                {
                    ref_propertyData->bindingIndex = *(text + i + LAYOUT_TEXT_SIZE - 1) - /*going from char to int*/48;

                    // Setting uniform start to the actual start of the first white spaces before the actual uniform data
                    uniformStart = text + i + LAYOUT_TEXT_SIZE + UNIFORM_TEXT_SIZE - 1;
                    while (*uniformStart != '{')
                        uniformStart++;
                    uniformStart++;
                }
            }
        }

        // If this shader has no uniform buffer
        if (uniformStart == nullptr)
        {
            ref_propertyData->propertyCount = 0;
            ref_propertyData->uniformBufferSize = 0;
        }
        else // If this shader does have a uniform buffer
        {
            // Counting the properties
            char* uniformStartCopy = uniformStart;
            while (*uniformStartCopy != '}') // The closing bracket would indicate the end of the uniform block
            {
                if (*uniformStartCopy == ';') // Every semicolon indicates a line that contains a property
                    ref_propertyData->propertyCount++;
                uniformStartCopy++;
            }

            ref_propertyData->propertyStringsMemory = Alloc(vk_state->rendererAllocator, PROPERTY_MAX_NAME_LENGTH * ref_propertyData->propertyCount, MEM_TAG_RENDERER_SUBSYS);
            ref_propertyData->propertyNameArray = Alloc(vk_state->rendererAllocator, sizeof(*ref_propertyData->propertyNameArray) * ref_propertyData->propertyCount, MEM_TAG_RENDERER_SUBSYS);
            ref_propertyData->propertyOffsets = Alloc(vk_state->rendererAllocator, sizeof(*ref_propertyData->propertyOffsets) * ref_propertyData->propertyCount, MEM_TAG_RENDERER_SUBSYS);
            ref_propertyData->propertySizes = Alloc(vk_state->rendererAllocator, sizeof(*ref_propertyData->propertySizes) * ref_propertyData->propertyCount, MEM_TAG_RENDERER_SUBSYS);

            for (int i = 0; i < ref_propertyData->propertyCount; i++)
            {
                ref_propertyData->propertyNameArray[i] = ref_propertyData->propertyStringsMemory + PROPERTY_MAX_NAME_LENGTH * i;
            }

            // Getting all the relevent data about each property
            u32 currentProperty = 0;
            while (*uniformStart != '}')
            {
                while (*uniformStart == ' ' || *uniformStart == '\t' || *uniformStart == '\n')
                    uniformStart++;

                if (MemoryCompare(uniformStart, "float", 5))
                {
                    // Putting uniformStart past the type and at the start of the property name
                    uniformStart += 6;

                    // ===== Saving information about the property
                    // Making sure the matrix is properly aligned
                    u32 alignmentPadding = (SCALAR_ALIGNMENT - (ref_propertyData->uniformBufferSize % SCALAR_ALIGNMENT)) % SCALAR_ALIGNMENT;
                    ref_propertyData->uniformBufferSize += alignmentPadding;

                    ref_propertyData->propertyOffsets[currentProperty] = ref_propertyData->uniformBufferSize;
                    ref_propertyData->propertySizes[currentProperty] = SCALAR_SIZE;
                    ref_propertyData->uniformBufferSize += SCALAR_SIZE;
                }

                if (MemoryCompare(uniformStart, "mat4", 4))
                {
                    // Putting uniformStart past the type and at the start of the property name
                    uniformStart += 5;

                    // ===== Saving information about the property
                    // Making sure the matrix is properly aligned
                    u32 alignmentPadding = (MAT4_ALIGNMENT - (ref_propertyData->uniformBufferSize % MAT4_ALIGNMENT)) % MAT4_ALIGNMENT;
                    ref_propertyData->uniformBufferSize += alignmentPadding;

                    ref_propertyData->propertyOffsets[currentProperty] = ref_propertyData->uniformBufferSize;
                    ref_propertyData->propertySizes[currentProperty] = MAT4_SIZE;
                    ref_propertyData->uniformBufferSize += MAT4_SIZE;
                }

                if (MemoryCompare(uniformStart, "vec4", 4))
                {
                    // Putting uniformStart past the type and at the start of the property name
                    uniformStart += 5;

                    // Making sure the vector is properly aligned
                    u32 alignmentPadding = (VEC4_ALIGNMENT - (ref_propertyData->uniformBufferSize % VEC4_ALIGNMENT)) % VEC4_ALIGNMENT;
                    ref_propertyData->uniformBufferSize += alignmentPadding;

                    ref_propertyData->propertyOffsets[currentProperty] = ref_propertyData->uniformBufferSize;
                    ref_propertyData->propertySizes[currentProperty] = VEC4_SIZE;
                    ref_propertyData->uniformBufferSize += VEC4_SIZE;
                }

                if (MemoryCompare(uniformStart, "vec3", 4))
                {
                    // Putting uniformStart past the type and at the start of the property name
                    uniformStart += 5;

                    // Making sure the vector is properly aligned
                    u32 alignmentPadding = (VEC3_ALIGNMENT - (ref_propertyData->uniformBufferSize % VEC3_ALIGNMENT)) % VEC3_ALIGNMENT;
                    ref_propertyData->uniformBufferSize += alignmentPadding;

                    ref_propertyData->propertyOffsets[currentProperty] = ref_propertyData->uniformBufferSize;
                    ref_propertyData->propertySizes[currentProperty] = VEC3_SIZE;
                    ref_propertyData->uniformBufferSize += VEC3_SIZE;
                }

                if (MemoryCompare(uniformStart, "vec2", 4))
                {
                    // Putting uniformStart past the type and at the start of the property name
                    uniformStart += 5;

                    // Making sure the vector is properly aligned
                    u32 alignmentPadding = (VEC2_ALIGNMENT - (ref_propertyData->uniformBufferSize % VEC2_ALIGNMENT)) % VEC2_ALIGNMENT;
                    ref_propertyData->uniformBufferSize += alignmentPadding;

                    ref_propertyData->propertyOffsets[currentProperty] = ref_propertyData->uniformBufferSize;
                    ref_propertyData->propertySizes[currentProperty] = VEC2_SIZE;
                    ref_propertyData->uniformBufferSize += VEC2_SIZE;
                }

                // ===== Getting the property name
                uniformStartCopy = uniformStart;
                u32 nameLength = 0;
                while (*uniformStart != ';')
                {
                    nameLength++;
                    uniformStart++;
                }

                MemoryCopy(ref_propertyData->propertyNameArray[currentProperty], uniformStartCopy, nameLength);
                ref_propertyData->propertyNameArray[currentProperty][nameLength] = '\0';

                //_DEBUG("CurrentProperty: %i, Name: %s, Size: %i, Offset: %i", currentProperty, shader->propertyNameArray[currentProperty], shader->propertySizes[currentProperty], shader->propertyOffsets[currentProperty]);

                // Preparing for the next property
                currentProperty++;

                while (*uniformStart != '\n')
                    uniformStart++;
                uniformStart++;
            }
        }
        //_DEBUG("Properties: %i, Uniform size: %i", shader->propertyCount, shader->uniformBufferSize);
    }

    // ============================================== Getting uniform texture data ============================================================
    {
        #define UNIFORM_SAMPLER_TEXT ") uniform sampler2D"
        #define UNIFORM_SAMPLER_TEXT_SIZE 20
        // Finding the uniform blocks
        ref_textureData->textureCount = 0;
        u32 bindingIndices[10];
        char nameStrings[10 * PROPERTY_MAX_NAME_LENGTH];
        for (u32 i = 0; i < fileSize - 100 /*minus one hundred to not check past the end of the file after finding an enter in the last line*/; i++)
        {
            GRASSERT_DEBUG(ref_textureData->textureCount < 8); // TODO: make the texture count limit higher if necessary, this needs to be coordinated with vulkan_shader and vulkan_material
            if (text[i] == '\n')
            {
                if (MemoryCompare(text + i, LAYOUT_TEXT, LAYOUT_TEXT_SIZE - 1) && MemoryCompare(text + i + LAYOUT_TEXT_SIZE, UNIFORM_SAMPLER_TEXT, UNIFORM_SAMPLER_TEXT_SIZE - 1) && !BracketBeforeSemicolon(text + i + LAYOUT_TEXT_SIZE))
                {
                    bindingIndices[ref_textureData->textureCount] = *(text + i + LAYOUT_TEXT_SIZE - 1) - 48/*translating char to int*/;

                    // ===== Getting the property name
                    char* namePtr = text + i + LAYOUT_TEXT_SIZE + UNIFORM_SAMPLER_TEXT_SIZE - 2;
                    while (*namePtr != ' ')namePtr++;
                    namePtr++;
                    char* namePtrCopy = namePtr;

                    u32 nameLength = 0;
                    while (*namePtr != ';')
                    {
                        namePtr++;
                        nameLength++;
                    }

                    MemoryCopy(nameStrings + ref_textureData->textureCount * PROPERTY_MAX_NAME_LENGTH, namePtrCopy, nameLength);
                    nameStrings[nameLength + ref_textureData->textureCount * PROPERTY_MAX_NAME_LENGTH] = '\0';

                    //_DEBUG("TexName: %s", nameStrings + ref_textureData->textureCount * PROPERTY_MAX_NAME_LENGTH);

                    ref_textureData->textureCount++;
                }
            }
        }
        //_DEBUG("Textures: %i", ref_textureData->textureCount);

        if (ref_textureData->textureCount > 0)
        {
            ref_textureData->bindingIndices = Alloc(vk_state->rendererAllocator, ref_textureData->textureCount * sizeof(*ref_textureData->bindingIndices), MEM_TAG_RENDERER_SUBSYS);
            ref_textureData->textureStringsMemory = Alloc(vk_state->rendererAllocator, ref_textureData->textureCount * PROPERTY_MAX_NAME_LENGTH, MEM_TAG_RENDERER_SUBSYS);
            ref_textureData->textureNameArray = Alloc(vk_state->rendererAllocator, ref_textureData->textureCount * sizeof(*ref_textureData->textureNameArray), MEM_TAG_RENDERER_SUBSYS);

            MemoryCopy(ref_textureData->bindingIndices, bindingIndices, ref_textureData->textureCount * sizeof(*ref_textureData->bindingIndices));
            MemoryCopy(ref_textureData->textureStringsMemory, nameStrings, ref_textureData->textureCount * PROPERTY_MAX_NAME_LENGTH);

            for (int i = 0; i < ref_textureData->textureCount; i++)
            {
                ref_textureData->textureNameArray[i] = ref_textureData->textureStringsMemory + PROPERTY_MAX_NAME_LENGTH * i;
            }
        }
    }

    Free(vk_state->rendererAllocator, text);
}

void FreeUniformData(UniformPropertiesData* propertyData, UniformTexturesData* textureData)
{
    if (propertyData->propertyCount > 0)
    {
        Free(vk_state->rendererAllocator, propertyData->propertyNameArray);
        Free(vk_state->rendererAllocator, propertyData->propertyStringsMemory);
        Free(vk_state->rendererAllocator, propertyData->propertyOffsets);
        Free(vk_state->rendererAllocator, propertyData->propertySizes);
    }

    if (textureData->textureCount > 0)
    {
        Free(vk_state->rendererAllocator, textureData->bindingIndices);
        Free(vk_state->rendererAllocator, textureData->textureNameArray);
        Free(vk_state->rendererAllocator, textureData->textureStringsMemory);
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
