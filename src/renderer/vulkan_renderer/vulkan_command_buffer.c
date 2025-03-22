#include "vulkan_command_buffer.h"

#include "core/logger.h"
#include "core/asserts.h"

#define MAX_SUBMITTED_COMMAND_BUFFERS 20

DEFINE_DARRAY_TYPE(VkSemaphoreSubmitInfo);
DEFINE_DARRAY_TYPE(VkCommandBufferSubmitInfo);

void AllocateCommandBuffer(QueueFamily* queueFamily, CommandBuffer* ref_pCommandBuffer)
{
	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.pNext = nullptr;
	allocateInfo.commandPool = queueFamily->commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	VK_CHECK(vkAllocateCommandBuffers(vk_state->device, &allocateInfo, &ref_pCommandBuffer->handle));

	ref_pCommandBuffer->queueFamily = queueFamily;
}

void ResetCommandBuffer(CommandBuffer commandBuffer)
{
	// No reset flags, because resources attached to the command buffer don't necessarily need to be freed and this allows Vulkan to decide whats best
	VK_CHECK(vkResetCommandBuffer(commandBuffer.handle, 0));
}

void ResetAndBeginCommandBuffer(CommandBuffer commandBuffer)
{
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.pNext = 0;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; /// TODO: test if the validation layer likes this or not
	beginInfo.pInheritanceInfo = 0;

	VK_CHECK(vkBeginCommandBuffer(commandBuffer.handle, &beginInfo));
}

void EndCommandBuffer(CommandBuffer commandBuffer)
{
	VK_CHECK(vkEndCommandBuffer(commandBuffer.handle));
}

void SubmitCommandBuffers(u32 waitSemaphoreCount, VkSemaphoreSubmitInfo* pWaitSemaphoreInfos, u32 signalSemaphoreCount, VkSemaphoreSubmitInfo* pSignalSemaphoreInfos, u32 commandBufferCount, CommandBuffer* commandBuffers, VkFence fence)
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

	VkSubmitInfo2 submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.pNext = 0;
	submitInfo.flags = 0;
	submitInfo.waitSemaphoreInfoCount = waitSemaphoreCount;
	submitInfo.pWaitSemaphoreInfos = pWaitSemaphoreInfos;
	submitInfo.commandBufferInfoCount = commandBufferCount;
	submitInfo.pCommandBufferInfos = commandBufferSubmitInfosDarray->data;
	submitInfo.signalSemaphoreInfoCount = signalSemaphoreCount;
	submitInfo.pSignalSemaphoreInfos = pSignalSemaphoreInfos;

	VK_CHECK(vkQueueSubmit2(commandBuffers[0].queueFamily->handle, 1, &submitInfo, fence));

	DarrayDestroy(commandBufferSubmitInfosDarray);
}
