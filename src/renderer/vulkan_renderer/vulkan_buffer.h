#pragma once
#include "defines.h"
#include "vulkan_types.h"
#include "containers/darray.h"

typedef struct BufferDestructionObject
{
	VkBuffer buffer;
	VulkanAllocation* allocation;
} BufferDestructionObject;

void VulkanBufferDestructor(void* resource);

