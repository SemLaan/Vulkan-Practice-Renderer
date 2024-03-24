#include "../material.h"

#include "core/asserts.h"
#include "core/meminc.h"
#include "vulkan_buffer.h"
#include "vulkan_types.h"
#include <string.h>

Material MaterialCreate(Shader clientShader)
{
    VulkanShader* shader = clientShader.internalState;

    // Allocating a material struct
    Material clientMaterial;
    clientMaterial.internalState = Alloc(vk_state->rendererAllocator, sizeof(VulkanMaterial), MEM_TAG_RENDERER_SUBSYS);
    VulkanMaterial* material = clientMaterial.internalState;
    material->shader = shader;

    // ============================================================================================================================================================
    // ======================== Creating uniform buffers ============================================================================
    // ============================================================================================================================================================
    if (shader->totalUniformDataSize > 0)
    {
        CreateBuffer(shader->totalUniformDataSize * MAX_FRAMES_IN_FLIGHT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &material->uniformBuffer, &material->uniformBufferMemory);
        vkMapMemory(vk_state->device, material->uniformBufferMemory, 0, shader->totalUniformDataSize * MAX_FRAMES_IN_FLIGHT, 0, &material->uniformBufferMapped);
    }

    // ============================================================================================================================================================
    // ======================== Allocating descriptor sets ============================================================================
    // ============================================================================================================================================================
    VkDescriptorSetLayout descriptorSetLayouts[MAX_FRAMES_IN_FLIGHT];
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        descriptorSetLayouts[i] = shader->descriptorSetLayout;
    }

    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
    descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocInfo.pNext = nullptr;
    descriptorSetAllocInfo.descriptorPool = vk_state->descriptorPool;
    descriptorSetAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
    descriptorSetAllocInfo.pSetLayouts = descriptorSetLayouts;

    material->descriptorSetArray = Alloc(vk_state->rendererAllocator, MAX_FRAMES_IN_FLIGHT * sizeof(*material->descriptorSetArray), MEM_TAG_RENDERER_SUBSYS);

    if (VK_SUCCESS != vkAllocateDescriptorSets(vk_state->device, &descriptorSetAllocInfo, material->descriptorSetArray))
    {
        GRASSERT_MSG(false, "Vulkan descriptor set allocation failed");
    }

    // ============================================================================================================================================================
    // =============================== Initialize/Update descriptor sets ====================================================
    // ============================================================================================================================================================
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VkWriteDescriptorSet descriptorWrites[10] = {};

        u32 descriptorWriteIndex = 0;

        if (shader->vertUniformPropertiesData.propertyCount > 0)
        {
            VkDescriptorBufferInfo descriptorBufferInfo = {};
            descriptorBufferInfo.buffer = material->uniformBuffer;
            descriptorBufferInfo.offset = i * shader->totalUniformDataSize;
            descriptorBufferInfo.range = shader->vertUniformPropertiesData.uniformBufferSize;

            descriptorWrites[descriptorWriteIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[descriptorWriteIndex].pNext = nullptr;
            descriptorWrites[descriptorWriteIndex].dstSet = material->descriptorSetArray[i];
            descriptorWrites[descriptorWriteIndex].dstBinding = shader->vertUniformPropertiesData.bindingIndex;
            descriptorWrites[descriptorWriteIndex].dstArrayElement = 0;
            descriptorWrites[descriptorWriteIndex].descriptorCount = 1;
            descriptorWrites[descriptorWriteIndex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[descriptorWriteIndex].pImageInfo = nullptr;
            descriptorWrites[descriptorWriteIndex].pBufferInfo = &descriptorBufferInfo;
            descriptorWrites[descriptorWriteIndex].pTexelBufferView = nullptr;

            descriptorWriteIndex++;
        }

        if (shader->fragUniformPropertiesData.propertyCount > 0)
        {
            VkDescriptorBufferInfo descriptorBufferInfo = {};
            descriptorBufferInfo.buffer = material->uniformBuffer;
            descriptorBufferInfo.offset = (i * shader->totalUniformDataSize) + shader->fragmentUniformBufferOffset;
            descriptorBufferInfo.range = shader->fragUniformPropertiesData.uniformBufferSize;

            descriptorWrites[descriptorWriteIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[descriptorWriteIndex].pNext = nullptr;
            descriptorWrites[descriptorWriteIndex].dstSet = material->descriptorSetArray[i];
            descriptorWrites[descriptorWriteIndex].dstBinding = shader->fragUniformPropertiesData.bindingIndex;
            descriptorWrites[descriptorWriteIndex].dstArrayElement = 0;
            descriptorWrites[descriptorWriteIndex].descriptorCount = 1;
            descriptorWrites[descriptorWriteIndex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[descriptorWriteIndex].pImageInfo = nullptr;
            descriptorWrites[descriptorWriteIndex].pBufferInfo = &descriptorBufferInfo;
            descriptorWrites[descriptorWriteIndex].pTexelBufferView = nullptr;

            descriptorWriteIndex++;
        }

        VulkanImage* defaultTexture = vk_state->defaultTexture.internalState;

        VkDescriptorImageInfo descriptorImageInfo = {};
        descriptorImageInfo.sampler = vk_state->samplers->nearestRepeat;
        descriptorImageInfo.imageView = defaultTexture->view;
        descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        for (int j = 0; j < shader->vertUniformTexturesData.textureCount; j++)
        {
            descriptorWrites[descriptorWriteIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[descriptorWriteIndex].pNext = nullptr;
            descriptorWrites[descriptorWriteIndex].dstSet = material->descriptorSetArray[i];
            descriptorWrites[descriptorWriteIndex].dstBinding = shader->vertUniformTexturesData.bindingIndices[j];
            descriptorWrites[descriptorWriteIndex].dstArrayElement = 0;
            descriptorWrites[descriptorWriteIndex].descriptorCount = 1;
            descriptorWrites[descriptorWriteIndex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[descriptorWriteIndex].pImageInfo = &descriptorImageInfo;
            descriptorWrites[descriptorWriteIndex].pBufferInfo = nullptr;
            descriptorWrites[descriptorWriteIndex].pTexelBufferView = nullptr;

            descriptorWriteIndex++;
        }

        for (int j = 0; j < shader->fragUniformTexturesData.textureCount; j++)
        {
            descriptorWrites[descriptorWriteIndex].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[descriptorWriteIndex].pNext = nullptr;
            descriptorWrites[descriptorWriteIndex].dstSet = material->descriptorSetArray[i];
            descriptorWrites[descriptorWriteIndex].dstBinding = shader->fragUniformTexturesData.bindingIndices[j];
            descriptorWrites[descriptorWriteIndex].dstArrayElement = 0;
            descriptorWrites[descriptorWriteIndex].descriptorCount = 1;
            descriptorWrites[descriptorWriteIndex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[descriptorWriteIndex].pImageInfo = &descriptorImageInfo;
            descriptorWrites[descriptorWriteIndex].pBufferInfo = nullptr;
            descriptorWrites[descriptorWriteIndex].pTexelBufferView = nullptr;

            descriptorWriteIndex++;
        }

        vkUpdateDescriptorSets(vk_state->device, descriptorWriteIndex /*interpreted as descriptor write count*/, descriptorWrites, 0, nullptr);
    }

    return clientMaterial;
}

void MaterialDestroy(Material clientMaterial)
{
    VulkanMaterial* material = clientMaterial.internalState;
    VulkanShader* shader = material->shader;

    if (shader->totalUniformDataSize > 0)
    {
        vkUnmapMemory(vk_state->device, material->uniformBufferMemory);
        vkDestroyBuffer(vk_state->device, material->uniformBuffer, vk_state->vkAllocator);
        vkFreeMemory(vk_state->device, material->uniformBufferMemory, vk_state->vkAllocator);
    }

    Free(vk_state->rendererAllocator, material);
}

void MaterialUpdateProperty(Material clientMaterial, const char* name, void* value)
{
    VulkanMaterial* material = clientMaterial.internalState;
    VulkanShader* shader = material->shader;

    u32 nameLength = strlen(name);

    for (int i = 0; i < shader->vertUniformPropertiesData.propertyCount; i++)
    {
        if (MemoryCompare(name, shader->vertUniformPropertiesData.propertyNameArray[i], nameLength))
        {
            // Taking the mapped buffer, then offsetting into the current frame, then offsetting into the current property
            u8* uniformPropertyLocation = ((u8*)material->uniformBufferMapped) + vk_state->currentInFlightFrameIndex * shader->totalUniformDataSize + shader->vertUniformPropertiesData.propertyOffsets[i];
            MemoryCopy(uniformPropertyLocation, value, shader->vertUniformPropertiesData.propertySizes[i]);
            return;
        }
    }

    for (int i = 0; i < shader->fragUniformPropertiesData.propertyCount; i++)
    {
        if (MemoryCompare(name, shader->fragUniformPropertiesData.propertyNameArray[i], nameLength))
        {
            // Taking the mapped buffer, then offsetting into the current frame, then offsetting into the current property
            u8* uniformPropertyLocation = ((u8*)material->uniformBufferMapped) + vk_state->currentInFlightFrameIndex * shader->totalUniformDataSize + shader->fragUniformPropertiesData.propertyOffsets[i];
            MemoryCopy(uniformPropertyLocation, value, shader->fragUniformPropertiesData.propertySizes[i]);
            return;
        }
    }

    _FATAL("Property name: %s, couldn't be found in material", name);
    GRASSERT_MSG(false, "Property name couldn't be found");
}

void MaterialUpdateTexture(Material clientMaterial, const char* name, Texture clientTexture, SamplerType samplerType)
{
    VulkanMaterial* material = clientMaterial.internalState;
    VulkanShader* shader = material->shader;

    u32 nameLength = strlen(name);

    VulkanImage* texture = clientTexture.internalState;

    // Getting the relevant sampler
    VkSampler sampler = nullptr;
    {
        switch (samplerType)
        {
        case SAMPLER_TYPE_NEAREST_CLAMP_EDGE:
            sampler = vk_state->samplers->nearestClampEdge;
            break;
        case SAMPLER_TYPE_NEAREST_REPEAT:
            sampler = vk_state->samplers->nearestRepeat;
            break;
        case SAMPLER_TYPE_LINEAR_CLAMP_EDGE:
            sampler = vk_state->samplers->linearClampEdge;
            break;
        case SAMPLER_TYPE_LINEAR_REPEAT:
            sampler = vk_state->samplers->linearRepeat;
            break;
        case SAMPLER_TYPE_ANISOTROPIC_CLAMP_EDGE:
            GRASSERT_MSG(false, "not implemented");
            break;
        case SAMPLER_TYPE_ANISOTROPIC_REPEAT:
            GRASSERT_MSG(false, "not implemented");
            break;
        case SAMPLER_TYPE_SHADOW:
            sampler = vk_state->samplers->shadow;
            break;
        default:
            GRASSERT_MSG(false, "Invalid sampler type");
            break;
        }
    }

    VkDescriptorImageInfo descriptorImageInfo = {};
    descriptorImageInfo.sampler = sampler;
    descriptorImageInfo.imageView = texture->view;
    descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Looping through all vertex shader texture properties trying to find the texture being set
    for (int i = 0; i < shader->vertUniformTexturesData.textureCount; i++)
    {
        if (MemoryCompare(name, shader->vertUniformTexturesData.textureNameArray[i], nameLength))
        {
            // Updating the descriptor bindings for the texture being set
            VkWriteDescriptorSet descriptorWrites[MAX_FRAMES_IN_FLIGHT] = {};

            for (int j = 0; j < MAX_FRAMES_IN_FLIGHT; j++)
            {
                descriptorWrites[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[j].pNext = nullptr;
                descriptorWrites[j].dstSet = material->descriptorSetArray[j];
                descriptorWrites[j].dstBinding = shader->vertUniformTexturesData.bindingIndices[i];
                descriptorWrites[j].dstArrayElement = 0;
                descriptorWrites[j].descriptorCount = 1;
                descriptorWrites[j].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[j].pImageInfo = &descriptorImageInfo;
                descriptorWrites[j].pBufferInfo = nullptr;
                descriptorWrites[j].pTexelBufferView = nullptr;
            }

            vkUpdateDescriptorSets(vk_state->device, MAX_FRAMES_IN_FLIGHT, descriptorWrites, 0, nullptr);
            return;
        }
    }

    // Looping through all fragment shader texture properties trying to find the texture being set
    for (int i = 0; i < shader->fragUniformTexturesData.textureCount; i++)
    {
        if (MemoryCompare(name, shader->fragUniformTexturesData.textureNameArray[i], nameLength))
        {
            // Updating the descriptor bindings for the texture being set
            VkWriteDescriptorSet descriptorWrites[MAX_FRAMES_IN_FLIGHT] = {};

            for (int j = 0; j < MAX_FRAMES_IN_FLIGHT; j++)
            {
                descriptorWrites[j].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[j].pNext = nullptr;
                descriptorWrites[j].dstSet = material->descriptorSetArray[j];
                descriptorWrites[j].dstBinding = shader->fragUniformTexturesData.bindingIndices[i];
                descriptorWrites[j].dstArrayElement = 0;
                descriptorWrites[j].descriptorCount = 1;
                descriptorWrites[j].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[j].pImageInfo = &descriptorImageInfo;
                descriptorWrites[j].pBufferInfo = nullptr;
                descriptorWrites[j].pTexelBufferView = nullptr;
            }

            vkUpdateDescriptorSets(vk_state->device, MAX_FRAMES_IN_FLIGHT, descriptorWrites, 0, nullptr);
            return;
        }
    }

    _FATAL("Texture name: %s, couldn't be found in material", name);
    GRASSERT_MSG(false, "Texture name couldn't be found");
}

void MaterialBind(Material clientMaterial)
{
    VulkanMaterial* material = clientMaterial.internalState;

    VkCommandBuffer currentCommandBuffer = vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex].handle;

    // TODO: check which shader is bound first and dont change if the shader is already bound
    vkCmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material->shader->pipelineObject);
    vkCmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, material->shader->pipelineLayout, 1, 1, &material->descriptorSetArray[vk_state->currentInFlightFrameIndex], 0, nullptr);

    vk_state->boundShader = material->shader;
}
