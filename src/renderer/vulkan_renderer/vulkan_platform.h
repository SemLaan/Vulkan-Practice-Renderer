#pragma once
#include "defines.h"
#include <vulkan/vulkan.h>
#include "containers/darray.h"


void GetPlatformExtensions(u32* pExtensionNameCount, const char** extensionNames);

bool PlatformCreateSurface(VkInstance instance, VkAllocationCallbacks* allocator, VkSurfaceKHR* out_surface);
