#pragma once
#include "defines.h"
#include "vulkan_types.h"
#include "containers/darray.h"


// Creates VkBuffer and VkDeviceMemory objects for a buffer with the specified size and flags
bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags bufferUsageFlags, VkMemoryPropertyFlags memoryPropertyFlags, VkBuffer* out_buffer, VkDeviceMemory* out_memory);

void VulkanBufferDestructor(void* resource);
void VulkanMemoryDestructor(void* resource);

/// TODO: make custom vulkan allocator and move this there
u32 FindMemoryType(u32 typeFilter, VkMemoryPropertyFlags requiredFlags);
