#pragma once
#include "defines.h"
#include "vulkan_types.h"
#include "containers/darray.h"




bool AllocateCommandBuffer(QueueFamily* queueFamily, CommandBuffer* out_pCommandBuffer);
void FreeCommandBuffer(CommandBuffer commandBuffer);

bool AllocateAndBeginSingleUseCommandBuffer(QueueFamily* queueFamily, CommandBuffer* out_pCommandBuffer);
bool EndSubmitAndFreeSingleUseCommandBuffer(CommandBuffer commandBuffer, u32 waitSemaphoreCount, VkSemaphoreSubmitInfo* pWaitSemaphoreSubmitInfos, u32 signalSemaphoreCount, VkSemaphoreSubmitInfo* pSignalSemaphoreSubmitInfos, u64* out_signaledValue);

/// <summary>
/// Resets the command buffer, only supported if the command buffer was allocated from a command pool with the VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag.
/// </summary>
/// <param name="commandBuffer"></param>
void ResetCommandBuffer(CommandBuffer commandBuffer);
bool ResetAndBeginCommandBuffer(CommandBuffer commandBuffer);
void EndCommandBuffer(CommandBuffer commandBuffer);
bool SubmitCommandBuffers(u32 waitSemaphoreCount, VkSemaphoreSubmitInfo* pWaitSemaphoreInfos, u32 signalSemaphoreCount, VkSemaphoreSubmitInfo* pSignalSemaphoreInfos, u32 commandBufferCount, CommandBuffer* commandBuffers, VkFence fence);
