#pragma once

#include "defines.h"
#include "vulkan_types.h"

typedef enum DestructionTime
{
	DESTRUCTION_TIME_CURRENT_FRAME = 0,
	DESTRUCTION_TIME_NEXT_FRAME = 1,
} DestructionTime;

bool CheckRequiredExtensions(u32 requiredExtensionCount, const char** requiredExtensions, u32 availableExtensionCount, VkExtensionProperties* availableExtensions);

bool CheckRequiredLayers(u32 requiredLayerCount, const char** requiredLayers, u32 availableLayerCount, VkLayerProperties* availableLayers);

void InitDeferredResourceDestructionState(DeferResourceDestructionState* state, u32 circularQueueSize);
void ShutdownDeferredResourceDestructionState(DeferResourceDestructionState* state);

void QueueDeferredBufferDestruction(VkBuffer buffer, VulkanAllocation* pAllocation, DestructionTime destructionTime);
void QueueDeferredImageDestruction(VkImage image, VkImageView imageView, VulkanAllocation* pAllocation, DestructionTime destructionTime);

void TryDestroyResourcesPendingDestruction();
