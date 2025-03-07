#pragma once
#include "defines.h"
#include "vulkan_types.h"




typedef struct VulkanCreateImageParameters
{
	u32						width;
	u32						height;
	VkFormat				format;
	VkImageTiling			tiling;
	VkImageUsageFlags		usage;
} VulkanCreateImageParameters;

void VulkanMemoryInit();
void VulkanMemoryShutdown();

void BufferCreate(VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, VkMemoryTypeHolder memoryType, VkBuffer* out_buffer, VulkanAllocation* out_allocation);
void BufferDestroy(VkBuffer* buffer, VulkanAllocation* allocation);

void CopyDataToAllocation(VulkanAllocation* allocation, void* data, u64 offset, u64 size);

void ImageCreate(VulkanCreateImageParameters* pCreateParams, VkMemoryTypeHolder memoryType, VkImage* out_image, VulkanAllocation* out_allocation);
void ImageDestroy(VkImage* image, VulkanAllocation* allocation);



