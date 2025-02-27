// This file implements both of these headers!
#include "vulkan_buffer.h"
#include "../buffer.h"

#include "core/asserts.h"
#include "vulkan_command_buffer.h"
#include "vulkan_types.h"

u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags requiredFlags)
{
    VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
    vkGetPhysicalDeviceMemoryProperties(vk_state->physicalDevice, &deviceMemoryProperties);

    for (u32 i = 0; i < deviceMemoryProperties.memoryTypeCount; ++i)
    {
        if (typeFilter & (1 << i) && (deviceMemoryProperties.memoryTypes[i].propertyFlags & requiredFlags) == requiredFlags)
        {
            return i;
        }
    }

    GRASSERT(false);
    return 0;
}

static void CopyBufferAndTransitionQueue(VkBuffer dstBuffer, VkBuffer srcBuffer, u32 waitSemaphoreCount, VkSemaphoreSubmitInfo* pWaitSemaphoreInfos, u32 signalSemaphoreCount, VkSemaphoreSubmitInfo* pSignalSemaphoreInfos, VkDependencyInfo* pDependencyInfo, VkDeviceSize size, u64* out_signaledValue)
{
    CommandBuffer transferCommandBuffer;
    AllocateAndBeginSingleUseCommandBuffer(&vk_state->transferQueue, &transferCommandBuffer);

    VkBufferCopy copyRegion = {};
    copyRegion.dstOffset = 0;
    copyRegion.srcOffset = 0;
    copyRegion.size = size;

    vkCmdCopyBuffer(transferCommandBuffer.handle, srcBuffer, dstBuffer, 1, &copyRegion);

    if (pDependencyInfo)
        vkCmdPipelineBarrier2(transferCommandBuffer.handle, pDependencyInfo);

    EndSubmitAndFreeSingleUseCommandBuffer(transferCommandBuffer, waitSemaphoreCount, pWaitSemaphoreInfos, signalSemaphoreCount, pSignalSemaphoreInfos, out_signaledValue);
}

bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer* out_buffer, VkDeviceMemory* out_memory)
{
    VkBufferCreateInfo bufferCreateInfo = {};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.pNext = nullptr;
    bufferCreateInfo.flags = 0;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = bufferUsageFlags;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferCreateInfo.queueFamilyIndexCount = 0;
    bufferCreateInfo.pQueueFamilyIndices = nullptr;

    if (VK_SUCCESS != vkCreateBuffer(vk_state->device, &bufferCreateInfo, vk_state->vkAllocator, out_buffer))
    {
        _FATAL("Buffer creation failed");
        return false;
    }

    VkMemoryRequirements stagingMemoryRequirements;
    vkGetBufferMemoryRequirements(vk_state->device, *out_buffer, &stagingMemoryRequirements);

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.allocationSize = stagingMemoryRequirements.size;
    allocateInfo.memoryTypeIndex = FindMemoryType(stagingMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (VK_SUCCESS != vkAllocateMemory(vk_state->device, &allocateInfo, vk_state->vkAllocator, out_memory))
    {
        _FATAL("Vulkan device memory allocation failed");
        return false;
    }

    vkBindBufferMemory(vk_state->device, *out_buffer, *out_memory, 0);

    return true;
}

void VulkanBufferDestructor(void* resource)
{
    vkDestroyBuffer(vk_state->device, (VkBuffer)resource, vk_state->vkAllocator);
}

void VulkanMemoryDestructor(void* resource)
{
    vkFreeMemory(vk_state->device, (VkDeviceMemory)resource, vk_state->vkAllocator);
}

VertexBuffer VertexBufferCreate(void* vertices, size_t size)
{
    // Allocating memory for the buffer (struct on CPU not vulkan)
    VertexBuffer clientBuffer;
    clientBuffer.internalState = Alloc(vk_state->rendererAllocator, sizeof(VulkanVertexBuffer));
    VulkanVertexBuffer* buffer = (VulkanVertexBuffer*)clientBuffer.internalState;
    buffer->size = size;

    // ================= creating the GPU buffer =========================
    CreateBuffer(buffer->size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer->handle, &buffer->memory);

    if (vertices != nullptr)
    {
        // ================ Staging buffer =========================
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        CreateBuffer(buffer->size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory);

        // ================= copying data into staging buffer ===============================
        void* data;
        vkMapMemory(vk_state->device, stagingMemory, 0, buffer->size, 0, &data);
        MemoryCopy(data, vertices, (size_t)buffer->size);
        vkUnmapMemory(vk_state->device, stagingMemory);

        // Creating the buffer memory barrier for the queue family release operation
        VkBufferMemoryBarrier2 releaseBufferInfo = {};
        releaseBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        releaseBufferInfo.pNext = nullptr;
        releaseBufferInfo.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        releaseBufferInfo.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        releaseBufferInfo.dstStageMask = 0;  // IGNORED because it is a queue family release operation
        releaseBufferInfo.dstAccessMask = 0; // IGNORED because it is a queue family release operation
        releaseBufferInfo.srcQueueFamilyIndex = vk_state->transferQueue.index;
        releaseBufferInfo.dstQueueFamilyIndex = vk_state->graphicsQueue.index;
        releaseBufferInfo.buffer = buffer->handle;
        releaseBufferInfo.offset = 0;
        releaseBufferInfo.size = buffer->size;

        VkDependencyInfo releaseDependencyInfo = {};
        releaseDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        releaseDependencyInfo.pNext = nullptr;
        releaseDependencyInfo.dependencyFlags = 0;
        releaseDependencyInfo.memoryBarrierCount = 0;
        releaseDependencyInfo.pMemoryBarriers = nullptr;
        releaseDependencyInfo.bufferMemoryBarrierCount = 1;
        releaseDependencyInfo.pBufferMemoryBarriers = &releaseBufferInfo;
        releaseDependencyInfo.imageMemoryBarrierCount = 0;
        releaseDependencyInfo.pImageMemoryBarriers = nullptr;

        // Submitting the semaphore that can let other queues know when this vertex buffer has been uploaded
        vk_state->vertexUploadSemaphore.submitValue++;
        VkSemaphoreSubmitInfo semaphoreSubmitInfo = {};
        semaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        semaphoreSubmitInfo.pNext = nullptr;
        semaphoreSubmitInfo.semaphore = vk_state->vertexUploadSemaphore.handle;
        semaphoreSubmitInfo.value = vk_state->vertexUploadSemaphore.submitValue;
        semaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
        semaphoreSubmitInfo.deviceIndex = 0;

        // Copying the buffer to the GPU - this function returns before the buffer is actually copied
        u64 signaledValue; // This value indicates at what point in time the command buffer that executes the copy is finished, used for deleting staging buffer and memory
        CopyBufferAndTransitionQueue(buffer->handle, stagingBuffer, 0, nullptr, 1, &semaphoreSubmitInfo, &releaseDependencyInfo, buffer->size, &signaledValue);

        // Creating the buffer memory barrier for the queue family acquire operation
        // This is put in the requestedQueueAcquisitionOperations list and will be submitted as a command in the draw loop,
        // also synced with vertex upload semaphore, so ownership isn't acquired before it is released
        VkDependencyInfo* acquireDependencyInfo = Alloc(vk_state->resourceAcquisitionPool, sizeof(VkDependencyInfo) + sizeof(VkBufferMemoryBarrier2));
        VkBufferMemoryBarrier2* acquireBufferInfo = (VkBufferMemoryBarrier2*)(acquireDependencyInfo + 1);

        acquireBufferInfo->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
        acquireBufferInfo->pNext = nullptr;
        acquireBufferInfo->srcStageMask = 0;  // IGNORED because it is a queue family acquire operation
        acquireBufferInfo->srcAccessMask = 0; // IGNORED because it is a queue family acquire operation
        acquireBufferInfo->dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
        acquireBufferInfo->dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
        acquireBufferInfo->srcQueueFamilyIndex = vk_state->transferQueue.index;
        acquireBufferInfo->dstQueueFamilyIndex = vk_state->graphicsQueue.index;
        acquireBufferInfo->buffer = buffer->handle;
        acquireBufferInfo->offset = 0;
        acquireBufferInfo->size = buffer->size;

        acquireDependencyInfo->sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        acquireDependencyInfo->pNext = nullptr;
        acquireDependencyInfo->dependencyFlags = 0;
        acquireDependencyInfo->memoryBarrierCount = 0;
        acquireDependencyInfo->pMemoryBarriers = nullptr;
        acquireDependencyInfo->bufferMemoryBarrierCount = 1;
        acquireDependencyInfo->pBufferMemoryBarriers = acquireBufferInfo;
        acquireDependencyInfo->imageMemoryBarrierCount = 0;
        acquireDependencyInfo->pImageMemoryBarriers = nullptr;

        VkDependencyInfoRefDarrayPushback(vk_state->requestedQueueAcquisitionOperationsDarray, &acquireDependencyInfo);

        // Making sure the staging buffer and memory get deleted when their corresponding command buffer is completed
        ResourceDestructionInfo bufferDestructionInfo = {};
        bufferDestructionInfo.resource = stagingBuffer;
        bufferDestructionInfo.Destructor = VulkanBufferDestructor;
        bufferDestructionInfo.signalValue = signaledValue;

        ResourceDestructionInfo memoryDestructionInfo = {};
        memoryDestructionInfo.resource = stagingMemory;
        memoryDestructionInfo.Destructor = VulkanMemoryDestructor;
        memoryDestructionInfo.signalValue = signaledValue;

        ResourceDestructionInfoDarrayPushback(vk_state->transferQueue.resourcesPendingDestructionDarray, &bufferDestructionInfo);
        ResourceDestructionInfoDarrayPushback(vk_state->transferQueue.resourcesPendingDestructionDarray, &memoryDestructionInfo);
    }

    return clientBuffer;
}

// Updates the data in the vertex buffer, note that if size is less than the size of the buffer, whatever else was in the buffer is undefined
void VertexBufferUpdate(VertexBuffer clientBuffer, void* vertices, u64 size)
{
    // TODO: this is a slow copy, an option should be added to vertex buffer create that creates 3 buffers and an upload buffer so data can be uploaded without halting the cpu and gpu
    VulkanVertexBuffer* buffer = clientBuffer.internalState;
    GRASSERT_MSG(size <= buffer->size, "Tried to update vertex buffer with more vertices than vertex buffer can hold");

    // ================ Staging buffer =========================
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    CreateBuffer(buffer->size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory);

    // ================= copying data into staging buffer ===============================
    void* data;
    vkMapMemory(vk_state->device, stagingMemory, 0, buffer->size, 0, &data);
    MemoryCopy(data, vertices, (size_t)buffer->size);
    vkUnmapMemory(vk_state->device, stagingMemory);

    // Creating the buffer memory barrier for the queue family release operation
    VkBufferMemoryBarrier2 releaseBufferInfo = {};
    releaseBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    releaseBufferInfo.pNext = nullptr;
    releaseBufferInfo.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    releaseBufferInfo.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    releaseBufferInfo.dstStageMask = 0;  // IGNORED because it is a queue family release operation
    releaseBufferInfo.dstAccessMask = 0; // IGNORED because it is a queue family release operation
    releaseBufferInfo.srcQueueFamilyIndex = vk_state->transferQueue.index;
    releaseBufferInfo.dstQueueFamilyIndex = vk_state->graphicsQueue.index;
    releaseBufferInfo.buffer = buffer->handle;
    releaseBufferInfo.offset = 0;
    releaseBufferInfo.size = buffer->size;

    VkDependencyInfo releaseDependencyInfo = {};
    releaseDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    releaseDependencyInfo.pNext = nullptr;
    releaseDependencyInfo.dependencyFlags = 0;
    releaseDependencyInfo.memoryBarrierCount = 0;
    releaseDependencyInfo.pMemoryBarriers = nullptr;
    releaseDependencyInfo.bufferMemoryBarrierCount = 1;
    releaseDependencyInfo.pBufferMemoryBarriers = &releaseBufferInfo;
    releaseDependencyInfo.imageMemoryBarrierCount = 0;
    releaseDependencyInfo.pImageMemoryBarriers = nullptr;

    // Submitting the semaphore that can let other queues know when this vertex buffer has been uploaded
    vk_state->vertexUploadSemaphore.submitValue++;
    VkSemaphoreSubmitInfo signalSemaphoreSubmitInfo = {};
    signalSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signalSemaphoreSubmitInfo.pNext = nullptr;
    signalSemaphoreSubmitInfo.semaphore = vk_state->vertexUploadSemaphore.handle;
    signalSemaphoreSubmitInfo.value = vk_state->vertexUploadSemaphore.submitValue;
    signalSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    signalSemaphoreSubmitInfo.deviceIndex = 0;

    // Submitting the semaphore that makes the upload wait until the buffer isn't being used for rendering anymore
    VkSemaphoreSubmitInfo waitSemaphoreSubmitInfo = {};
    waitSemaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    waitSemaphoreSubmitInfo.pNext = nullptr;
    // TODO: Use a different semaphore that gets signaled when the gfx pipeline has consumed the vertex buffer, because now we have to wait for the entire pipeline to finish
    // TODO: before we can upload which is unnecessary
    waitSemaphoreSubmitInfo.semaphore = vk_state->frameSemaphore.handle; // When the vertex buffer has been consumed by the pipeline we can begin uploading
    waitSemaphoreSubmitInfo.value = vk_state->frameSemaphore.submitValue;
    waitSemaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    waitSemaphoreSubmitInfo.deviceIndex = 0;

    // Copying the buffer to the GPU - this function returns before the buffer is actually copied
    u64 signaledValue; // This value indicates at what point in time the command buffer that executes the copy is finished, used for deleting staging buffer and memory
    CopyBufferAndTransitionQueue(buffer->handle, stagingBuffer, 1, &waitSemaphoreSubmitInfo, 1, &signalSemaphoreSubmitInfo, &releaseDependencyInfo, buffer->size, &signaledValue);

    // Creating the buffer memory barrier for the queue family acquire operation
    // This is put in the requestedQueueAcquisitionOperations list and will be submitted as a command in the draw loop,
    // also synced with vertex upload semaphore, so ownership isn't acquired before it is released
    VkDependencyInfo* acquireDependencyInfo = Alloc(vk_state->resourceAcquisitionPool, sizeof(VkDependencyInfo) + sizeof(VkBufferMemoryBarrier2));
    VkBufferMemoryBarrier2* acquireBufferInfo = (VkBufferMemoryBarrier2*)(acquireDependencyInfo + 1);

    acquireBufferInfo->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    acquireBufferInfo->pNext = nullptr;
    acquireBufferInfo->srcStageMask = 0;  // IGNORED because it is a queue family acquire operation
    acquireBufferInfo->srcAccessMask = 0; // IGNORED because it is a queue family acquire operation
    acquireBufferInfo->dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
    acquireBufferInfo->dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    acquireBufferInfo->srcQueueFamilyIndex = vk_state->transferQueue.index;
    acquireBufferInfo->dstQueueFamilyIndex = vk_state->graphicsQueue.index;
    acquireBufferInfo->buffer = buffer->handle;
    acquireBufferInfo->offset = 0;
    acquireBufferInfo->size = buffer->size;

    acquireDependencyInfo->sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    acquireDependencyInfo->pNext = nullptr;
    acquireDependencyInfo->dependencyFlags = 0;
    acquireDependencyInfo->memoryBarrierCount = 0;
    acquireDependencyInfo->pMemoryBarriers = nullptr;
    acquireDependencyInfo->bufferMemoryBarrierCount = 1;
    acquireDependencyInfo->pBufferMemoryBarriers = acquireBufferInfo;
    acquireDependencyInfo->imageMemoryBarrierCount = 0;
    acquireDependencyInfo->pImageMemoryBarriers = nullptr;

    VkDependencyInfoRefDarrayPushback(vk_state->requestedQueueAcquisitionOperationsDarray, &acquireDependencyInfo);

    // Making sure the staging buffer and memory get deleted when their corresponding command buffer is completed
    ResourceDestructionInfo bufferDestructionInfo = {};
    bufferDestructionInfo.resource = stagingBuffer;
    bufferDestructionInfo.Destructor = VulkanBufferDestructor;
    bufferDestructionInfo.signalValue = signaledValue;

    ResourceDestructionInfo memoryDestructionInfo = {};
    memoryDestructionInfo.resource = stagingMemory;
    memoryDestructionInfo.Destructor = VulkanMemoryDestructor;
    memoryDestructionInfo.signalValue = signaledValue;

    ResourceDestructionInfoDarrayPushback(vk_state->transferQueue.resourcesPendingDestructionDarray, &bufferDestructionInfo);
    ResourceDestructionInfoDarrayPushback(vk_state->transferQueue.resourcesPendingDestructionDarray, &memoryDestructionInfo);
}

static void VertexBufferDestructor(void* resource)
{
    VulkanVertexBuffer* buffer = (VulkanVertexBuffer*)resource;

    vkDestroyBuffer(vk_state->device, buffer->handle, vk_state->vkAllocator);
    vkFreeMemory(vk_state->device, buffer->memory, vk_state->vkAllocator);

    Free(vk_state->rendererAllocator, buffer);
}

void VertexBufferDestroy(VertexBuffer clientBuffer)
{
    ResourceDestructionInfo vertexBufferDestructionInfo = {};
    vertexBufferDestructionInfo.resource = clientBuffer.internalState;
    vertexBufferDestructionInfo.Destructor = VertexBufferDestructor;
    vertexBufferDestructionInfo.signalValue = vk_state->graphicsQueue.semaphore.submitValue;

    ResourceDestructionInfoDarrayPushback(vk_state->graphicsQueue.resourcesPendingDestructionDarray, &vertexBufferDestructionInfo);
}

IndexBuffer IndexBufferCreate(u32* indices, size_t indexCount)
{
    IndexBuffer clientBuffer;
    clientBuffer.internalState = Alloc(vk_state->rendererAllocator, sizeof(VulkanIndexBuffer));
    VulkanIndexBuffer* buffer = (VulkanIndexBuffer*)clientBuffer.internalState;
    buffer->size = indexCount * sizeof(u32);
    buffer->indexCount = indexCount;

    // ================ Staging buffer =========================
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    CreateBuffer(buffer->size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &stagingBuffer, &stagingMemory);

    // ================= copying data into staging buffer ===============================
    void* data;
    vkMapMemory(vk_state->device, stagingMemory, 0, buffer->size, 0, &data);
    MemoryCopy(data, indices, (size_t)buffer->size);
    vkUnmapMemory(vk_state->device, stagingMemory);

    // ================= creating the actual buffer =========================
    CreateBuffer(buffer->size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &buffer->handle, &buffer->memory);

    // Creating the buffer memory barrier for the queue family release operation
    VkBufferMemoryBarrier2 releaseBufferInfo = {};
    releaseBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    releaseBufferInfo.pNext = nullptr;
    releaseBufferInfo.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    releaseBufferInfo.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    releaseBufferInfo.dstStageMask = 0;  // IGNORED because it is a queue family release operation
    releaseBufferInfo.dstAccessMask = 0; // IGNORED because it is a queue family release operation
    releaseBufferInfo.srcQueueFamilyIndex = vk_state->transferQueue.index;
    releaseBufferInfo.dstQueueFamilyIndex = vk_state->graphicsQueue.index;
    releaseBufferInfo.buffer = buffer->handle;
    releaseBufferInfo.offset = 0;
    releaseBufferInfo.size = buffer->size;

    VkDependencyInfo releaseDependencyInfo = {};
    releaseDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    releaseDependencyInfo.pNext = nullptr;
    releaseDependencyInfo.dependencyFlags = 0;
    releaseDependencyInfo.memoryBarrierCount = 0;
    releaseDependencyInfo.pMemoryBarriers = nullptr;
    releaseDependencyInfo.bufferMemoryBarrierCount = 1;
    releaseDependencyInfo.pBufferMemoryBarriers = &releaseBufferInfo;
    releaseDependencyInfo.imageMemoryBarrierCount = 0;
    releaseDependencyInfo.pImageMemoryBarriers = nullptr;

    // Submitting the semaphore that can let other queues know when this index buffer has been uploaded
    vk_state->indexUploadSemaphore.submitValue++;
    VkSemaphoreSubmitInfo semaphoreSubmitInfo = {};
    semaphoreSubmitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    semaphoreSubmitInfo.pNext = nullptr;
    semaphoreSubmitInfo.semaphore = vk_state->indexUploadSemaphore.handle;
    semaphoreSubmitInfo.value = vk_state->indexUploadSemaphore.submitValue;
    semaphoreSubmitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
    semaphoreSubmitInfo.deviceIndex = 0;

    // Copying the buffer to the GPU - this function returns before the buffer is actually copied
    u64 signaledValue; // This value indicates at what point in time the command buffer that executes the copy is finished, used for deleting staging buffer and memory
    CopyBufferAndTransitionQueue(buffer->handle, stagingBuffer, 0, nullptr, 1, &semaphoreSubmitInfo, &releaseDependencyInfo, buffer->size, &signaledValue);

    // Creating the buffer memory barrier for the queue family acquire operation
    // This is put in the requestedQueueAcquisitionOperations list and will be submitted as a command in the draw loop,
    // also synced with index upload semaphore, so ownership isn't acquired before it is released
    VkDependencyInfo* acquireDependencyInfo = Alloc(vk_state->resourceAcquisitionPool, sizeof(VkDependencyInfo) + sizeof(VkBufferMemoryBarrier2));
    VkBufferMemoryBarrier2* acquireBufferInfo = (VkBufferMemoryBarrier2*)(acquireDependencyInfo + 1);

    acquireBufferInfo->sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    acquireBufferInfo->pNext = nullptr;
    acquireBufferInfo->srcStageMask = 0;  // IGNORED because it is a queue family release operation
    acquireBufferInfo->srcAccessMask = 0; // IGNORED because it is a queue family release operation
    acquireBufferInfo->dstStageMask = VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT;
    acquireBufferInfo->dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
    acquireBufferInfo->srcQueueFamilyIndex = vk_state->transferQueue.index;
    acquireBufferInfo->dstQueueFamilyIndex = vk_state->graphicsQueue.index;
    acquireBufferInfo->buffer = buffer->handle;
    acquireBufferInfo->offset = 0;
    acquireBufferInfo->size = buffer->size;

    acquireDependencyInfo->sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    acquireDependencyInfo->pNext = nullptr;
    acquireDependencyInfo->dependencyFlags = 0;
    acquireDependencyInfo->memoryBarrierCount = 0;
    acquireDependencyInfo->pMemoryBarriers = nullptr;
    acquireDependencyInfo->bufferMemoryBarrierCount = 1;
    acquireDependencyInfo->pBufferMemoryBarriers = acquireBufferInfo;
    acquireDependencyInfo->imageMemoryBarrierCount = 0;
    acquireDependencyInfo->pImageMemoryBarriers = nullptr;

    VkDependencyInfoRefDarrayPushback(vk_state->requestedQueueAcquisitionOperationsDarray, &acquireDependencyInfo);

    // Making sure the staging buffer and memory get deleted when their corresponding command buffer is completed
    ResourceDestructionInfo bufferDestructionInfo = {};
    bufferDestructionInfo.resource = stagingBuffer;
    bufferDestructionInfo.Destructor = VulkanBufferDestructor;
    bufferDestructionInfo.signalValue = signaledValue;

    ResourceDestructionInfo memoryDestructionInfo = {};
    memoryDestructionInfo.resource = stagingMemory;
    memoryDestructionInfo.Destructor = VulkanMemoryDestructor;
    memoryDestructionInfo.signalValue = signaledValue;

    ResourceDestructionInfoDarrayPushback(vk_state->transferQueue.resourcesPendingDestructionDarray, &bufferDestructionInfo);
    ResourceDestructionInfoDarrayPushback(vk_state->transferQueue.resourcesPendingDestructionDarray, &memoryDestructionInfo);

    return clientBuffer;
}

static void IndexBufferDestructor(void* resource)
{
    VulkanIndexBuffer* buffer = (VulkanIndexBuffer*)resource;

    vkDestroyBuffer(vk_state->device, buffer->handle, vk_state->vkAllocator);
    vkFreeMemory(vk_state->device, buffer->memory, vk_state->vkAllocator);

    Free(vk_state->rendererAllocator, buffer);
}

void IndexBufferDestroy(IndexBuffer clientBuffer)
{
    ResourceDestructionInfo indexBufferDestructionInfo = {};
    indexBufferDestructionInfo.resource = clientBuffer.internalState;
    indexBufferDestructionInfo.Destructor = IndexBufferDestructor;
    indexBufferDestructionInfo.signalValue = vk_state->graphicsQueue.semaphore.submitValue;

    ResourceDestructionInfoDarrayPushback(vk_state->graphicsQueue.resourcesPendingDestructionDarray, &indexBufferDestructionInfo);
}
