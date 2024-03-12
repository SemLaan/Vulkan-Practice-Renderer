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
	VkMemoryPropertyFlags	properties;
} VulkanCreateImageParameters;


bool CreateImage(VulkanCreateImageParameters* pCreateParameters, VulkanImage* vulkanImage);
bool CreateImageView(VulkanImage* pImage, VkImageAspectFlags aspectMask, VkImageView* pImageView);

///TODO: create image view helper function maybe? since swapchain also needs it
