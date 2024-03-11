#include "../material.h"

#include "core/asserts.h"
#include "vulkan_buffer.h"
#include "vulkan_types.h"
#include "core/meminc.h"
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
    material->uniformBufferArray = Alloc(vk_state->rendererAllocator, MAX_FRAMES_IN_FLIGHT * sizeof(*material->uniformBufferArray), MEM_TAG_RENDERER_SUBSYS);
    material->uniformBufferMemoryArray = Alloc(vk_state->rendererAllocator, MAX_FRAMES_IN_FLIGHT * sizeof(*material->uniformBufferMemoryArray), MEM_TAG_RENDERER_SUBSYS);
    material->uniformBufferMappedArray = Alloc(vk_state->rendererAllocator, MAX_FRAMES_IN_FLIGHT * sizeof(*material->uniformBufferMappedArray), MEM_TAG_RENDERER_SUBSYS);

    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        CreateBuffer(shader->uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &material->uniformBufferArray[i], &material->uniformBufferMemoryArray[i]);
        vkMapMemory(vk_state->device, material->uniformBufferMemoryArray[i], 0, shader->uniformBufferSize, 0, &material->uniformBufferMappedArray[i]);
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
        VkDescriptorBufferInfo descriptorBufferInfo = {};
        descriptorBufferInfo.buffer = material->uniformBufferArray[i];
        descriptorBufferInfo.offset = 0;
        descriptorBufferInfo.range = shader->uniformBufferSize;

        VkWriteDescriptorSet descriptorWrites[2] = {};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].pNext = nullptr;
        descriptorWrites[0].dstSet = material->descriptorSetArray[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].pImageInfo = nullptr;
        descriptorWrites[0].pBufferInfo = &descriptorBufferInfo;
        descriptorWrites[0].pTexelBufferView = nullptr;

        VulkanImage* defaultTexture = vk_state->defaultTexture.internalState;

        VkDescriptorImageInfo descriptorImageInfo = {};
        descriptorImageInfo.sampler = defaultTexture->sampler;
        descriptorImageInfo.imageView = defaultTexture->view;
        descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].pNext = nullptr;
        descriptorWrites[1].dstSet = material->descriptorSetArray[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].pImageInfo = &descriptorImageInfo;
        descriptorWrites[1].pBufferInfo = nullptr;
        descriptorWrites[1].pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(vk_state->device, 2, descriptorWrites, 0, nullptr);
    }

    return clientMaterial;
}

void MaterialDestroy(Material clientMaterial)
{
    VulkanMaterial* material = clientMaterial.internalState;

    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        vkUnmapMemory(vk_state->device, material->uniformBufferMemoryArray[i]);
        vkDestroyBuffer(vk_state->device, material->uniformBufferArray[i], vk_state->vkAllocator);
        vkFreeMemory(vk_state->device, material->uniformBufferMemoryArray[i], vk_state->vkAllocator);
    }

    Free(vk_state->rendererAllocator, material->uniformBufferMappedArray);
    Free(vk_state->rendererAllocator, material->uniformBufferArray);
    Free(vk_state->rendererAllocator, material->uniformBufferMemoryArray);
    Free(vk_state->rendererAllocator, material);
}

void MaterialUpdateProperty(Material clientMaterial, const char* name, void* value)
{
    VulkanMaterial* material = clientMaterial.internalState;
    VulkanShader* shader = material->shader;

    u32 nameLength = strlen(name);

    for (int i = 0; i < shader->propertyCount; i++)
    {
        if (MemoryCompare(name, shader->propertyNameArray[i], nameLength))
        {
            MemoryCopy((u8*)material->uniformBufferMappedArray[vk_state->currentInFlightFrameIndex] + shader->propertyOffsets[i], value, shader->propertySizes[i]);
            break;
        }
    }
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
