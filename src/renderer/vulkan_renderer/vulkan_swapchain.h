#pragma once
#include "defines.h"
#include "vulkan_types.h"


SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

bool CreateSwapchain(RendererState* state);

void DestroySwapchain(RendererState* state);
