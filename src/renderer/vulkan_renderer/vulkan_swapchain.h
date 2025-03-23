#pragma once
#include "defines.h"
#include "vulkan_types.h"


SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

void CreateSwapchain(GrPresentMode requestedPresentMode);

void DestroySwapchain();
