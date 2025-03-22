#pragma once
#include "defines.h"
#include "vulkan_types.h"
#include "containers/darray.h"




void AllocateCommandBuffer(QueueFamily* queueFamily, CommandBuffer* ref_pCommandBuffer);

/// <summary>
/// Resets the command buffer, only supported if the command buffer was allocated from a command pool with the VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT flag.
/// </summary>
/// <param name="commandBuffer"></param>
void ResetCommandBuffer(CommandBuffer commandBuffer);
void ResetAndBeginCommandBuffer(CommandBuffer commandBuffer);
void EndCommandBuffer(CommandBuffer commandBuffer);
void SubmitCommandBuffers(u32 waitSemaphoreCount, VkSemaphoreSubmitInfo* pWaitSemaphoreInfos, u32 signalSemaphoreCount, VkSemaphoreSubmitInfo* pSignalSemaphoreInfos, u32 commandBufferCount, CommandBuffer* commandBuffers, VkFence fence);
