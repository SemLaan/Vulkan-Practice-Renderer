#include "vulkan_swapchain.h"
#include "core/platform.h"


SwapchainSupportDetails QuerySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapchainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formatCount, nullptr);
	details.formats = Alloc(vk_state->rendererAllocator, sizeof(*details.formats) * details.formatCount, MEM_TAG_RENDERER_SUBSYS);
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formatCount, (VkSurfaceFormatKHR*)details.formats);

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.presentModeCount, nullptr);
	details.presentModes = Alloc(vk_state->rendererAllocator, sizeof(*details.presentModes) * details.presentModeCount, MEM_TAG_RENDERER_SUBSYS);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.presentModeCount, (VkPresentModeKHR*)details.presentModes);

	return details;
}


bool CreateSwapchain(RendererState* state)
{
	// Getting a swapchain format
	VkSurfaceFormatKHR format = vk_state->swapchainSupport.formats[0];
	for (u32 i = 0; i < vk_state->swapchainSupport.formatCount; ++i)
	{
		VkSurfaceFormatKHR availableFormat = vk_state->swapchainSupport.formats[i];
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			format = availableFormat;
	}

	// Getting a presentation mode
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (u32 i = 0; i < vk_state->swapchainSupport.presentModeCount; ++i)
	{
		VkPresentModeKHR availablePresentMode = vk_state->swapchainSupport.presentModes[i];
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			presentMode = availablePresentMode;
	}

	// Setting swapchain resolution to window size
	vec2i windowSize = GetPlatformWindowSize();
	VkExtent2D swapchainExtent = { (u32)windowSize.x, (u32)windowSize.y };
	// Making sure the swapchain isn't too big or too small
	Free(vk_state->rendererAllocator, vk_state->swapchainSupport.formats);
	Free(vk_state->rendererAllocator, vk_state->swapchainSupport.presentModes);
	vk_state->swapchainSupport = QuerySwapchainSupport(state->physicalDevice, state->surface);
	if (swapchainExtent.width > state->swapchainSupport.capabilities.maxImageExtent.width)
		swapchainExtent.width = state->swapchainSupport.capabilities.maxImageExtent.width;
	if (swapchainExtent.height > state->swapchainSupport.capabilities.maxImageExtent.height)
		swapchainExtent.height = state->swapchainSupport.capabilities.maxImageExtent.height;
	if (swapchainExtent.width < state->swapchainSupport.capabilities.minImageExtent.width)
		swapchainExtent.width = state->swapchainSupport.capabilities.minImageExtent.width;
	if (swapchainExtent.height < state->swapchainSupport.capabilities.minImageExtent.height)
		swapchainExtent.height = state->swapchainSupport.capabilities.minImageExtent.height;

	u32 imageCount = state->swapchainSupport.capabilities.minImageCount + 1;
	if (imageCount > state->swapchainSupport.capabilities.maxImageCount)
		imageCount = state->swapchainSupport.capabilities.maxImageCount;

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.pNext = 0;
	createInfo.flags = 0;
	createInfo.surface = state->surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = format.format;
	createInfo.imageColorSpace = format.colorSpace;
	createInfo.imageExtent = swapchainExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	u32 queueFamilyIndices[2] = { state->graphicsQueue.index, state->presentQueueFamilyIndex };
	if (state->graphicsQueue.index != state->presentQueueFamilyIndex)
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		createInfo.queueFamilyIndexCount = 0;
		createInfo.pQueueFamilyIndices = 0;
	}

	createInfo.preTransform = state->swapchainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	if (VK_SUCCESS != vkCreateSwapchainKHR(state->device, &createInfo, state->vkAllocator, &state->swapchain))
	{
		_FATAL("Vulkan Swapchain creation failed");
		return false;
	}

	state->swapchainFormat = format.format;
	state->swapchainExtent = swapchainExtent;

	vkGetSwapchainImagesKHR(state->device, state->swapchain, &vk_state->swapchainImageCount, 0);
	state->swapchainImages = Alloc(vk_state->rendererBumpAllocator, sizeof(*state->swapchainImages) * vk_state->swapchainImageCount, MEM_TAG_RENDERER_SUBSYS);
	vkGetSwapchainImagesKHR(state->device, state->swapchain, &vk_state->swapchainImageCount, state->swapchainImages);

	state->swapchainImageViews = Alloc(vk_state->rendererBumpAllocator, sizeof(*state->swapchainImageViews) * vk_state->swapchainImageCount, MEM_TAG_RENDERER_SUBSYS);

	for (u32 i = 0; i < vk_state->swapchainImageCount; ++i)
	{
		VkImageViewCreateInfo viewCreateInfo = {};
		viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCreateInfo.pNext = nullptr;
		viewCreateInfo.flags = 0;
		viewCreateInfo.image = state->swapchainImages[i];
		viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCreateInfo.format = state->swapchainFormat;
		viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCreateInfo.subresourceRange.baseArrayLayer = 0;
		viewCreateInfo.subresourceRange.layerCount = 1;
		viewCreateInfo.subresourceRange.baseMipLevel = 0;
		viewCreateInfo.subresourceRange.levelCount = 1;

		if (VK_SUCCESS != vkCreateImageView(state->device, &viewCreateInfo, state->vkAllocator, &state->swapchainImageViews[i]))
		{
			_FATAL("Swapchain image view creation failed");
			return false;
		}
	}

	_TRACE("Vulkan swapchain created");

	return true;
}

void DestroySwapchain(RendererState* state)
{
	if (state->swapchainImageViews)
	{
		for (u32 i = 0; i < vk_state->swapchainImageCount; ++i)
		{
			vkDestroyImageView(state->device, state->swapchainImageViews[i], state->vkAllocator);
		}
	}

	if (state->swapchain)
		vkDestroySwapchainKHR(state->device, state->swapchain, state->vkAllocator);

	if (state->swapchainImages)
		Free(vk_state->rendererBumpAllocator, state->swapchainImages);
	if (state->swapchainImageViews)
		Free(vk_state->rendererBumpAllocator, state->swapchainImageViews);
}
