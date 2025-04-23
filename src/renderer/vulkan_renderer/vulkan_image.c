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
#include "core/engine.h"
#include <math.h>
#include <stdio.h>
#include <string.h>


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
	viewCreateInfo.subresourceRange.levelCount = pImage->mipLevels;

	VK_CHECK(vkCreateImageView(vk_state->device, &viewCreateInfo, vk_state->vkAllocator, &pImage->view));
}

void GenerateMips()
{
	ArenaMarker marker = ArenaGetMarker(grGlobals->frameArena);

	VkCommandBuffer currentCommandBuffer = vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex].handle;

	// Setting the first mip level of all the images to TRANSFER_SRC and all the other levels to TRANSFER_DST
	{
		u32 imageTransitionBarrierCount = vk_state->mipGenerationQueue->size * 2;
		VkImageMemoryBarrier2* imageTransitionBarriers = ArenaAlloc(grGlobals->frameArena, sizeof(*imageTransitionBarriers) * imageTransitionBarrierCount);

		for (u32 i = 0; i < vk_state->mipGenerationQueue->size; i++)
		{
			imageTransitionBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			imageTransitionBarriers[i].pNext = nullptr;
			imageTransitionBarriers[i].srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			imageTransitionBarriers[i].srcAccessMask = 0;
			imageTransitionBarriers[i].dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
			imageTransitionBarriers[i].dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
			imageTransitionBarriers[i].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageTransitionBarriers[i].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageTransitionBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageTransitionBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageTransitionBarriers[i].image = vk_state->mipGenerationQueue->data[i]->handle;
			imageTransitionBarriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageTransitionBarriers[i].subresourceRange.baseMipLevel = 0;
			imageTransitionBarriers[i].subresourceRange.levelCount = 1;
			imageTransitionBarriers[i].subresourceRange.baseArrayLayer = 0;
			imageTransitionBarriers[i].subresourceRange.layerCount = 1;

			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].pNext = nullptr;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].srcAccessMask = 0;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].image = vk_state->mipGenerationQueue->data[i]->handle;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].subresourceRange.baseMipLevel = 1;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].subresourceRange.levelCount = vk_state->mipGenerationQueue->data[i]->mipLevels - 1;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].subresourceRange.baseArrayLayer = 0;
			imageTransitionBarriers[i + vk_state->mipGenerationQueue->size].subresourceRange.layerCount = 1;
		}

		VkDependencyInfo dependencyInfo = {};
		dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependencyInfo.pNext = nullptr;
		dependencyInfo.dependencyFlags = 0;
		dependencyInfo.memoryBarrierCount = 0;
		dependencyInfo.bufferMemoryBarrierCount = 0;
		dependencyInfo.imageMemoryBarrierCount = imageTransitionBarrierCount;
		dependencyInfo.pImageMemoryBarriers = imageTransitionBarriers;

		vkCmdPipelineBarrier2(currentCommandBuffer, &dependencyInfo);
	}

	for (u32 i = 0; i < vk_state->mipGenerationQueue->size; i++)
	{
		u32 mipWidth = vk_state->mipGenerationQueue->data[i]->width;
		u32 mipHeight = vk_state->mipGenerationQueue->data[i]->height;

		for (u32 j = 1; j < vk_state->mipGenerationQueue->data[i]->mipLevels; j++)
		{
			u32 nextMipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
			u32 nextMipHeight = mipHeight > 1 ? mipHeight / 2 : 1;

			// Blit previous mip level to current
			VkImageBlit2 blitRegion = {};
			blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
			blitRegion.pNext = nullptr;
			blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blitRegion.srcSubresource.mipLevel = j - 1;
			blitRegion.srcSubresource.baseArrayLayer = 0;
			blitRegion.srcSubresource.layerCount = 1;
			blitRegion.srcOffsets[0] = (VkOffset3D){ 0, 0, 0 };
			blitRegion.srcOffsets[1] = (VkOffset3D){ mipWidth, mipHeight, 1 };
			blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blitRegion.dstSubresource.mipLevel = j;
			blitRegion.dstSubresource.baseArrayLayer = 0;
			blitRegion.dstSubresource.layerCount = 1;
			blitRegion.dstOffsets[0] = (VkOffset3D){ 0, 0, 0 };
			blitRegion.dstOffsets[1] = (VkOffset3D){ nextMipWidth, nextMipHeight, 1 };

			VkBlitImageInfo2 blitInfo = {};
			blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
			blitInfo.pNext = nullptr;
			blitInfo.srcImage = vk_state->mipGenerationQueue->data[i]->handle;
			blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			blitInfo.dstImage = vk_state->mipGenerationQueue->data[i]->handle;
			blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			blitInfo.regionCount = 1;
			blitInfo.pRegions = &blitRegion;
			blitInfo.filter = VK_FILTER_LINEAR;

			vkCmdBlitImage2(currentCommandBuffer, &blitInfo);

			// Transition current mip level from TRANSFER_DST to TRANSFER_SRC
			VkImageMemoryBarrier2 mipTransitionBarrier = {};
			mipTransitionBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			mipTransitionBarrier.pNext = nullptr;
			mipTransitionBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
			mipTransitionBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
			mipTransitionBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
			mipTransitionBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
			mipTransitionBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			mipTransitionBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			mipTransitionBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			mipTransitionBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			mipTransitionBarrier.image = vk_state->mipGenerationQueue->data[i]->handle;
			mipTransitionBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			mipTransitionBarrier.subresourceRange.baseMipLevel = j;
			mipTransitionBarrier.subresourceRange.levelCount = 1;
			mipTransitionBarrier.subresourceRange.baseArrayLayer = 0;
			mipTransitionBarrier.subresourceRange.layerCount = 1;

			VkDependencyInfo dependencyInfo = {};
			dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
			dependencyInfo.pNext = nullptr;
			dependencyInfo.dependencyFlags = 0;
			dependencyInfo.memoryBarrierCount = 0;
			dependencyInfo.bufferMemoryBarrierCount = 0;
			dependencyInfo.imageMemoryBarrierCount = 1;
			dependencyInfo.pImageMemoryBarriers = &mipTransitionBarrier;

			vkCmdPipelineBarrier2(currentCommandBuffer, &dependencyInfo);

			mipWidth = nextMipWidth;
			mipHeight = nextMipHeight;
		}
	}

	// Setting all the image layouts of all the mips to shader read optimal
	{
		u32 imageTransitionBarrierCount = vk_state->mipGenerationQueue->size;
		VkImageMemoryBarrier2* imageTransitionBarriers = ArenaAlloc(grGlobals->frameArena, sizeof(*imageTransitionBarriers) * imageTransitionBarrierCount);

		for (u32 i = 0; i < vk_state->mipGenerationQueue->size; i++)
		{
			imageTransitionBarriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			imageTransitionBarriers[i].pNext = nullptr;
			imageTransitionBarriers[i].srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
			imageTransitionBarriers[i].srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT;
			imageTransitionBarriers[i].dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			imageTransitionBarriers[i].dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
			imageTransitionBarriers[i].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageTransitionBarriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			imageTransitionBarriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageTransitionBarriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			imageTransitionBarriers[i].image = vk_state->mipGenerationQueue->data[i]->handle;
			imageTransitionBarriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageTransitionBarriers[i].subresourceRange.baseMipLevel = 0;
			imageTransitionBarriers[i].subresourceRange.levelCount = vk_state->mipGenerationQueue->data[i]->mipLevels;
			imageTransitionBarriers[i].subresourceRange.baseArrayLayer = 0;
			imageTransitionBarriers[i].subresourceRange.layerCount = 1;
		}

		VkDependencyInfo dependencyInfo = {};
		dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		dependencyInfo.pNext = nullptr;
		dependencyInfo.dependencyFlags = 0;
		dependencyInfo.memoryBarrierCount = 0;
		dependencyInfo.bufferMemoryBarrierCount = 0;
		dependencyInfo.imageMemoryBarrierCount = imageTransitionBarrierCount;
		dependencyInfo.pImageMemoryBarriers = imageTransitionBarriers;

		vkCmdPipelineBarrier2(currentCommandBuffer, &dependencyInfo);
	}

	DarraySetSize(vk_state->mipGenerationQueue, 0);

	ArenaFreeMarker(grGlobals->frameArena, marker);
}

Texture TextureCreate(u32 width, u32 height, void* pixels, TextureStorageType textureStorageType, bool mipmapped)
{
	Texture out_texture = {};
	out_texture.internalState = Alloc(vk_state->rendererAllocator, sizeof(VulkanImage));
	VulkanImage* image = (VulkanImage*)out_texture.internalState;
	image->width = width;
	image->height = height;
	image->mipLevels = mipmapped ? (u32)floorf(log2f(width > height ? width : height)) + 1 : 1;

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
	if (mipmapped)
		createImageParameters.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	createImageParameters.mipLevels = image->mipLevels;

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

	if (mipmapped)
	{
		VulkanImageRefDarrayPushback(vk_state->mipGenerationQueue, &image);
	}

	return out_texture;
}

void TextureDestroy(Texture clientTexture)
{
	VulkanImage* image = (VulkanImage*)clientTexture.internalState;

	// Making sure the staging buffer and memory get deleted
	QueueDeferredImageDestruction(image->handle, image->view, &image->memory, DESTRUCTION_TIME_CURRENT_FRAME);

	Free(vk_state->rendererAllocator, image);
}
