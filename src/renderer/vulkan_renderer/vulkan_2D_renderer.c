// Implements both
#include "vulkan_2D_renderer.h"

#include "core/asserts.h"
#include "renderer/renderer.h"
#include "vulkan_buffer.h"
#include "vulkan_shader_loader.h"
#include "containers/hashmap_u64.h"
#include "containers/darray.h"

// ============================================================================================================================================================
// ============================ vulkan 2d renderer.h ============================================================================================================
// ============================================================================================================================================================

typedef struct SpriteInstance
{
	mat4 model;
	u32 textureIndex;
} SpriteInstance;

#define COMBINED_IMAGE_SAMPLERS_ARRAY_SIZE 100

// TODO: factor out some data into a struct for user data and renderer data
// State for the 2d renderer
typedef struct Renderer2DState
{
    SceneRenderData2D currentRenderData;            //
    GlobalUniformObject currentGlobalUBO;           //
    VertexBuffer quadVertexBuffer;                  //
    VertexBuffer instancedBuffer;                   //
    IndexBuffer quadIndexBuffer;                    //
    VkDescriptorSetLayout descriptorSetLayout;      //
    VkBuffer* uniformBuffersDarray;                 //
    VkDeviceMemory* uniformBuffersMemoryDarray;     //
    void** uniformBuffersMappedDarray;              //
    VkDescriptorPool uniformDescriptorPool;         //
    VkDescriptorSet* uniformDescriptorSetsDarray;   //
    VkPipelineLayout pipelineLayout;                //
    VkPipeline graphicsPipeline;                    //
    HashmapU64* textureMap;                         //
    VulkanImage* textureDarray;                     //
} Renderer2DState;

static Renderer2DState* state2D = nullptr;

bool Initialize2DRenderer()
{
    state2D = Alloc(vk_state->rendererBumpAllocator, sizeof(*state2D), MEM_TAG_RENDERER_SUBSYS);

    // ============================================================================================================================================================
    // ======================== Creating graphics pipeline for the instanced quad shader ==========================================================================
    // ============================================================================================================================================================
    



    // ============================================================================================================================================================
    // ======================== Creating uniform buffers for the instanced quad shader ============================================================================
    // ============================================================================================================================================================
    {
        VkDeviceSize uniformBufferSize = sizeof(GlobalUniformObject);

        state2D->uniformBuffersDarray = (VkBuffer*)DarrayCreateWithSize(sizeof(VkBuffer), MAX_FRAMES_IN_FLIGHT, vk_state->rendererAllocator, MEM_TAG_RENDERER_SUBSYS);
        state2D->uniformBuffersMemoryDarray = (VkDeviceMemory*)DarrayCreateWithSize(sizeof(VkDeviceMemory), MAX_FRAMES_IN_FLIGHT, vk_state->rendererAllocator, MEM_TAG_RENDERER_SUBSYS);
        state2D->uniformBuffersMappedDarray = (void**)DarrayCreateWithSize(sizeof(void*), MAX_FRAMES_IN_FLIGHT, vk_state->rendererAllocator, MEM_TAG_RENDERER_SUBSYS);

        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            CreateBuffer(uniformBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &state2D->uniformBuffersDarray[i], &state2D->uniformBuffersMemoryDarray[i]);
            vkMapMemory(vk_state->device, state2D->uniformBuffersMemoryDarray[i], 0, uniformBufferSize, 0, &state2D->uniformBuffersMappedDarray[i]);
        }

        VkDescriptorSetLayout descriptorSetLayouts[MAX_FRAMES_IN_FLIGHT];
        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            descriptorSetLayouts[i] = state2D->descriptorSetLayout;
        }

        VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {};
        descriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorSetAllocInfo.pNext = nullptr;
        descriptorSetAllocInfo.descriptorPool = state2D->uniformDescriptorPool;
        descriptorSetAllocInfo.descriptorSetCount = MAX_FRAMES_IN_FLIGHT;
        descriptorSetAllocInfo.pSetLayouts = descriptorSetLayouts;

        state2D->uniformDescriptorSetsDarray = (VkDescriptorSet*)DarrayCreateWithSize(sizeof(VkDescriptorSet), MAX_FRAMES_IN_FLIGHT, vk_state->rendererAllocator, MEM_TAG_RENDERER_SUBSYS);

        if (VK_SUCCESS != vkAllocateDescriptorSets(vk_state->device, &descriptorSetAllocInfo, state2D->uniformDescriptorSetsDarray))
        {
            _FATAL("Vulkan descriptor set allocation failed");
            return false;
        }

        // =============================== Initialize descriptor sets ====================================================
        for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            VkDescriptorBufferInfo descriptorBufferInfo = {};
            descriptorBufferInfo.buffer = state2D->uniformBuffersDarray[i];
            descriptorBufferInfo.offset = 0;
            descriptorBufferInfo.range = sizeof(GlobalUniformObject);

            VkDescriptorImageInfo descriptorImageInfos[COMBINED_IMAGE_SAMPLERS_ARRAY_SIZE];
            for (u32 i = 0; i < COMBINED_IMAGE_SAMPLERS_ARRAY_SIZE; ++i)
            {
                descriptorImageInfos[i].sampler = ((VulkanImage*)vk_state->defaultTexture.internalState)->sampler;
                descriptorImageInfos[i].imageView = ((VulkanImage*)vk_state->defaultTexture.internalState)->view;
                descriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            VkWriteDescriptorSet descriptorWrites[2] = {};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].pNext = nullptr;
            descriptorWrites[0].dstSet = state2D->uniformDescriptorSetsDarray[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].pImageInfo = nullptr;
            descriptorWrites[0].pBufferInfo = &descriptorBufferInfo;
            descriptorWrites[0].pTexelBufferView = nullptr;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].pNext = nullptr;
            descriptorWrites[1].dstSet = state2D->uniformDescriptorSetsDarray[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorCount = COMBINED_IMAGE_SAMPLERS_ARRAY_SIZE;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].pImageInfo = descriptorImageInfos;
            descriptorWrites[1].pBufferInfo = nullptr;
            descriptorWrites[1].pTexelBufferView = nullptr;

            vkUpdateDescriptorSets(vk_state->device, 2, descriptorWrites, 0, nullptr);
        }
    }

    // ============================================================================================================================================================
    // ======================== Creating the quad mesh ============================================================================================================
    // ============================================================================================================================================================
#define QUAD_VERT_COUNT 4
    Vertex quadVertices[QUAD_VERT_COUNT] =
        {
            {{ 0.0, 0.0, 0}, {0, 0, 0}, {0, 1}},
            {{ 1.0, 0.0, 0}, {0, 0, 0}, {1, 1}},
            {{ 0.0, 1.0, 0}, {0, 0, 0}, {0, 0}},
            {{ 1.0, 1.0, 0}, {0, 0, 0}, {1, 0}},
        };

    state2D->quadVertexBuffer = VertexBufferCreate(quadVertices, sizeof(quadVertices));

#define QUAD_INDEX_COUNT 6
    u32 quadIndices[QUAD_INDEX_COUNT] =
        {
            0, 1, 2,
            2, 1, 3};

    state2D->quadIndexBuffer = IndexBufferCreate(quadIndices, QUAD_INDEX_COUNT);
    state2D->instancedBuffer = VertexBufferCreate(nullptr, 10000 * sizeof(SpriteInstance));

    const u32 mapBackingArraySize = 100;
    const u32 mapLinkedElementsSize = 10;
    state2D->textureMap = MapU64Create(vk_state->rendererBumpAllocator, MEM_TAG_RENDERER_SUBSYS, mapBackingArraySize, mapLinkedElementsSize, Hash6432Shift);

    state2D->textureDarray = DarrayCreate(sizeof(*state2D->textureDarray), mapBackingArraySize, vk_state->rendererAllocator, MEM_TAG_RENDERER_SUBSYS);

    _TRACE("2D renderer initialized");

    return true;
}

void Shutdown2DRenderer()
{
    DarrayDestroy(state2D->textureDarray);
    MapU64Destroy(state2D->textureMap);

    // ============================================================================================================================================================
    // ======================== Destroying the quad mesh ==========================================================================================================
    // ============================================================================================================================================================
    IndexBufferDestroy(state2D->quadIndexBuffer);
    VertexBufferDestroy(state2D->instancedBuffer);
    VertexBufferDestroy(state2D->quadVertexBuffer);

    // ============================================================================================================================================================
    // ======================== Destroying uniform buffers for the instanced quad shader ==========================================================================
    // ============================================================================================================================================================

    // ============================================================================================================================================================
    // ======================== Destroying graphics pipeline for the instanced quad shader ========================================================================
    // ============================================================================================================================================================
    {
        if (state2D->graphicsPipeline)
            vkDestroyPipeline(vk_state->device, state2D->graphicsPipeline, vk_state->vkAllocator);
        if (state2D->pipelineLayout)
            vkDestroyPipelineLayout(vk_state->device, state2D->pipelineLayout, vk_state->vkAllocator);

        for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vkUnmapMemory(vk_state->device, state2D->uniformBuffersMemoryDarray[i]);
            vkDestroyBuffer(vk_state->device, state2D->uniformBuffersDarray[i], vk_state->vkAllocator);
            vkFreeMemory(vk_state->device, state2D->uniformBuffersMemoryDarray[i], vk_state->vkAllocator);
        }

        DarrayDestroy(state2D->uniformBuffersDarray);
        DarrayDestroy(state2D->uniformBuffersMappedDarray);
        DarrayDestroy(state2D->uniformBuffersMemoryDarray);
        DarrayDestroy(state2D->uniformDescriptorSetsDarray);

        if (state2D->uniformDescriptorPool)
            vkDestroyDescriptorPool(vk_state->device, state2D->uniformDescriptorPool, vk_state->vkAllocator);

        if (state2D->descriptorSetLayout)
            vkDestroyDescriptorSetLayout(vk_state->device, state2D->descriptorSetLayout, vk_state->vkAllocator);
    }

    Free(vk_state->rendererBumpAllocator, state2D);
}

void Preprocess2DSceneData()
{
    // ========================== Preprocessing camera ================================
    state2D->currentGlobalUBO.projView = state2D->currentRenderData.camera;

    // =========================== preprocessing quads =================================
    u32 instanceCount = DarrayGetSize(state2D->currentRenderData.spriteRenderInfoDarray);
    GRASSERT_DEBUG(instanceCount > 0);

    SpriteInstance* instanceData = Alloc(vk_state->rendererAllocator, sizeof(*instanceData) * instanceCount, MEM_TAG_RENDERER_SUBSYS);

    DarraySetSize(state2D->textureDarray, 0);
    MapU64Flush(state2D->textureMap);

#define TEXTURE_INDEX_BIT (1 << 16)
    u32 currentTextureIndex = 0 + TEXTURE_INDEX_BIT;

    for (u32 i = 0; i < instanceCount; ++i)
    {
        // TODO: compare performance of using map and darray for checking whether a texture is already in the list
        Texture texture = state2D->currentRenderData.spriteRenderInfoDarray[i].texture;
        u32 result = (u64)MapU64Lookup(state2D->textureMap, (u64)texture.internalState);

        if (result == 0)
        {
            MapU64Insert(state2D->textureMap, (u64)texture.internalState, (void*)(u64)currentTextureIndex);
            DarrayPushback(state2D->textureDarray, texture.internalState);
            result = currentTextureIndex;
            currentTextureIndex++;
        }

        instanceData[i].model = state2D->currentRenderData.spriteRenderInfoDarray[i].model;
        instanceData[i].textureIndex = result - TEXTURE_INDEX_BIT;
    }

    VertexBufferUpdate(state2D->instancedBuffer, instanceData, instanceCount * sizeof(*instanceData));

    Free(vk_state->rendererAllocator, instanceData);

    // =============================== Updating descriptor sets ==================================
    MemoryCopy(state2D->uniformBuffersMappedDarray[vk_state->currentInFlightFrameIndex], &state2D->currentGlobalUBO, sizeof(GlobalUniformObject));

    {
        VkDescriptorBufferInfo descriptorBufferInfo = {};
        descriptorBufferInfo.buffer = state2D->uniformBuffersDarray[vk_state->currentInFlightFrameIndex];
        descriptorBufferInfo.offset = 0;
        descriptorBufferInfo.range = sizeof(GlobalUniformObject);

        VkDescriptorImageInfo descriptorImageInfos[COMBINED_IMAGE_SAMPLERS_ARRAY_SIZE];
        for (u32 i = 0; i < currentTextureIndex - TEXTURE_INDEX_BIT; ++i)
        {
            descriptorImageInfos[i].sampler = state2D->textureDarray[i].sampler;
            descriptorImageInfos[i].imageView = state2D->textureDarray[i].view;
            descriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        VkWriteDescriptorSet descriptorWrites[2] = {};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].pNext = nullptr;
        descriptorWrites[0].dstSet = state2D->uniformDescriptorSetsDarray[vk_state->currentInFlightFrameIndex];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].pImageInfo = nullptr;
        descriptorWrites[0].pBufferInfo = &descriptorBufferInfo;
        descriptorWrites[0].pTexelBufferView = nullptr;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].pNext = nullptr;
        descriptorWrites[1].dstSet = state2D->uniformDescriptorSetsDarray[vk_state->currentInFlightFrameIndex];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorCount = currentTextureIndex - TEXTURE_INDEX_BIT;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].pImageInfo = descriptorImageInfos;
        descriptorWrites[1].pBufferInfo = nullptr;
        descriptorWrites[1].pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(vk_state->device, 2, descriptorWrites, 0, nullptr);
    }
}

void Render2DScene()
{
    VkCommandBuffer currentCommandBuffer = vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex].handle;

    // Binding global descriptor set
    vkCmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state2D->pipelineLayout, 0, 1, &state2D->uniformDescriptorSetsDarray[vk_state->currentInFlightFrameIndex], 0, nullptr);

    vkCmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, state2D->graphicsPipeline);

    u32 instanceCount = DarrayGetSize(state2D->currentRenderData.spriteRenderInfoDarray);

    VulkanVertexBuffer* quadBuffer = state2D->quadVertexBuffer.internalState;
    VulkanIndexBuffer* indexBuffer = state2D->quadIndexBuffer.internalState;
    VulkanVertexBuffer* instancedBuffer = state2D->instancedBuffer.internalState;

    VkBuffer vertexBuffers[2] = {quadBuffer->handle, instancedBuffer->handle};

    VkDeviceSize offsets[2] = {0, 0};
    vkCmdBindVertexBuffers(currentCommandBuffer, 0, 2, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(currentCommandBuffer, indexBuffer->handle, 0, VK_INDEX_TYPE_UINT32);

    vkCmdDrawIndexed(currentCommandBuffer, indexBuffer->indexCount, instanceCount, 0, 0, 0);

    DarrayDestroy(state2D->currentRenderData.spriteRenderInfoDarray);
}

// ============================================================================================================================================================
// ============================ 2D renderer.h ============================================================================================================
// ============================================================================================================================================================
void Submit2DScene(SceneRenderData2D sceneData)
{
    state2D->currentRenderData = sceneData;
}
