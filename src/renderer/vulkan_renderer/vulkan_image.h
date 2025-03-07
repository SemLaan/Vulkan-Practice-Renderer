#pragma once
#include "defines.h"
#include "vulkan_types.h"



bool CreateImageView(VulkanImage* pImage, VkImageAspectFlags aspectMask, VkFormat format);

///TODO: create image view helper function maybe? since swapchain also needs it
