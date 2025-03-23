#include "vulkan_utils.h"

#include "vulkan_memory.h"
// TODO: make a custom string thing with strncmp replacement so string.h doesn't have to be included
#include <string.h>

#define RESOURCE_DESTRUCTION_OVERFLOW_DARRAY_STANDARD_CAPACITY 10

bool CheckRequiredExtensions(u32 requiredExtensionCount, const char** requiredExtensions, u32 availableExtensionCount, VkExtensionProperties* availableExtensions)
{
    u32 availableRequiredExtensions = 0;
    for (u32 i = 0; i < requiredExtensionCount; ++i)
    {
        for (u32 j = 0; j < availableExtensionCount; ++j)
        {
            if (0 == strncmp(requiredExtensions[i], availableExtensions[j].extensionName, VK_MAX_EXTENSION_NAME_SIZE))
            {
                availableRequiredExtensions++;
            }
        }
    }

    return availableRequiredExtensions == requiredExtensionCount;
}

bool CheckRequiredLayers(u32 requiredLayerCount, const char** requiredLayers, u32 availableLayerCount, VkLayerProperties* availableLayers)
{
    u32 availableRequiredLayers = 0;
    for (u32 i = 0; i < requiredLayerCount; ++i)
    {
        for (u32 j = 0; j < availableLayerCount; ++j)
        {
            if (0 == strncmp(requiredLayers[i], availableLayers[j].layerName, VK_MAX_EXTENSION_NAME_SIZE))
            {
                availableRequiredLayers++;
            }
        }
    }

    return availableRequiredLayers == requiredLayerCount;
}


void InitDeferredResourceDestructionState(DeferResourceDestructionState* state, u32 circularQueueSize)
{
	ResourceDestructionInfoCircularQueueCreate(&state->destructionQueue, circularQueueSize, vk_state->rendererAllocator);
	state->destructionOverflowDarray = ResourceDestructionInfoDarrayCreate(RESOURCE_DESTRUCTION_OVERFLOW_DARRAY_STANDARD_CAPACITY, vk_state->rendererAllocator);
}

void ShutdownDeferredResourceDestructionState(DeferResourceDestructionState* state)
{
	CircularQueueDestroy(&state->destructionQueue);
	DarrayDestroy(state->destructionOverflowDarray);
}

void QueueDeferredBufferDestruction(VkBuffer buffer, VulkanAllocation* pAllocation, DestructionTime destructionTime)
{
	DeferResourceDestructionState* state = &vk_state->deferredResourceDestruction;
	ResourceDestructionInfo destructionInfo = {};
	destructionInfo.resource0 = buffer;
	destructionInfo.allocation = *pAllocation;
	destructionInfo.signalValue = vk_state->frameSemaphore.submitValue + destructionTime;
	destructionInfo.destructionObjectType = DESTRUCTION_OBJECT_TYPE_BUFFER;
	if (state->destructionQueue.size < state->destructionQueue.capacity)
	{
		ResourceDestructionInfoCircularQueueEnqueue(&state->destructionQueue, &destructionInfo);
	}
	else
	{
		ResourceDestructionInfoDarrayPushback(state->destructionOverflowDarray, &destructionInfo);
	}
}

void QueueDeferredImageDestruction(VkImage image, VkImageView imageView, VulkanAllocation* pAllocation, DestructionTime destructionTime)
{
	DeferResourceDestructionState* state = &vk_state->deferredResourceDestruction;
	ResourceDestructionInfo destructionInfo = {};
	destructionInfo.resource0 = image;
	destructionInfo.resource1 = imageView;
	destructionInfo.allocation = *pAllocation;
	destructionInfo.signalValue = vk_state->frameSemaphore.submitValue + destructionTime;
	destructionInfo.destructionObjectType = DESTRUCTION_OBJECT_TYPE_IMAGE;
	if (state->destructionQueue.size < state->destructionQueue.capacity)
	{
		ResourceDestructionInfoCircularQueueEnqueue(&state->destructionQueue, &destructionInfo);
	}
	else
	{
		ResourceDestructionInfoDarrayPushback(state->destructionOverflowDarray, &destructionInfo);
	}
}

void TryDestroyResourcesPendingDestruction()
{
	DeferResourceDestructionState* state = &vk_state->deferredResourceDestruction;

	u64 semaphoreValue;
	VK_CHECK(vkGetSemaphoreCounterValue(vk_state->device, vk_state->frameSemaphore.handle, &semaphoreValue));

	// While the destruction queue is not empty and the semaphore value of the next item at the back of the queue has been signaled: destroy the resource in that position and dequeue it
	while (state->destructionQueue.size > 0 && state->destructionQueue.data[state->destructionQueue.rear].signalValue <= semaphoreValue)
	{
		// destroy resource
		ResourceDestructionInfo* resource = &state->destructionQueue.data[state->destructionQueue.rear];

		if (resource->destructionObjectType == DESTRUCTION_OBJECT_TYPE_BUFFER)
		{
			VkBuffer buffer = resource->resource0;
			BufferDestroy(&buffer, &resource->allocation);
		}
		else if (resource->destructionObjectType == DESTRUCTION_OBJECT_TYPE_IMAGE)
		{
			vkDestroyImageView(vk_state->device, (VkImageView)resource->resource1, vk_state->vkAllocator);
			VkImage image = resource->resource0;
			ImageDestroy(&image, &resource->allocation);
		}

		CircularQueueDequeue(&state->destructionQueue);
	}

	// Loop from the back to the front so we can pop elements from the array in the loop
	for (i32 i = state->destructionOverflowDarray->size - 1; i >= 0; i--)
	{
		if (state->destructionOverflowDarray->data[i].signalValue > semaphoreValue)
			return;
		
		// destroy resource
		ResourceDestructionInfo* resource = &state->destructionOverflowDarray->data[i];

		if (resource->destructionObjectType == DESTRUCTION_OBJECT_TYPE_BUFFER)
		{
			VkBuffer buffer = resource->resource0;
			BufferDestroy(&buffer, &resource->allocation);
		}
		else if (resource->destructionObjectType == DESTRUCTION_OBJECT_TYPE_IMAGE)
		{
			vkDestroyImageView(vk_state->device, (VkImageView)resource->resource1, vk_state->vkAllocator);
			VkImage image = resource->resource0;
			ImageDestroy(&image, &resource->allocation);
		}

		DarrayPopAt(state->destructionOverflowDarray, i);
		if (state->destructionOverflowDarray->size == 0)
			DarraySetCapacity(state->destructionOverflowDarray, RESOURCE_DESTRUCTION_OVERFLOW_DARRAY_STANDARD_CAPACITY);
	}
}
