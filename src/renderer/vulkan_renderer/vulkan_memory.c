#include "vulkan_memory.h"

#include "core/asserts.h"

// Adjustment factors for heap sizes to account for other applications using GPU memory, factors based on amd vulkanised memory allocator slides
// Slide 27: https://www.khronos.org/assets/uploads/developers/library/2018-vulkanised/03-Steven-Tovey-VulkanMemoryManagement_Vulkanised2018.pdf
#define DEFAULT_HEAP_SIZE_ADJUSTMENT_FACTOR 0.8
#define DEVICE_LOCAL_HOST_VISIBLE_HEAP_SIZE_ADJUSTMENT_FACTOR 0.66

#define STATIC_MEMORY_FLAGS (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
#define DYNAMIC_MEMORY_FLAGS (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
#define UPLOAD_MEMORY_FLAGS (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)

#define DEFAULT_GPU_ALLOCATOR_BLOCK_SIZE (32 * MiB)
#define ALLOCATOR_STATE_ALLOCATOR_SIZE (5 * MiB)
#define VULKAN_MEMORY_BLOCK_NODE_COUNT 20

#define LARGE_BUFFER_ALLOCATION_THRESHOLD KiB

static VkMemoryPropertyFlags memoryTypeLUT[3] = {STATIC_MEMORY_FLAGS, UPLOAD_MEMORY_FLAGS, DYNAMIC_MEMORY_FLAGS};

static const char* GetMemoryScaleString(u64 bytes, u64* out_scale)
{
    if (bytes < KiB)
    {
        *out_scale = 1;
        return "B";
    }
    else if (bytes < MiB)
    {
        *out_scale = KiB;
        return "KiB";
    }
    else if (bytes < GiB)
    {
        *out_scale = MiB;
        return "MiB";
    }
    else
    {
        *out_scale = GiB;
        return "GiB";
    }
}

static inline void CreateVulkanMemoryBlock(VulkanAllocatorMemoryBlock* out_allocatorBlock, u32 memoryType, u32 heapIndex, VkDeviceSize blockSize)
{
	VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.pNext = nullptr;
    allocateInfo.allocationSize = blockSize;
    allocateInfo.memoryTypeIndex = memoryType;

	out_allocatorBlock->size = blockSize;
	out_allocatorBlock->nodeCount = VULKAN_MEMORY_BLOCK_NODE_COUNT;
	out_allocatorBlock->nodePool = Alloc(vk_state->vkMemory->vulkanAllocatorStateAllocator, sizeof(*out_allocatorBlock->nodePool) * out_allocatorBlock->nodeCount);
	out_allocatorBlock->head = out_allocatorBlock->nodePool;
	out_allocatorBlock->head->address = 0;
	out_allocatorBlock->head->size = blockSize;
	out_allocatorBlock->head->next = nullptr;
	out_allocatorBlock->mappedMemory = nullptr; // will get set at the end of this function if this block is host visible

	vk_state->vkMemory->heapInfos[heapIndex].heapUsage += blockSize;
	GRASSERT(vk_state->vkMemory->heapInfos[heapIndex].heapUsage < vk_state->vkMemory->heapInfos[heapIndex].heapCapacity);

	VK_CHECK(vkAllocateMemory(vk_state->device, &allocateInfo, vk_state->vkAllocator, &out_allocatorBlock->deviceMemory));

	// check if the memory should be mapped, and map if so
	if (vk_state->vkMemory->deviceMemoryProperties.memoryTypes[memoryType].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		vkMapMemory(vk_state->device, out_allocatorBlock->deviceMemory, 0, blockSize, 0, &out_allocatorBlock->mappedMemory);
	}
}

static inline void DestroyVulkanMemoryBlock(VulkanAllocatorMemoryBlock* allocatorBlock, u32 heapIndex)
{
	// remove block size from heap usage
	vk_state->vkMemory->heapInfos[heapIndex].heapUsage -= allocatorBlock->size;

	// Check if memory should be unmapped, and unmap if so
	if (allocatorBlock->mappedMemory)
		vkUnmapMemory(vk_state->device, allocatorBlock->deviceMemory);
	
	allocatorBlock->mappedMemory = nullptr;

	// Free device memory
	vkFreeMemory(vk_state->device, allocatorBlock->deviceMemory, vk_state->vkAllocator);

	// Free node pool
	Free(vk_state->vkMemory->vulkanAllocatorStateAllocator, allocatorBlock->nodePool);
	allocatorBlock->nodePool = nullptr;
}

void VulkanMemoryInit()
{
	vk_state->vkMemory = AlignedAlloc(vk_state->rendererAllocator, sizeof(*vk_state->vkMemory), CACHE_ALIGN);
	VulkanMemoryState* vkMemory = vk_state->vkMemory;

	CreateFreelistAllocator("Vulkan Allocator state allocator", vk_state->rendererAllocator, ALLOCATOR_STATE_ALLOCATOR_SIZE, &vkMemory->vulkanAllocatorStateAllocator, false);

	vkGetPhysicalDeviceMemoryProperties(vk_state->physicalDevice, &vkMemory->deviceMemoryProperties);

	vkMemory->memoryTypeCount = vkMemory->deviceMemoryProperties.memoryTypeCount;
	vkMemory->heapCount = vkMemory->deviceMemoryProperties.memoryHeapCount;
	vkMemory->heapInfos = Alloc(vk_state->rendererAllocator, sizeof(*vkMemory->heapInfos) * vkMemory->heapCount);

	// Printing all the memory heaps and memory types of the device and storing the sizes of the heaps
	for (u32 i = 0; i < vkMemory->deviceMemoryProperties.memoryHeapCount; i++)
	{
		_INFO("Memory heap: %u", i);

		// Logic to be able to print the heap size in MiB or GiB instead of in bytes (because that is unreadable)
		const char* scaleString;
    	u64 scale;
    	scaleString = GetMemoryScaleString(vkMemory->deviceMemoryProperties.memoryHeaps[i].size, &scale);
    	f32 heapSizeScaled = (f32)vkMemory->deviceMemoryProperties.memoryHeaps[i].size / (f32)scale;

		_INFO("Heap size: %.2f%s", heapSizeScaled, scaleString);
		if (vkMemory->deviceMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) { _INFO("DEVICE_LOCAL"); }
		if (vkMemory->deviceMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) { _INFO("MULTI_INSTANCE"); }

		f64 heapSizeAdjustmentFactor = DEFAULT_HEAP_SIZE_ADJUSTMENT_FACTOR;

		for (u32 j = 0; j < vkMemory->deviceMemoryProperties.memoryTypeCount; j++)
		{
			if (vkMemory->deviceMemoryProperties.memoryTypes[j].heapIndex == i)
			{
				// If the heap has mem types that are host visible AND device local, use a different size adjustment factor
				if (vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT && vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
					heapSizeAdjustmentFactor = DEVICE_LOCAL_HOST_VISIBLE_HEAP_SIZE_ADJUSTMENT_FACTOR;

				_INFO("\tMemory type: %u", j);
				if (vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) { _INFO("\t\tDEVICE_LOCAL"); }
				if (vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) { _INFO("\t\tHOST_VISIBLE"); }
				if (vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) { _INFO("\t\tHOST_COHERENT"); }
				if (vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) { _INFO("\t\tHOST_CACHED"); }
				if (vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) { _INFO("\t\tLAZILY_ALLOCATED"); }
				if (vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) { _INFO("\t\tPROTECTED"); }
				if (vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD) { _INFO("\t\tDEVICE_COHERENT_AMD"); }
				if (vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD) { _INFO("\t\tDEVICE_UNCACHED_AMD"); }
				if (vkMemory->deviceMemoryProperties.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV) { _INFO("\t\tRDMA_CAPABLE_NV"); }
			}
		}

		vkMemory->heapInfos[i].heapCapacity = (VkDeviceSize)((f64)vkMemory->deviceMemoryProperties.memoryHeaps[i].size * heapSizeAdjustmentFactor);
		vkMemory->heapInfos[i].heapUsage = 0;
	}

	vkMemory->smallBufferAllocators = Alloc(vk_state->rendererAllocator, sizeof(*vkMemory->smallBufferAllocators) * vkMemory->memoryTypeCount);
	vkMemory->largeBufferAllocators = Alloc(vk_state->rendererAllocator, sizeof(*vkMemory->largeBufferAllocators) * vkMemory->memoryTypeCount);
	vkMemory->imageAllocators = Alloc(vk_state->rendererAllocator, sizeof(*vkMemory->imageAllocators) * vkMemory->memoryTypeCount);

	// Initializing state of all allocators
	for (u32 i = 0; i < vkMemory->deviceMemoryProperties.memoryTypeCount; i++)
	{
		vkMemory->smallBufferAllocators[i].memoryTypeIndex = i;
		vkMemory->smallBufferAllocators[i].heapIndex = vkMemory->deviceMemoryProperties.memoryTypes[i].heapIndex;
		vkMemory->smallBufferAllocators[i].memoryBlockCount = 0;
		vkMemory->smallBufferAllocators[i].memoryBlocks = nullptr;

		vkMemory->largeBufferAllocators[i].memoryTypeIndex = i;
		vkMemory->largeBufferAllocators[i].heapIndex = vkMemory->deviceMemoryProperties.memoryTypes[i].heapIndex;
		vkMemory->largeBufferAllocators[i].memoryBlockCount = 0;
		vkMemory->largeBufferAllocators[i].memoryBlocks = nullptr;

		vkMemory->imageAllocators[i].memoryTypeIndex = i;
		vkMemory->imageAllocators[i].heapIndex = vkMemory->deviceMemoryProperties.memoryTypes[i].heapIndex;
		vkMemory->imageAllocators[i].memoryBlockCount = 0;
		vkMemory->imageAllocators[i].memoryBlocks = nullptr;
	}
}

void VulkanMemoryShutdown()
{
	VulkanMemoryState* vkMemory = vk_state->vkMemory;

	for (u32 i = 0; i < vkMemory->deviceMemoryProperties.memoryTypeCount; i++)
	{
		for (u32 j = 0; j < vkMemory->smallBufferAllocators[i].memoryBlockCount; j++)
		{
			DestroyVulkanMemoryBlock(&vkMemory->smallBufferAllocators[i].memoryBlocks[j], vkMemory->smallBufferAllocators[i].heapIndex);
		}
		
		for (u32 j = 0; j < vkMemory->largeBufferAllocators[i].memoryBlockCount; j++)
		{
			DestroyVulkanMemoryBlock(&vkMemory->largeBufferAllocators[i].memoryBlocks[j], vkMemory->largeBufferAllocators[i].heapIndex);
		}

		for (u32 j = 0; j < vkMemory->imageAllocators[i].memoryBlockCount; j++)
		{
			DestroyVulkanMemoryBlock(&vkMemory->imageAllocators[i].memoryBlocks[j], vkMemory->imageAllocators[i].heapIndex);
		}

		if (vkMemory->smallBufferAllocators[i].memoryBlockCount > 0)
			Free(vkMemory->vulkanAllocatorStateAllocator, vkMemory->smallBufferAllocators[i].memoryBlocks);
		if (vkMemory->largeBufferAllocators[i].memoryBlockCount > 0)
			Free(vkMemory->vulkanAllocatorStateAllocator, vkMemory->largeBufferAllocators[i].memoryBlocks);
		if (vkMemory->imageAllocators[i].memoryBlockCount > 0)
			Free(vkMemory->vulkanAllocatorStateAllocator, vkMemory->imageAllocators[i].memoryBlocks);
	}

	DestroyFreelistAllocator(vkMemory->vulkanAllocatorStateAllocator);

	Free(vk_state->rendererAllocator, vkMemory->smallBufferAllocators);
	Free(vk_state->rendererAllocator, vkMemory->largeBufferAllocators);
	Free(vk_state->rendererAllocator, vkMemory->imageAllocators);

	Free(vk_state->rendererAllocator, vkMemory->heapInfos);
	Free(vk_state->rendererAllocator, vkMemory);
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

    VK_CHECK(vkCreateBuffer(vk_state->device, &bufferCreateInfo, vk_state->vkAllocator, out_buffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(vk_state->device, *out_buffer, &memoryRequirements);

	u32 allocatorIndex = FindMemoryType(memoryRequirements.memoryTypeBits, memoryTypeLUT[memoryType.memoryType]);
	// If the allocation size is larger than large allocation threshold, use the largeBufferAllocators
	if (memoryRequirements.size > LARGE_BUFFER_ALLOCATION_THRESHOLD)
	{
		// Memory type index is the same as allocator index
		VulkanFreelistAllocate(&vk_state->vkMemory->largeBufferAllocators[allocatorIndex], &memoryRequirements, out_allocation);
	}
	else // If the allocation size is smaller than large allocation threshold, use the smallBufferAllocators
	{
		// Memory type index is the same as allocator index
		VulkanFreelistAllocate(&vk_state->vkMemory->smallBufferAllocators[allocatorIndex], &memoryRequirements, out_allocation);
	}

    vkBindBufferMemory(vk_state->device, *out_buffer, out_allocation->deviceMemory, out_allocation->deviceAddress);
}

void BufferDestroy(VkBuffer* buffer, VulkanAllocation* allocation)
{
	vkDestroyBuffer(vk_state->device, *buffer, vk_state->vkAllocator);

	if (allocation->userAllocationSize > LARGE_BUFFER_ALLOCATION_THRESHOLD)
		VulkanFreelistFree(&vk_state->vkMemory->largeBufferAllocators[allocation->memoryType], allocation);
	else
		VulkanFreelistFree(&vk_state->vkMemory->smallBufferAllocators[allocation->memoryType], allocation);
}

void CopyDataToAllocation(VulkanAllocation* allocation, void* data, u64 offset, u64 size)
{
	GRASSERT_DEBUG(allocation->mappedMemory);
	MemoryCopy((u8*)allocation->mappedMemory + offset, data, size);
}

void ImageCreate(VulkanCreateImageParameters* pCreateParams, VkMemoryTypeHolder memoryType, VkImage* out_image, VulkanAllocation* out_allocation)
{
	GRASSERT_DEBUG(memoryType.memoryType != MEMORY_TYPE_UPLOAD); // Don't upload images using images, upload images using buffers

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

	VK_CHECK(vkCreateImage(vk_state->device, &imageCreateInfo, vk_state->vkAllocator, out_image));

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(vk_state->device, *out_image, &memoryRequirements);

	// Memory type index is the same as allocator index
	u32 allocatorIndex = FindMemoryType(memoryRequirements.memoryTypeBits, memoryTypeLUT[memoryType.memoryType]);
	VulkanFreelistAllocate(&vk_state->vkMemory->imageAllocators[allocatorIndex], &memoryRequirements, out_allocation);

	vkBindImageMemory(vk_state->device, *out_image, out_allocation->deviceMemory, out_allocation->deviceAddress);
}

void ImageDestroy(VkImage* image, VulkanAllocation* allocation)
{
	vkDestroyImage(vk_state->device, *image, vk_state->vkAllocator);
	VulkanFreelistFree(&vk_state->vkMemory->imageAllocators[allocation->memoryType], allocation);
}

static inline bool VulkanAllocatorBlockAllocate(VulkanAllocatorMemoryBlock* allocatorBlock, VkMemoryRequirements* memoryRequirements, VulkanAllocation* out_allocation)
{
	VulkanFreelistNode* previousNode = nullptr;
	VulkanFreelistNode* node = allocatorBlock->head;
	while (node)
	{
		// If the freelist node is definitely big enough to hold the allocation, allocate from that node
		if (node->size >= memoryRequirements->size + memoryRequirements->alignment)
		{
			// Setting the allocation data
			out_allocation->deviceMemory = allocatorBlock->deviceMemory;
			out_allocation->deviceAddress = (node->address + memoryRequirements->alignment - 1) & ~((u64)memoryRequirements->alignment - 1);
			out_allocation->userAllocationOffset = out_allocation->deviceAddress - node->address;
			out_allocation->userAllocationSize = memoryRequirements->size;
			out_allocation->mappedMemory = nullptr;
			if (allocatorBlock->mappedMemory)
				out_allocation->mappedMemory = (void*)((u8*)allocatorBlock->mappedMemory + out_allocation->deviceAddress);

			// Updating the freelist node
			node->size -= out_allocation->userAllocationSize + out_allocation->userAllocationOffset;
			// If the node size is now zero, remove the node
			if (node->size == 0)
			{
				if (previousNode)
					previousNode->next = node->next;
				else
					allocatorBlock->head = node->next;
				node->address = 0;
				node->size = 0;
				node->next = nullptr;
			}
			else // If the node size is not zero, just update the node address
			{
				node->address += out_allocation->userAllocationSize + out_allocation->userAllocationOffset;
			}

			return true;
		}

		previousNode = node;
		node = node->next;
	}

	return false;
}

static inline VulkanFreelistNode* GetNodeFromPool(VulkanAllocatorMemoryBlock* allocatorBlock)
{
    for (u32 i = 0; i < allocatorBlock->nodeCount; ++i)
    {
        if (allocatorBlock->nodePool[i].size == 0)
            return allocatorBlock->nodePool + i;
    }

    GRASSERT_MSG(false, "Ran out of pool nodes");
    return nullptr;
}

static inline void ReturnNodeToPool(VulkanFreelistNode* node)
{
    node->address = nullptr;
    node->next = nullptr;
    node->size = 0;
}

static inline void VulkanAllocatorBlockFree(VulkanAllocatorMemoryBlock* allocatorBlock, VulkanAllocation* allocation)
{
	VkDeviceAddress blockAddress = allocation->deviceAddress - allocation->userAllocationOffset;
	VkDeviceSize blockSize = allocation->userAllocationSize + allocation->userAllocationOffset;

	if (!allocatorBlock->head)
	{
		allocatorBlock->head = GetNodeFromPool(allocatorBlock);
		allocatorBlock->head->address = blockAddress;
		allocatorBlock->head->size = blockSize;
		allocatorBlock->head->next = nullptr;
		return;
	}

	VulkanFreelistNode* node = allocatorBlock->head;
	VulkanFreelistNode* previous = nullptr;

	while (node || previous)
	{
		// If freed block sits before the current free node, or we're at the end of the list
		if ((node == nullptr) ? true : node->address > blockAddress)
		{
			// True if previous exists and end of previous aligns with start of freed block
			u8 aligns = previous ? (previous->address + previous->size) == blockAddress : false;
			// True if the end of the freed block aligns with the start of the next node (also checks if node exist in case we are at the end of the list)
			aligns |= node ? ((blockAddress + blockSize) == node->address) << 1 : false;

			// aligns:
			// 00 if nothing aligns
			// 01 if the previous aligns
			// 10 if the next aligns
			// 11 if both align

			VulkanFreelistNode* newNode = nullptr;

			switch (aligns)
			{
			case 0b00: // Nothing aligns ====================
				newNode = GetNodeFromPool(allocatorBlock);
				newNode->next = node;
				newNode->address = blockAddress;
				newNode->size = blockSize;
				if (previous)
					previous->next = newNode;
				else
					allocatorBlock->head = newNode;
				return;
			case 0b01: // Previous aligns ===================
				previous->size += blockSize;
				return;
			case 0b10: // Next aligns =======================
				node->address = blockAddress;
				node->size += blockSize;
				return;
			case 0b11: // Previous and next align ===========
				previous->next = node->next;
				previous->size += blockSize + node->size;
				ReturnNodeToPool(node);
				return;
			}
		}

		// If the block being freed sits after the current node, go to the next node
		previous = node;
		node = node->next;
	}
}

void VulkanFreelistAllocate(VulkanFreelistAllocator* allocator, VkMemoryRequirements* memoryRequirements, VulkanAllocation* out_allocation)
{
	out_allocation->memoryType = allocator->memoryTypeIndex;

	for (u32 i = 0; i < allocator->memoryBlockCount; i++)
	{
		if (VulkanAllocatorBlockAllocate(&allocator->memoryBlocks[i], memoryRequirements, out_allocation))
			return;
	}

	allocator->memoryBlockCount++;
	if (allocator->memoryBlocks == nullptr)
		allocator->memoryBlocks = Alloc(vk_state->vkMemory->vulkanAllocatorStateAllocator, sizeof(*allocator->memoryBlocks));
	else
		allocator->memoryBlocks = Realloc(vk_state->vkMemory->vulkanAllocatorStateAllocator, allocator->memoryBlocks, sizeof(*allocator->memoryBlocks) * allocator->memoryBlockCount);
	
	CreateVulkanMemoryBlock(&allocator->memoryBlocks[allocator->memoryBlockCount-1], allocator->memoryTypeIndex, allocator->heapIndex, DEFAULT_GPU_ALLOCATOR_BLOCK_SIZE);
	bool result = VulkanAllocatorBlockAllocate(&allocator->memoryBlocks[allocator->memoryBlockCount-1], memoryRequirements, out_allocation);
	GRASSERT(result);
}

void VulkanFreelistFree(VulkanFreelistAllocator* allocator, VulkanAllocation* allocation)
{
	for (u32 i = 0; i < allocator->memoryBlockCount; i++)
	{
		if (allocator->memoryBlocks[i].deviceMemory == allocation->deviceMemory)
		{
			VulkanAllocatorBlockFree(&allocator->memoryBlocks[i], allocation);
			return;
		}
	}
	GRASSERT_MSG(false, "Memory free failed, block not found");
}


