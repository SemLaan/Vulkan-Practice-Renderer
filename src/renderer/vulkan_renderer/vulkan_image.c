// Implements both!
#include "../texture.h"
#include "vulkan_image.h"

#include "vulkan_types.h"
#include "vulkan_buffer.h"
#include "vulkan_command_buffer.h"
#include "vulkan_memory.h"
#include "core/logger.h"
#include "core/asserts.h"
#include "vulkan_transfer.h"
#include "vulkan_utils.h"


void CreateImageView(VulkanImage* pImage, VkImageAspectFlags aspectMask, VkFormat format)
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.pNext = nullptr;
	viewCreateInfo.flags = 0;
	viewCreateInfo.image = pImage->handle;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.subresourceRange.aspectMask = aspectMask;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.levelCount = 1;

	VK_CHECK(vkCreateImageView(vk_state->device, &viewCreateInfo, vk_state->vkAllocator, &pImage->view));
}

Texture TextureCreate(u32 width, u32 height, void* pixels, TextureStorageType textureStorageType)
{
	Texture out_texture = {};
	out_texture.internalState = Alloc(vk_state->rendererAllocator, sizeof(VulkanImage));
	VulkanImage* image = (VulkanImage*)out_texture.internalState;

	size_t size = width * height * TEXTURE_CHANNELS;

	VkFormat chosenFormat;
	if (textureStorageType == TEXTURE_STORAGE_RGBA8SRGB)
		chosenFormat = VK_FORMAT_R8G8B8A8_SRGB;
	else 
		chosenFormat = VK_FORMAT_R8G8B8A8_UNORM;

	VkBuffer stagingBuffer;
	VulkanAllocation stagingAllocation;

	BufferCreate(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemType(MEMORY_TYPE_UPLOAD), &stagingBuffer, &stagingAllocation);
	CopyDataToAllocation(&stagingAllocation, pixels, 0, size);

	VulkanCreateImageParameters createImageParameters = {};
	createImageParameters.width = width;
	createImageParameters.height = height;
	createImageParameters.format = chosenFormat;
	createImageParameters.tiling = VK_IMAGE_TILING_OPTIMAL;
	createImageParameters.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	// Creating the image resource
	ImageCreate(&createImageParameters, MemType(MEMORY_TYPE_STATIC), &image->handle, &image->memory);

	// Transitioning the image for copying, copying the image to gpu, transitioning the image for shader reads, transfering image resource ownership to graphics queue
	VulkanBufferToImageUploadData bufferToImageUploadData = {};
	bufferToImageUploadData.srcBuffer = stagingBuffer;
	bufferToImageUploadData.dstImage = image->handle;
	bufferToImageUploadData.imageWidth = width;
	bufferToImageUploadData.imageHeight = height;

	RequestImageUpload(&bufferToImageUploadData, TRANSFER_METHOD_UNSYNCHRONIZED);

	// Making sure the staging buffer and memory get deleted
	QueueDeferredBufferDestruction(stagingBuffer, &stagingAllocation, DESTRUCTION_TIME_NEXT_FRAME);

	// Creating the image view
	CreateImageView(image, VK_IMAGE_ASPECT_COLOR_BIT, chosenFormat);

	return out_texture;
}

void TextureDestroy(Texture clientTexture)
{
	VulkanImage* image = (VulkanImage*)clientTexture.internalState;

	// Making sure the staging buffer and memory get deleted
	QueueDeferredImageDestruction(image->handle, image->view, &image->memory, DESTRUCTION_TIME_CURRENT_FRAME);

	Free(vk_state->rendererAllocator, image);
}
