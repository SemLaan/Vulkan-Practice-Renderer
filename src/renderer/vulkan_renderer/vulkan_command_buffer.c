#include "vulkan_command_buffer.h"

#include "core/logger.h"
#include "core/asserts.h"

#define MAX_SUBMITTED_COMMAND_BUFFERS 20

DEFINE_DARRAY_TYPE(VkSemaphoreSubmitInfo);
DEFINE_DARRAY_TYPE(VkCommandBufferSubmitInfo);

bool AllocateCommandBuffer(QueueFamily* queueFamily, CommandBuffer* ref_pCommandBuffer)
{
	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.commandPool = queueFamily->commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	if (VK_SUCCESS != vkAllocateCommandBuffers(vk_state->device, &allocateInfo, &ref_pCommandBuffer->handle))
	{
		_FATAL("Failed to allocate command buffer");
		return false;
	}

	ref_pCommandBuffer->queueFamily = queueFamily;

	return true;
}

void FreeCommandBuffer(CommandBuffer commandBuffer)
{
	vkFreeCommandBuffers(vk_state->device, commandBuffer.queueFamily->commandPool, 1, &commandBuffer.handle);
}

bool AllocateAndBeginSingleUseCommandBuffer(QueueFamily* queueFamily, CommandBuffer* ref_pCommandBuffer)
{
	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.commandPool = queueFamily->commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	if (VK_SUCCESS != vkAllocateCommandBuffers(vk_state->device, &allocateInfo, &ref_pCommandBuffer->handle))
	{
		_FATAL("Failed to allocate command buffer");
		return false;
	}

	ref_pCommandBuffer->queueFamily = queueFamily;

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = nullptr;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pInheritanceInfo = nullptr;

	if (VK_SUCCESS != vkBeginCommandBuffer(ref_pCommandBuffer->handle, &beginInfo))
	{
		_FATAL("Failed to start recording command buffer");
		return false;
	}

	return true;
}

static void SingleUseCommandBufferDestructor(void* resource)
{
	CommandBuffer* commandBuffer = (CommandBuffer*)resource;
	vkFreeCommandBuffers(vk_state->device, commandBuffer->queueFamily->commandPool, 1, &commandBuffer->handle);
	Free(vk_state->poolAllocator32B, commandBuffer);
}

bool EndSubmitAndFreeSingleUseCommandBuffer(CommandBuffer commandBuffer, u32 waitSemaphoreCount, VkSemaphoreSubmitInfo* pWaitSemaphoreSubmitInfos, u32 signalSemaphoreCount, VkSemaphoreSubmitInfo* pSignalSemaphoreSubmitInfos, u64* ref_signaledValue)
{
	if (VK_SUCCESS != vkEndCommandBuffer(commandBuffer.handle))
	{
		_FATAL("Failed to stop recording command buffer");
		return false;
	}

	VkCommandBufferSubmitInfo commandBufferInfo = {};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	commandBufferInfo.pNext = nullptr;
	commandBufferInfo.commandBuffer = commandBuffer.handle;
	commandBufferInfo.deviceMask = 0;

	commandBuffer.queueFamily->semaphore.submitValue++;
	VkSemaphoreSubmitInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	semaphoreInfo.pNext = nullptr;
	semaphoreInfo.semaphore = commandBuffer.queueFamily->semaphore.handle;
	semaphoreInfo.value = commandBuffer.queueFamily->semaphore.submitValue;
	semaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	semaphoreInfo.deviceIndex = 0;

	VkSemaphoreSubmitInfoDarray* semaphoreInfosDarray = VkSemaphoreSubmitInfoDarrayCreate(signalSemaphoreCount + 1, GetGlobalAllocator());
	VkSemaphoreSubmitInfoDarrayPushback(semaphoreInfosDarray, &semaphoreInfo);

	for (u32 i = 0; i < signalSemaphoreCount; ++i)
		VkSemaphoreSubmitInfoDarrayPushback(semaphoreInfosDarray, &pSignalSemaphoreSubmitInfos[i]);

	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.pNext = nullptr;
	submitInfo.flags = 0;
	submitInfo.waitSemaphoreInfoCount = waitSemaphoreCount;
	submitInfo.pWaitSemaphoreInfos = pWaitSemaphoreSubmitInfos;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &commandBufferInfo;
	submitInfo.signalSemaphoreInfoCount = semaphoreInfosDarray->size;
	submitInfo.pSignalSemaphoreInfos = semaphoreInfosDarray->data;

	VK_CHECK(vkQueueSubmit2(commandBuffer.queueFamily->handle, 1, &submitInfo, VK_NULL_HANDLE));

	DarrayDestroy(semaphoreInfosDarray);

	CommandBuffer* destructionResource = Alloc(vk_state->poolAllocator32B, RENDER_POOL_BLOCK_SIZE_32);
	*destructionResource = commandBuffer;

	ResourceDestructionInfo commandBufferDestructionInfo = {};
	commandBufferDestructionInfo.resource = destructionResource;
	commandBufferDestructionInfo.Destructor = SingleUseCommandBufferDestructor;
	commandBufferDestructionInfo.signalValue = commandBuffer.queueFamily->semaphore.submitValue;

	if (ref_signaledValue)
		*ref_signaledValue = commandBuffer.queueFamily->semaphore.submitValue;

	ResourceDestructionInfoDarrayPushback(commandBuffer.queueFamily->resourcesPendingDestructionDarray, &commandBufferDestructionInfo);

	return true;
}

void ResetCommandBuffer(CommandBuffer commandBuffer)
{
	// No reset flags, because resources attached to the command buffer don't necessarily need to be freed and this allows Vulkan to decide whats best
	vkResetCommandBuffer(commandBuffer.handle, 0);
}

bool ResetAndBeginCommandBuffer(CommandBuffer commandBuffer)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = 0;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; /// TODO: test if the validation layer likes this or not
	beginInfo.pInheritanceInfo = 0;

	if (VK_SUCCESS != vkBeginCommandBuffer(commandBuffer.handle, &beginInfo))
	{
		_FATAL("Failed to start recording command buffer");
		return false;
	}

	return true;
}

void EndCommandBuffer(CommandBuffer commandBuffer)
{
	if (VK_SUCCESS != vkEndCommandBuffer(commandBuffer.handle))
	{
		_FATAL("Failed to stop recording command buffer");
		return;
	}
}

bool SubmitCommandBuffers(u32 waitSemaphoreCount, VkSemaphoreSubmitInfo* pWaitSemaphoreInfos, u32 signalSemaphoreCount, VkSemaphoreSubmitInfo* pSignalSemaphoreInfos, u32 commandBufferCount, CommandBuffer* commandBuffers, VkFence fence)
{
#ifdef DEBUG
	if (commandBufferCount > MAX_SUBMITTED_COMMAND_BUFFERS)
		GRASSERT_MSG(false, "Can't submit that many command buffers at once, increase the max submitted value or reduce the amount of command buffers submitted at once");
	u32 queueFamilyIndex = commandBuffers[0].queueFamily->index;
	// Checking if all the command buffers are from the same queue family
	for (u32 i = 0; i < commandBufferCount; ++i)
	{
		if (queueFamilyIndex != commandBuffers[i].queueFamily->index)
			GRASSERT_MSG(false, "Command buffers in submit command buffers from different queue families");
	}
#endif // DEBUG

	VkCommandBufferSubmitInfoDarray* commandBufferSubmitInfosDarray = VkCommandBufferSubmitInfoDarrayCreate(commandBufferCount, GetGlobalAllocator());
	for (u32 i = 0; i < commandBufferCount; ++i)
	{
		VkCommandBufferSubmitInfo commandBufferInfo = {};
		commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
		commandBufferInfo.pNext = 0;
		commandBufferInfo.commandBuffer = commandBuffers[i].handle;
		commandBufferInfo.deviceMask = 0;
		VkCommandBufferSubmitInfoDarrayPushback(commandBufferSubmitInfosDarray, &commandBufferInfo);
	}

	commandBuffers[0].queueFamily->semaphore.submitValue++;
	VkSemaphoreSubmitInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	semaphoreInfo.pNext = 0;
	semaphoreInfo.semaphore = commandBuffers[0].queueFamily->semaphore.handle;
	semaphoreInfo.value = commandBuffers[0].queueFamily->semaphore.submitValue;
	semaphoreInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	semaphoreInfo.deviceIndex = 0;

	VkSemaphoreSubmitInfoDarray* semaphoreInfosDarray = VkSemaphoreSubmitInfoDarrayCreate(signalSemaphoreCount + 1, GetGlobalAllocator());
	VkSemaphoreSubmitInfoDarrayPushback(semaphoreInfosDarray, &semaphoreInfo);

	for (u32 i = 0; i < signalSemaphoreCount; ++i)
		VkSemaphoreSubmitInfoDarrayPushback(semaphoreInfosDarray, &pSignalSemaphoreInfos[i]);

	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.pNext = 0;
	submitInfo.flags = 0;
	submitInfo.waitSemaphoreInfoCount = waitSemaphoreCount;
	submitInfo.pWaitSemaphoreInfos = pWaitSemaphoreInfos;
	submitInfo.commandBufferInfoCount = commandBufferCount;
	submitInfo.pCommandBufferInfos = commandBufferSubmitInfosDarray->data;
	submitInfo.signalSemaphoreInfoCount = semaphoreInfosDarray->size;
	submitInfo.pSignalSemaphoreInfos = semaphoreInfosDarray->data;

	if (VK_SUCCESS != vkQueueSubmit2(commandBuffers[0].queueFamily->handle, 1, &submitInfo, fence))
	{
		DarrayDestroy(semaphoreInfosDarray);
		DarrayDestroy(commandBufferSubmitInfosDarray);
		_ERROR("Failed to submit queue");
		return false;
	}

	DarrayDestroy(semaphoreInfosDarray);
	DarrayDestroy(commandBufferSubmitInfosDarray);

	return true;
}
