#pragma once
#include "defines.h"
#include "vulkan_types.h"


SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

bool CreateSwapchain(GrPresentMode requestedPresentMode);

void DestroySwapchain();
