#include "../render_target.h"
#include "core/asserts.h"
#include "vulkan_command_buffer.h"
#include "vulkan_image.h"
#include "vulkan_types.h"
#include "vulkan_memory.h"

RenderTarget RenderTargetCreate(u32 width, u32 height, RenderTargetUsage colorBufferUsage, RenderTargetUsage depthBufferUsage)
{
    // Allocating RenderTarget struct
    RenderTarget clientRenderTarget;
    clientRenderTarget.internalState = Alloc(vk_state->rendererAllocator, sizeof(VulkanRenderTarget));
    VulkanRenderTarget* renderTarget = clientRenderTarget.internalState;
    renderTarget->colorBufferUsage = colorBufferUsage;
    renderTarget->depthBufferUsage = depthBufferUsage;
    renderTarget->extent.width = width;
    renderTarget->extent.height = height;
	renderTarget->colorImage.mipLevels = 1;
	renderTarget->depthImage.mipLevels = 1;

    // Creating color buffer
    if (colorBufferUsage == RENDER_TARGET_USAGE_DISPLAY || colorBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
    {
        VkImageUsageFlags vulkanColorImageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        vulkanColorImageUsage |= (colorBufferUsage == RENDER_TARGET_USAGE_TEXTURE) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;
        vulkanColorImageUsage |= (colorBufferUsage == RENDER_TARGET_USAGE_DISPLAY) ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0;

        VulkanCreateImageParameters createImageParameters = {};
        createImageParameters.width = width;
        createImageParameters.height = height;
        createImageParameters.format = vk_state->renderTargetColorFormat;
        createImageParameters.tiling = VK_IMAGE_TILING_OPTIMAL;
        createImageParameters.usage = vulkanColorImageUsage;
		createImageParameters.mipLevels = renderTarget->colorImage.mipLevels;

        ImageCreate(&createImageParameters, MemType(MEMORY_TYPE_STATIC), &renderTarget->colorImage.handle, &renderTarget->colorImage.memory);
        CreateImageView(&renderTarget->colorImage, VK_IMAGE_ASPECT_COLOR_BIT, vk_state->renderTargetColorFormat);
    }

    // Creating depth buffer
    if (depthBufferUsage == RENDER_TARGET_USAGE_DEPTH || depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
    {
        VkImageUsageFlags vulkanDepthImageUsage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        vulkanDepthImageUsage |= (depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE) ? VK_IMAGE_USAGE_SAMPLED_BIT : 0;

        VulkanCreateImageParameters createImageParameters = {};
        createImageParameters.width = width;
        createImageParameters.height = height;
        createImageParameters.format = vk_state->renderTargetDepthFormat;
        createImageParameters.tiling = VK_IMAGE_TILING_OPTIMAL;
        createImageParameters.usage = vulkanDepthImageUsage;
		createImageParameters.mipLevels = renderTarget->depthImage.mipLevels;

        ImageCreate(&createImageParameters, MemType(MEMORY_TYPE_STATIC), &renderTarget->depthImage.handle, &renderTarget->depthImage.memory);
        CreateImageView(&renderTarget->depthImage, VK_IMAGE_ASPECT_DEPTH_BIT, vk_state->renderTargetDepthFormat);
    }

    return clientRenderTarget;
}

void RenderTargetDestroy(RenderTarget clientRenderTarget)
{
    VulkanRenderTarget* renderTarget = clientRenderTarget.internalState;

    if (renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_DEPTH || renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
    {
        if (renderTarget->depthImage.view)
            vkDestroyImageView(vk_state->device, renderTarget->depthImage.view, vk_state->vkAllocator);
        if (renderTarget->depthImage.handle)
			ImageDestroy(&renderTarget->depthImage.handle, &renderTarget->depthImage.memory);
    }

    if (renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_DISPLAY || renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
    {
        if (renderTarget->colorImage.view)
            vkDestroyImageView(vk_state->device, renderTarget->colorImage.view, vk_state->vkAllocator);
        if (renderTarget->colorImage.handle)
		ImageDestroy(&renderTarget->colorImage.handle, &renderTarget->colorImage.memory);
    }

	Free(vk_state->rendererAllocator, renderTarget);
}

void RenderTargetStartRendering(RenderTarget clientRenderTarget)
{
    VulkanRenderTarget* renderTarget = clientRenderTarget.internalState;
    VkCommandBuffer currentCommandBuffer = vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex].handle;

    // Transitioning images if necessary
    {
        VkImageMemoryBarrier2 rendertargetTransitionImageBarrierInfos[2] = {};
        u32 barrierCount = 0;

        // Transitioning color image if there is one
        if (renderTarget->colorBufferUsage != RENDER_TARGET_USAGE_NONE)
        {
            rendertargetTransitionImageBarrierInfos[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            rendertargetTransitionImageBarrierInfos[barrierCount].pNext = nullptr;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcAccessMask = VK_ACCESS_2_NONE;
            rendertargetTransitionImageBarrierInfos[barrierCount].dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            rendertargetTransitionImageBarrierInfos[barrierCount].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcQueueFamilyIndex = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].dstQueueFamilyIndex = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].image = renderTarget->colorImage.handle;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.baseMipLevel = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.levelCount = 1;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.baseArrayLayer = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.layerCount = 1;
            barrierCount++;
        }

        // Transitioning depth image if a transition is required (which is only if it is used as a texture aswell)
        if (renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
        {
            rendertargetTransitionImageBarrierInfos[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            rendertargetTransitionImageBarrierInfos[barrierCount].pNext = nullptr;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcStageMask = VK_PIPELINE_STAGE_2_NONE;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcAccessMask = VK_ACCESS_2_NONE;
            rendertargetTransitionImageBarrierInfos[barrierCount].dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            rendertargetTransitionImageBarrierInfos[barrierCount].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcQueueFamilyIndex = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].dstQueueFamilyIndex = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].image = renderTarget->depthImage.handle;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.baseMipLevel = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.levelCount = 1;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.baseArrayLayer = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.layerCount = 1;
            barrierCount++;
        }

        VkDependencyInfo rendertargetTransitionDependencyInfo = {};
        rendertargetTransitionDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        rendertargetTransitionDependencyInfo.pNext = nullptr;
        rendertargetTransitionDependencyInfo.dependencyFlags = 0;
        rendertargetTransitionDependencyInfo.memoryBarrierCount = 0;
        rendertargetTransitionDependencyInfo.bufferMemoryBarrierCount = 0;
        rendertargetTransitionDependencyInfo.imageMemoryBarrierCount = barrierCount;
        rendertargetTransitionDependencyInfo.pImageMemoryBarriers = rendertargetTransitionImageBarrierInfos;

        if (barrierCount > 0)
            vkCmdPipelineBarrier2(currentCommandBuffer, &rendertargetTransitionDependencyInfo);
    }

    // ==================================== Begin renderpass ==============================================
    VkRenderingAttachmentInfo renderingAttachmentInfos[2] = {};

    if (renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_DISPLAY || renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
    {
        renderingAttachmentInfos[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        renderingAttachmentInfos[0].pNext = nullptr;
        renderingAttachmentInfos[0].imageView = renderTarget->colorImage.view;
        renderingAttachmentInfos[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        renderingAttachmentInfos[0].resolveMode = 0;
        renderingAttachmentInfos[0].resolveImageView = nullptr;
        renderingAttachmentInfos[0].resolveImageLayout = 0;
        renderingAttachmentInfos[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        renderingAttachmentInfos[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        renderingAttachmentInfos[0].clearValue.color.float32[0] = 0;
        renderingAttachmentInfos[0].clearValue.color.float32[1] = 0;
        renderingAttachmentInfos[0].clearValue.color.float32[2] = 0;
        renderingAttachmentInfos[0].clearValue.color.float32[3] = 1.0f;
    }

    if (renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE || renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_DEPTH)
    {
        renderingAttachmentInfos[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        renderingAttachmentInfos[1].pNext = nullptr;
        renderingAttachmentInfos[1].imageView = renderTarget->depthImage.view;
        renderingAttachmentInfos[1].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        renderingAttachmentInfos[1].resolveMode = 0;
        renderingAttachmentInfos[1].resolveImageView = nullptr;
        renderingAttachmentInfos[1].resolveImageLayout = 0;
        renderingAttachmentInfos[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        if (renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
            renderingAttachmentInfos[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        else
            renderingAttachmentInfos[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        renderingAttachmentInfos[1].clearValue.depthStencil.depth = 1.0f;
    }

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.pNext = nullptr;
    renderingInfo.flags = 0;
    renderingInfo.renderArea.offset.x = 0;
    renderingInfo.renderArea.offset.y = 0;
    renderingInfo.renderArea.extent = renderTarget->extent;
    renderingInfo.layerCount = 1;
    renderingInfo.viewMask = 0;
    if (renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_DISPLAY || renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
    {
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &renderingAttachmentInfos[0];
    }
    else
    {
        renderingInfo.colorAttachmentCount = 0;
        renderingInfo.pColorAttachments = nullptr;
    }
    if (renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE || renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_DEPTH)
        renderingInfo.pDepthAttachment = &renderingAttachmentInfos[1];
    else
        renderingInfo.pDepthAttachment = nullptr;
    renderingInfo.pStencilAttachment = nullptr;

    // Viewport and scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (f32)renderTarget->extent.width;
    viewport.height = (f32)renderTarget->extent.height;
    viewport.minDepth = 1.0f;
    viewport.maxDepth = 0.0f;
    vkCmdSetViewport(currentCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent = renderTarget->extent;
    vkCmdSetScissor(currentCommandBuffer, 0, 1, &scissor);

    vkCmdBeginRendering(currentCommandBuffer, &renderingInfo);
}

void RenderTargetStopRendering(RenderTarget clientRenderTarget)
{
    VulkanRenderTarget* renderTarget = clientRenderTarget.internalState;
    VkCommandBuffer currentCommandBuffer = vk_state->graphicsCommandBuffers[vk_state->currentInFlightFrameIndex].handle;

    vkCmdEndRendering(currentCommandBuffer);

    // Transitioning images if necessary
    {
        VkImageMemoryBarrier2 rendertargetTransitionImageBarrierInfos[2] = {};
        u32 barrierCount = 0;

        // Transitioning color image if there is one
        if (renderTarget->colorBufferUsage != RENDER_TARGET_USAGE_NONE)
        {
            rendertargetTransitionImageBarrierInfos[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            rendertargetTransitionImageBarrierInfos[barrierCount].pNext = nullptr;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            if (renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_DISPLAY)
            {
                rendertargetTransitionImageBarrierInfos[barrierCount].dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
                rendertargetTransitionImageBarrierInfos[barrierCount].dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                rendertargetTransitionImageBarrierInfos[barrierCount].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                rendertargetTransitionImageBarrierInfos[barrierCount].newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            }
            else if (renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
            {
                rendertargetTransitionImageBarrierInfos[barrierCount].dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                rendertargetTransitionImageBarrierInfos[barrierCount].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                rendertargetTransitionImageBarrierInfos[barrierCount].oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                rendertargetTransitionImageBarrierInfos[barrierCount].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            rendertargetTransitionImageBarrierInfos[barrierCount].srcQueueFamilyIndex = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].dstQueueFamilyIndex = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].image = renderTarget->colorImage.handle;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.baseMipLevel = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.levelCount = 1;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.baseArrayLayer = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.layerCount = 1;
            barrierCount++;
        }

        // Transitioning depth image if a transition is required (which is only if it is used as a texture aswell)
        if (renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
        {
            rendertargetTransitionImageBarrierInfos[barrierCount].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            rendertargetTransitionImageBarrierInfos[barrierCount].pNext = nullptr;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            rendertargetTransitionImageBarrierInfos[barrierCount].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            rendertargetTransitionImageBarrierInfos[barrierCount].srcQueueFamilyIndex = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].dstQueueFamilyIndex = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].image = renderTarget->depthImage.handle;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.baseMipLevel = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.levelCount = 1;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.baseArrayLayer = 0;
            rendertargetTransitionImageBarrierInfos[barrierCount].subresourceRange.layerCount = 1;
            barrierCount++;
        }

        VkDependencyInfo rendertargetTransitionDependencyInfo = {};
        rendertargetTransitionDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        rendertargetTransitionDependencyInfo.pNext = nullptr;
        rendertargetTransitionDependencyInfo.dependencyFlags = 0;
        rendertargetTransitionDependencyInfo.memoryBarrierCount = 0;
        rendertargetTransitionDependencyInfo.bufferMemoryBarrierCount = 0;
        rendertargetTransitionDependencyInfo.imageMemoryBarrierCount = barrierCount;
        rendertargetTransitionDependencyInfo.pImageMemoryBarriers = rendertargetTransitionImageBarrierInfos;

        if (barrierCount > 0)
            vkCmdPipelineBarrier2(currentCommandBuffer, &rendertargetTransitionDependencyInfo);
    }
}

Texture GetColorAsTexture(RenderTarget clientRenderTarget)
{
    VulkanRenderTarget* renderTarget = clientRenderTarget.internalState;

    GRASSERT(renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_TEXTURE);

    Texture texture = {};
    texture.internalState = &renderTarget->colorImage;
    return texture;
}

Texture GetDepthAsTexture(RenderTarget clientRenderTarget)
{
    VulkanRenderTarget* renderTarget = clientRenderTarget.internalState;

    GRASSERT(renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE);

    Texture texture = {};
    texture.internalState = &renderTarget->depthImage;
    return texture;
}
