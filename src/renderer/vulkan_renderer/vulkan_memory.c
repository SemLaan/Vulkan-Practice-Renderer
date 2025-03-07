#include "vulkan_memory.h"

#include "core/asserts.h"

#define STATIC_MEMORY_FLAGS (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
#define DYNAMIC_MEMORY_FLAGS (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
#define UPLOAD_MEMORY_FLAGS (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)

static VkMemoryPropertyFlags memoryTypeLUT[3] = {STATIC_MEMORY_FLAGS, UPLOAD_MEMORY_FLAGS, DYNAMIC_MEMORY_FLAGS};

void VulkanMemoryInit()
{
	/*
	vk_state->vkMemory = AlignedAlloc(vk_state->rendererAllocator, sizeof(*vk_state->vkMemory), CACHE_ALIGN);
	VulkanMemoryState* vkMemory = vk_state->vkMemory;

	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(vk_state->physicalDevice, &deviceMemoryProperties);

	// Getting memory type indices
	for (u32 i = 0; i < deviceMemoryProperties.memoryTypeCount; i++)
	{
		deviceMemoryProperties.memoryTypes[i].propertyFlags == STATIC_MEMORY_FLAGS;
		vkMemory->staticAllocator.memoryTypeIndex = i;
		break;
	}

	for (u32 i = 0; i < deviceMemoryProperties.memoryTypeCount; i++)
	{
		deviceMemoryProperties.memoryTypes[i].propertyFlags == DYNAMIC_MEMORY_FLAGS;
		vkMemory->dynamicAllocator.memoryTypeIndex = i;
		break;
	}

	for (u32 i = 0; i < deviceMemoryProperties.memoryTypeCount; i++)
	{
		deviceMemoryProperties.memoryTypes[i].propertyFlags == UPLOAD_MEMORY_FLAGS;
		vkMemory->uploadAllocator.memoryTypeIndex = i;
		break;
	}

	// Allocating VkDeviceMemory for all the allocators from the correct heaps and memory types
	{
		VkMemoryAllocateInfo allocateInfo = {};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.pNext = nullptr;
		//allocateInfo.allocationSize = stagingMemoryRequirements.size;
		//allocateInfo.memoryTypeIndex = FindMemoryType(stagingMemoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		//if (VK_SUCCESS != vkAllocateMemory(vk_state->device, &allocateInfo, vk_state->vkAllocator, out_memory))
		//{
		//	_FATAL("Vulkan device memory allocation failed");
		//	return false;
		//}
	}
	*/
	return;

}

void VulkanMemoryShutdown()
{
	/*
	Free(vk_state->rendererAllocator, vk_state->vkMemory);
	*/
	return;
}

static u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags requiredFlags)
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

void BufferCreate(VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, VkMemoryTypeHolder memoryType, VkBuffer* out_buffer, VulkanAllocation* out_allocation)
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
    }

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(vk_state->device, *out_buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, memoryTypeLUT[memoryType.memoryType]);

    if (VK_SUCCESS != vkAllocateMemory(vk_state->device, &allocateInfo, vk_state->vkAllocator, &out_allocation->deviceMemory))
    {
        _FATAL("Vulkan device memory allocation failed");
    }

    vkBindBufferMemory(vk_state->device, *out_buffer, out_allocation->deviceMemory, 0);

	out_allocation->mappedMemory = nullptr;
	if (memoryType.memoryType != MEMORY_TYPE_STATIC)
	{
		vkMapMemory(vk_state->device, out_allocation->deviceMemory, 0, size, 0, &out_allocation->mappedMemory);
	}
}

void BufferDestroy(VkBuffer* buffer, VulkanAllocation* allocation)
{
	if (allocation->mappedMemory != nullptr)
	{
		vkUnmapMemory(vk_state->device, allocation->deviceMemory);
	}
	vkDestroyBuffer(vk_state->device, *buffer, vk_state->vkAllocator);
    vkFreeMemory(vk_state->device, allocation->deviceMemory, vk_state->vkAllocator);
}

void CopyDataToAllocation(VulkanAllocation* allocation, void* data, u64 offset, u64 size)
{
	GRASSERT_DEBUG(allocation->mappedMemory);
	MemoryCopy((u8*)allocation->mappedMemory + offset, data, size);
}

void ImageCreate(VulkanCreateImageParameters* pCreateParams, VkMemoryTypeHolder memoryType, VkImage* out_image, VulkanAllocation* out_allocation)
{
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = nullptr;
	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = pCreateParams->format;
	imageCreateInfo.extent.width = pCreateParams->width;
	imageCreateInfo.extent.height = pCreateParams->height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = pCreateParams->tiling;
	imageCreateInfo.usage = pCreateParams->usage;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.pQueueFamilyIndices = nullptr;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	if (VK_SUCCESS != vkCreateImage(vk_state->device, &imageCreateInfo, vk_state->vkAllocator, out_image))
	{
		_FATAL("Vulkan image (texture) creation failed");
	}

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(vk_state->device, *out_image, &memoryRequirements);

	VkMemoryAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.allocationSize = memoryRequirements.size;
	allocateInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, memoryTypeLUT[memoryType.memoryType]);

	if (VK_SUCCESS != vkAllocateMemory(vk_state->device, &allocateInfo, vk_state->vkAllocator, &out_allocation->deviceMemory))
	{
		_FATAL("Vulkan image (texture) memory allocation failed");
	}

	vkBindImageMemory(vk_state->device, *out_image, out_allocation->deviceMemory, 0);
}

void ImageDestroy(VkImage* image, VulkanAllocation* allocation)
{
	vkDestroyImage(vk_state->device, *image, vk_state->vkAllocator);
	vkFreeMemory(vk_state->device, allocation->deviceMemory, vk_state->vkAllocator);
}

