#include "vulkan_transfer.h"

#include "vulkan_command_buffer.h"
#include "core/engine.h"

#define COPY_OPERATIONS_DARRAY_START_CAPACITY 20
#define MAX_COPY_REGIONS 40

void VulkanTransferInit()
{
	vk_state->transferState.bufferCopyOperations = VulkanBufferCopyDataDarrayCreate(COPY_OPERATIONS_DARRAY_START_CAPACITY, vk_state->rendererAllocator);
	vk_state->transferState.bufferToImageCopyOperations = VulkanBufferToImageUploadDataDarrayCreate(COPY_OPERATIONS_DARRAY_START_CAPACITY, vk_state->rendererAllocator);
	for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		AllocateCommandBuffer(&vk_state->transferQueue, &vk_state->transferState.transferCommandBuffers[i]);
	}

	vk_state->transferState.slowestTransferMethod = TRANSFER_METHOD_UNSYNCHRONIZED;
	vk_state->transferState.uploadAcquireDependencyInfo = nullptr;

	VkSemaphoreTypeCreateInfo semaphoreTypeInfo = {};
	semaphoreTypeInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	semaphoreTypeInfo.pNext = 0;
	semaphoreTypeInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	semaphoreTypeInfo.initialValue = MAX_FRAMES_IN_FLIGHT;

	VkSemaphoreCreateInfo timelineSemaphoreCreateInfo = {};
	timelineSemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	timelineSemaphoreCreateInfo.pNext = &semaphoreTypeInfo;
	timelineSemaphoreCreateInfo.flags = 0;

	VK_CHECK(vkCreateSemaphore(vk_state->device, &timelineSemaphoreCreateInfo, vk_state->vkAllocator, &vk_state->transferState.uploadSemaphore.handle));
	vk_state->transferState.uploadSemaphore.submitValue = MAX_FRAMES_IN_FLIGHT;
}

void VulkanTransferShutdown()
{
	vkDestroySemaphore(vk_state->device, vk_state->transferState.uploadSemaphore.handle, vk_state->vkAllocator);
	DarrayDestroy(vk_state->transferState.bufferCopyOperations);
	DarrayDestroy(vk_state->transferState.bufferToImageCopyOperations);
}

static inline bool CheckDstBufferOverlap(VkBufferCopy* a, VkBufferCopy* b)
{
	return (b->dstOffset + b->size > a->dstOffset &&
			b->dstOffset < a->dstOffset + a->size);
}

void VulkanCommitTransfers()
{
	ResetAndBeginCommandBuffer(vk_state->transferState.transferCommandBuffers[vk_state->currentInFlightFrameIndex]);
	VkCommandBuffer currentCommandBuffer = vk_state->transferState.transferCommandBuffers[vk_state->currentInFlightFrameIndex].handle;

	// ================================================== Copying buffers
	VulkanBufferCopyDataDarray* bufferCopyOperations = vk_state->transferState.bufferCopyOperations;

	// Doing all the copy operations in reverse order and cutting out copy regions that get overwritten by copies that were submitted after them
	// This prevents WAW hazards and saves copies at the cost of some CPU time, but the alternative would be memory barriers and duplicate copies
	for (i32 i = bufferCopyOperations->size - 1; i >= 0; i--)
	{
		VkBufferCopy copyRegions[MAX_COPY_REGIONS] = {};
		copyRegions[0] = bufferCopyOperations->data[i].copyRegion;
		u32 copyRegionCount = 1;

		// Looping over the copy operations that were submitted after this one to check whether a part of it gets overwritten, so that we can cancel that part of the copy
		for (i32 j = bufferCopyOperations->size - 1; j > i; j--)
		{
			// Looping over all the copy regions of the current copy, this starts as one, but since the copy region can be split by other copies that were checked before we now need to check each split region
			for (u32 k = 0; k < copyRegionCount; k++)
			{
				// If the copy regions and dst buffers overlap
				if (bufferCopyOperations->data[i].dstBuffer == bufferCopyOperations->data[j].dstBuffer && CheckDstBufferOverlap(&copyRegions[k], &bufferCopyOperations->data[j].copyRegion))
				{
					VkDeviceSize kEndOffset = copyRegions[k].dstOffset + copyRegions[k].size;
					VkDeviceSize jEndOffset = bufferCopyOperations->data[j].copyRegion.dstOffset + bufferCopyOperations->data[j].copyRegion.size;
					// Checking every type of overlap and adjusting the copy region accordingly
					if (copyRegions[k].dstOffset >= bufferCopyOperations->data[j].copyRegion.dstOffset && kEndOffset <= jEndOffset) // if the regions are equal or if the new copy region is completely within the already copied one, remove the new region
					{
						if (copyRegionCount - k - 1 > 0)
							MemoryCopy(copyRegions + k, copyRegions + k + 1, sizeof(*copyRegions) * copyRegionCount - k - 1);
						copyRegionCount--;
					}
					else if (copyRegions[k].dstOffset < bufferCopyOperations->data[j].copyRegion.dstOffset && kEndOffset > jEndOffset) // if the region completely envelops the already copied one, split the new region
					{
						// New split region on the right side of the old region
						VkBufferCopy newSplitRegion = {};
						newSplitRegion.dstOffset = jEndOffset;
						newSplitRegion.srcOffset = copyRegions[k].srcOffset + (jEndOffset - copyRegions[k].dstOffset);
						newSplitRegion.size = kEndOffset - jEndOffset;

						// New split region on the left side of the old region
						copyRegions[k].size = bufferCopyOperations->data[j].copyRegion.dstOffset - copyRegions[k].dstOffset;

						// Copying the new split region on the right into the array
						GRASSERT_DEBUG(copyRegionCount < MAX_COPY_REGIONS);
						copyRegions[copyRegionCount] = newSplitRegion;
						copyRegionCount++;
					}
					else if (copyRegions[k].dstOffset < bufferCopyOperations->data[j].copyRegion.dstOffset) // if the right side of the new region overlaps the old region, cut the right side of the new region off
					{
						copyRegions[k].size = bufferCopyOperations->data[j].copyRegion.dstOffset - copyRegions[k].dstOffset;
					}
					else // if the left side of the new region overlaps the old region, cut the left side of the new region off
					{
						copyRegions[k].srcOffset = copyRegions[k].srcOffset + (jEndOffset - copyRegions[k].dstOffset);
						copyRegions[k].dstOffset = jEndOffset;
						copyRegions[k].size = kEndOffset - jEndOffset;
					}
				}
			}
		}

		if (copyRegionCount > 0)
			vkCmdCopyBuffer(currentCommandBuffer, bufferCopyOperations->data[i].srcBuffer, bufferCopyOperations->data[i].dstBuffer, copyRegionCount, copyRegions);
	}

	ArenaMarker marker = ArenaGetMarker(grGlobals->frameArena);
	VkBufferMemoryBarrier2* bufferReleaseInfos = ArenaAlloc(grGlobals->frameArena, sizeof(*bufferReleaseInfos) * bufferCopyOperations->size);
	VkBufferMemoryBarrier2* bufferAcquireInfos = ArenaAlloc(vk_state->vkFrameArena, sizeof(*bufferAcquireInfos) * bufferCopyOperations->size);
	u32 bufferMemoryBarrierCount = 0;

	for (u32 i = 0; i < bufferCopyOperations->size; i++)
	{
		if (bufferCopyOperations->data[i].dstBuffer == nullptr)
			continue;

		VkDeviceSize startOffsetA = bufferCopyOperations->data[i].copyRegion.dstOffset;
		VkDeviceSize endOffsetA = startOffsetA + bufferCopyOperations->data[i].copyRegion.size - 1;

		for (u32 j = 0; j < bufferCopyOperations->size; j++)
		{
			if (bufferCopyOperations->data[i].dstBuffer != bufferCopyOperations->data[j].dstBuffer || i == j)
				continue;

			VkDeviceSize startOffsetB = bufferCopyOperations->data[j].copyRegion.dstOffset;
			VkDeviceSize endOffsetB = startOffsetB + bufferCopyOperations->data[j].copyRegion.size - 1;

			// Test if the copy ranges overlap, and if so merge them to make one memory barrier for both of them
			if ((startOffsetB >= startOffsetA && startOffsetB <= endOffsetA) ||
				(endOffsetB >= startOffsetA && endOffsetB <= endOffsetA))
			{
				startOffsetA = startOffsetA < startOffsetB ? startOffsetA : startOffsetB;
				endOffsetA = endOffsetA > endOffsetB ? endOffsetA : endOffsetB;
				bufferCopyOperations->data[j].dstBuffer = nullptr;
			}
		}

		bufferReleaseInfos[bufferMemoryBarrierCount].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		bufferReleaseInfos[bufferMemoryBarrierCount].pNext = nullptr;
		bufferReleaseInfos[bufferMemoryBarrierCount].srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		bufferReleaseInfos[bufferMemoryBarrierCount].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		bufferReleaseInfos[bufferMemoryBarrierCount].dstStageMask = 0;  // IGNORED because it is a queue family release operation
		bufferReleaseInfos[bufferMemoryBarrierCount].dstAccessMask = 0; // IGNORED because it is a queue family release operation
		bufferReleaseInfos[bufferMemoryBarrierCount].srcQueueFamilyIndex = vk_state->transferQueue.index;
		bufferReleaseInfos[bufferMemoryBarrierCount].dstQueueFamilyIndex = vk_state->graphicsQueue.index;
		bufferReleaseInfos[bufferMemoryBarrierCount].buffer = bufferCopyOperations->data[i].dstBuffer;
		bufferReleaseInfos[bufferMemoryBarrierCount].offset = startOffsetA;
		bufferReleaseInfos[bufferMemoryBarrierCount].size = endOffsetA - startOffsetA + 1;

		bufferAcquireInfos[bufferMemoryBarrierCount].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		bufferAcquireInfos[bufferMemoryBarrierCount].pNext = nullptr;
		bufferAcquireInfos[bufferMemoryBarrierCount].srcStageMask = 0;   // IGNORED because it is a queue family acquire operation
		bufferAcquireInfos[bufferMemoryBarrierCount].srcAccessMask = 0;  // IGNORED because it is a queue family acquire operation
		bufferAcquireInfos[bufferMemoryBarrierCount].dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		bufferAcquireInfos[bufferMemoryBarrierCount].dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
		bufferAcquireInfos[bufferMemoryBarrierCount].srcQueueFamilyIndex = vk_state->transferQueue.index;
		bufferAcquireInfos[bufferMemoryBarrierCount].dstQueueFamilyIndex = vk_state->graphicsQueue.index;
		bufferAcquireInfos[bufferMemoryBarrierCount].buffer = bufferCopyOperations->data[i].dstBuffer;
		bufferAcquireInfos[bufferMemoryBarrierCount].offset = startOffsetA;
		bufferAcquireInfos[bufferMemoryBarrierCount].size = endOffsetA - startOffsetA + 1;

		bufferMemoryBarrierCount++;
	}

	VkDependencyInfo releaseDependencyInfo = {};
	releaseDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	releaseDependencyInfo.pNext = nullptr;
	releaseDependencyInfo.dependencyFlags = 0;
	releaseDependencyInfo.memoryBarrierCount = 0;
	releaseDependencyInfo.pMemoryBarriers = nullptr;
	releaseDependencyInfo.bufferMemoryBarrierCount = bufferMemoryBarrierCount;
	releaseDependencyInfo.pBufferMemoryBarriers = bufferReleaseInfos;
	releaseDependencyInfo.imageMemoryBarrierCount = 0;
	releaseDependencyInfo.pImageMemoryBarriers = nullptr;

	vkCmdPipelineBarrier2(currentCommandBuffer, &releaseDependencyInfo);

	// =============================================== Copying images
	VulkanBufferToImageUploadDataDarray* bufferToImageCopyOperations = vk_state->transferState.bufferToImageCopyOperations;

	VkImageMemoryBarrier2* imageTransitionInfos = ArenaAlloc(grGlobals->frameArena, sizeof(*imageTransitionInfos) * bufferToImageCopyOperations->size);
	VkImageMemoryBarrier2* imageReleaseInfos = ArenaAlloc(grGlobals->frameArena, sizeof(*imageReleaseInfos) * bufferToImageCopyOperations->size);
	VkImageMemoryBarrier2* imageAcquireInfos = ArenaAlloc(vk_state->vkFrameArena, sizeof(*imageAcquireInfos) * bufferToImageCopyOperations->size);

	for (u32 i = 0; i < bufferToImageCopyOperations->size; i++)
	{
		imageTransitionInfos[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		imageTransitionInfos[i].pNext = nullptr;
		imageTransitionInfos[i].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
		imageTransitionInfos[i].srcAccessMask = 0;
		imageTransitionInfos[i].dstStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
		imageTransitionInfos[i].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imageTransitionInfos[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageTransitionInfos[i].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageTransitionInfos[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageTransitionInfos[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageTransitionInfos[i].image = bufferToImageCopyOperations->data[i].dstImage;
		imageTransitionInfos[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageTransitionInfos[i].subresourceRange.baseMipLevel = 0;
		imageTransitionInfos[i].subresourceRange.levelCount = 1;
		imageTransitionInfos[i].subresourceRange.baseArrayLayer = 0;
		imageTransitionInfos[i].subresourceRange.layerCount = 1;
	}

	VkDependencyInfo transitionDependencyInfo = {};
	transitionDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	transitionDependencyInfo.pNext = nullptr;
	transitionDependencyInfo.dependencyFlags = 0;
	transitionDependencyInfo.memoryBarrierCount = 0;
	transitionDependencyInfo.bufferMemoryBarrierCount = 0;
	transitionDependencyInfo.imageMemoryBarrierCount = bufferToImageCopyOperations->size;
	transitionDependencyInfo.pImageMemoryBarriers = imageTransitionInfos;

	vkCmdPipelineBarrier2(currentCommandBuffer, &transitionDependencyInfo);

	for (u32 i = 0; i < bufferToImageCopyOperations->size; i++)
	{
		// Copying the staging buffer into the gpu local image
		VkBufferImageCopy copyRegion = {};
		copyRegion.bufferOffset = 0;
		copyRegion.bufferRowLength = 0;
		copyRegion.bufferImageHeight = 0;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copyRegion.imageSubresource.mipLevel = 0;
		copyRegion.imageSubresource.baseArrayLayer = 0;
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageOffset.x = 0;
		copyRegion.imageOffset.y = 0;
		copyRegion.imageOffset.z = 0;
		copyRegion.imageExtent.width = bufferToImageCopyOperations->data[i].imageWidth;
		copyRegion.imageExtent.height = bufferToImageCopyOperations->data[i].imageHeight;
		copyRegion.imageExtent.depth = 1;
		
		vkCmdCopyBufferToImage(currentCommandBuffer, bufferToImageCopyOperations->data[i].srcBuffer, bufferToImageCopyOperations->data[i].dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		imageReleaseInfos[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		imageReleaseInfos[i].pNext = nullptr;
		imageReleaseInfos[i].srcStageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
		imageReleaseInfos[i].srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		imageReleaseInfos[i].dstStageMask = 0;  // IGNORED because it is a queue family release operation
		imageReleaseInfos[i].dstAccessMask = 0; // IGNORED because it is a queue family release operation
		imageReleaseInfos[i].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageReleaseInfos[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageReleaseInfos[i].srcQueueFamilyIndex = vk_state->transferQueue.index;
		imageReleaseInfos[i].dstQueueFamilyIndex = vk_state->graphicsQueue.index;
		imageReleaseInfos[i].image = bufferToImageCopyOperations->data[i].dstImage;
		imageReleaseInfos[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageReleaseInfos[i].subresourceRange.baseMipLevel = 0;
		imageReleaseInfos[i].subresourceRange.levelCount = 1;
		imageReleaseInfos[i].subresourceRange.baseArrayLayer = 0;
		imageReleaseInfos[i].subresourceRange.layerCount = 1;

		imageAcquireInfos[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		imageAcquireInfos[i].pNext = nullptr;
		imageAcquireInfos[i].srcStageMask = 0;  // IGNORED because it is a queue family acquire operation
		imageAcquireInfos[i].srcAccessMask = 0; // IGNORED because it is a queue family acquire operation
		imageAcquireInfos[i].dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		imageAcquireInfos[i].dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT;
		imageAcquireInfos[i].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageAcquireInfos[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageAcquireInfos[i].srcQueueFamilyIndex = vk_state->transferQueue.index;
		imageAcquireInfos[i].dstQueueFamilyIndex = vk_state->graphicsQueue.index;
		imageAcquireInfos[i].image = bufferToImageCopyOperations->data[i].dstImage;
		imageAcquireInfos[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageAcquireInfos[i].subresourceRange.baseMipLevel = 0;
		imageAcquireInfos[i].subresourceRange.levelCount = 1;
		imageAcquireInfos[i].subresourceRange.baseArrayLayer = 0;
		imageAcquireInfos[i].subresourceRange.layerCount = 1;
	}

	VkDependencyInfo imageReleaseDependencyInfo = {};
	imageReleaseDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	imageReleaseDependencyInfo.pNext = nullptr;
	imageReleaseDependencyInfo.dependencyFlags = 0;
	imageReleaseDependencyInfo.memoryBarrierCount = 0;
	imageReleaseDependencyInfo.bufferMemoryBarrierCount = 0;
	imageReleaseDependencyInfo.imageMemoryBarrierCount = bufferToImageCopyOperations->size;
	imageReleaseDependencyInfo.pImageMemoryBarriers = imageReleaseInfos;

	vkCmdPipelineBarrier2(currentCommandBuffer, &imageReleaseDependencyInfo);

	EndCommandBuffer(vk_state->transferState.transferCommandBuffers[vk_state->currentInFlightFrameIndex]);

	u32 waitSemaphoreCount = 0;
	VkSemaphoreSubmitInfo waitSemaphores[1] = {};

	if (vk_state->transferState.slowestTransferMethod == TRANSFER_METHOD_SYNCHRONIZED_SINGLE_BUFFERED)
	{
		waitSemaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
		waitSemaphores[0].pNext = nullptr;
		waitSemaphores[0].semaphore = vk_state->frameSemaphore.handle;
		waitSemaphores[0].value = vk_state->frameSemaphore.submitValue;
		waitSemaphores[0].stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		waitSemaphores[0].deviceIndex = 0;
		waitSemaphoreCount++;
	}

	const u32 signalSemaphoreCount = 1;
	vk_state->transferState.uploadSemaphore.submitValue++;
	VkSemaphoreSubmitInfo signalSemaphores[1] = {};
	signalSemaphores[0].sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSemaphores[0].pNext = nullptr;
	signalSemaphores[0].semaphore = vk_state->transferState.uploadSemaphore.handle;
	signalSemaphores[0].value = vk_state->transferState.uploadSemaphore.submitValue;
	signalSemaphores[0].stageMask = VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT;
	signalSemaphores[0].deviceIndex = 0;

	SubmitCommandBuffers(waitSemaphoreCount, waitSemaphores, signalSemaphoreCount, signalSemaphores, 1, &vk_state->transferState.transferCommandBuffers[vk_state->currentInFlightFrameIndex], nullptr);

	vk_state->transferState.slowestTransferMethod = TRANSFER_METHOD_UNSYNCHRONIZED;
	
	vk_state->transferState.uploadAcquireDependencyInfo = ArenaAlloc(vk_state->vkFrameArena, sizeof(*vk_state->transferState.uploadAcquireDependencyInfo));
	vk_state->transferState.uploadAcquireDependencyInfo->sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	vk_state->transferState.uploadAcquireDependencyInfo->pNext = nullptr;
	vk_state->transferState.uploadAcquireDependencyInfo->dependencyFlags = 0;
	vk_state->transferState.uploadAcquireDependencyInfo->memoryBarrierCount = 0;
	vk_state->transferState.uploadAcquireDependencyInfo->bufferMemoryBarrierCount = bufferMemoryBarrierCount;
	vk_state->transferState.uploadAcquireDependencyInfo->pBufferMemoryBarriers = bufferAcquireInfos;
	vk_state->transferState.uploadAcquireDependencyInfo->imageMemoryBarrierCount = bufferToImageCopyOperations->size;
	vk_state->transferState.uploadAcquireDependencyInfo->pImageMemoryBarriers = imageAcquireInfos;

	ArenaFreeMarker(grGlobals->frameArena, marker);

	DarraySetSize(vk_state->transferState.bufferCopyOperations, 0);
	DarraySetSize(vk_state->transferState.bufferToImageCopyOperations, 0);
}

void RequestBufferUpload(VulkanBufferCopyData* pCopyRequest, TransferMethod transferMethod)
{
	VulkanBufferCopyDataDarrayPushback(vk_state->transferState.bufferCopyOperations, pCopyRequest);
	if (transferMethod > vk_state->transferState.slowestTransferMethod)
		vk_state->transferState.slowestTransferMethod = transferMethod;
}

void RequestImageUpload(VulkanBufferToImageUploadData* pCopyRequest, TransferMethod transferMethod)
{
	VulkanBufferToImageUploadDataDarrayPushback(vk_state->transferState.bufferToImageCopyOperations, pCopyRequest);
	if (transferMethod > vk_state->transferState.slowestTransferMethod)
		vk_state->transferState.slowestTransferMethod = transferMethod;
}




