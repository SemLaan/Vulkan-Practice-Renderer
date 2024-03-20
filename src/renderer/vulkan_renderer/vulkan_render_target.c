#include "../render_target.h"
#include "core/asserts.h"
#include "vulkan_command_buffer.h"
#include "vulkan_image.h"
#include "vulkan_types.h"

RenderTarget RenderTargetCreate(u32 width, u32 height, RenderTargetUsage colorBufferUsage, RenderTargetUsage depthBufferUsage)
{
    // Allocating RenderTarget struct
    RenderTarget clientRenderTarget;
    clientRenderTarget.internalState = Alloc(vk_state->rendererAllocator, sizeof(VulkanRenderTarget), MEM_TAG_RENDERER_SUBSYS);
    VulkanRenderTarget* renderTarget = clientRenderTarget.internalState;
    renderTarget->colorBufferUsage = colorBufferUsage;
    renderTarget->depthBufferUsage = depthBufferUsage;
    renderTarget->extent.width = width;
    renderTarget->extent.height = height;

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
        createImageParameters.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        CreateImage(&createImageParameters, &renderTarget->colorImage);
        CreateImageView(&renderTarget->colorImage, VK_IMAGE_ASPECT_COLOR_BIT);

        // Creating sampler if this render target's color buffer will be used as texture
        if (colorBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
        {
            VkSamplerCreateInfo samplerCreateInfo = {};
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.pNext = nullptr;
            samplerCreateInfo.flags = 0;
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.anisotropyEnable = VK_FALSE;
            samplerCreateInfo.maxAnisotropy = 1.0f;
            samplerCreateInfo.compareEnable = VK_FALSE;
            samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerCreateInfo.mipLodBias = 0.0f;
            samplerCreateInfo.minLod = 0.0f;
            samplerCreateInfo.maxLod = 0.0f;
            samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

            if (VK_SUCCESS != vkCreateSampler(vk_state->device, &samplerCreateInfo, vk_state->vkAllocator, &renderTarget->colorImage.sampler))
            {
                GRASSERT_MSG(false, "failed to create image sampler");
            }
        }

        CommandBuffer oneTimeCommandBuffer = {};
        AllocateAndBeginSingleUseCommandBuffer(&vk_state->graphicsQueue, &oneTimeCommandBuffer);

        VkImageMemoryBarrier2 colorTransitionImageBarrierInfo = {};
        colorTransitionImageBarrierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        colorTransitionImageBarrierInfo.pNext = nullptr;
        colorTransitionImageBarrierInfo.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        colorTransitionImageBarrierInfo.srcAccessMask = VK_ACCESS_2_NONE;
        colorTransitionImageBarrierInfo.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        colorTransitionImageBarrierInfo.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        colorTransitionImageBarrierInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorTransitionImageBarrierInfo.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorTransitionImageBarrierInfo.srcQueueFamilyIndex = 0;
        colorTransitionImageBarrierInfo.dstQueueFamilyIndex = 0;
        colorTransitionImageBarrierInfo.image = renderTarget->colorImage.handle;
        colorTransitionImageBarrierInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        colorTransitionImageBarrierInfo.subresourceRange.baseMipLevel = 0;
        colorTransitionImageBarrierInfo.subresourceRange.levelCount = 1;
        colorTransitionImageBarrierInfo.subresourceRange.baseArrayLayer = 0;
        colorTransitionImageBarrierInfo.subresourceRange.layerCount = 1;

        VkDependencyInfo rendertargetTransitionDependencyInfo = {};
        rendertargetTransitionDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        rendertargetTransitionDependencyInfo.pNext = nullptr;
        rendertargetTransitionDependencyInfo.dependencyFlags = 0;
        rendertargetTransitionDependencyInfo.memoryBarrierCount = 0;
        rendertargetTransitionDependencyInfo.bufferMemoryBarrierCount = 0;
        rendertargetTransitionDependencyInfo.imageMemoryBarrierCount = 1;
        rendertargetTransitionDependencyInfo.pImageMemoryBarriers = &colorTransitionImageBarrierInfo;

        vkCmdPipelineBarrier2(oneTimeCommandBuffer.handle, &rendertargetTransitionDependencyInfo);

        EndSubmitAndFreeSingleUseCommandBuffer(oneTimeCommandBuffer, 0, nullptr, 0, nullptr, nullptr);
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
        createImageParameters.properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        CreateImage(&createImageParameters, &renderTarget->depthImage);
        CreateImageView(&renderTarget->depthImage, VK_IMAGE_ASPECT_DEPTH_BIT);

        // Creating sampler if this render target's depth buffer will be used as texture
        if (depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
        {
            VkSamplerCreateInfo samplerCreateInfo = {};
            samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerCreateInfo.pNext = nullptr;
            samplerCreateInfo.flags = 0;
            samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
            samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerCreateInfo.anisotropyEnable = VK_FALSE;
            samplerCreateInfo.maxAnisotropy = 1.0f;
            samplerCreateInfo.compareEnable = VK_FALSE;
            samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerCreateInfo.mipLodBias = 0.0f;
            samplerCreateInfo.minLod = 0.0f;
            samplerCreateInfo.maxLod = 0.0f;
            samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

            if (VK_SUCCESS != vkCreateSampler(vk_state->device, &samplerCreateInfo, vk_state->vkAllocator, &renderTarget->depthImage.sampler))
            {
                GRASSERT_MSG(false, "failed to create image sampler");
            }
        }

        CommandBuffer oneTimeCommandBuffer = {};
        AllocateAndBeginSingleUseCommandBuffer(&vk_state->graphicsQueue, &oneTimeCommandBuffer);

        VkImageMemoryBarrier2 depthStencilTransitionImageBarrierInfo = {};
        depthStencilTransitionImageBarrierInfo.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        depthStencilTransitionImageBarrierInfo.pNext = nullptr;
        depthStencilTransitionImageBarrierInfo.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
        depthStencilTransitionImageBarrierInfo.srcAccessMask = VK_ACCESS_2_NONE;
        depthStencilTransitionImageBarrierInfo.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        depthStencilTransitionImageBarrierInfo.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthStencilTransitionImageBarrierInfo.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthStencilTransitionImageBarrierInfo.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthStencilTransitionImageBarrierInfo.srcQueueFamilyIndex = 0;
        depthStencilTransitionImageBarrierInfo.dstQueueFamilyIndex = 0;
        depthStencilTransitionImageBarrierInfo.image = renderTarget->depthImage.handle;
        depthStencilTransitionImageBarrierInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        depthStencilTransitionImageBarrierInfo.subresourceRange.baseMipLevel = 0;
        depthStencilTransitionImageBarrierInfo.subresourceRange.levelCount = 1;
        depthStencilTransitionImageBarrierInfo.subresourceRange.baseArrayLayer = 0;
        depthStencilTransitionImageBarrierInfo.subresourceRange.layerCount = 1;

        VkDependencyInfo rendertargetTransitionDependencyInfo = {};
        rendertargetTransitionDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        rendertargetTransitionDependencyInfo.pNext = nullptr;
        rendertargetTransitionDependencyInfo.dependencyFlags = 0;
        rendertargetTransitionDependencyInfo.memoryBarrierCount = 0;
        rendertargetTransitionDependencyInfo.bufferMemoryBarrierCount = 0;
        rendertargetTransitionDependencyInfo.imageMemoryBarrierCount = 1;
        rendertargetTransitionDependencyInfo.pImageMemoryBarriers = &depthStencilTransitionImageBarrierInfo;

        vkCmdPipelineBarrier2(oneTimeCommandBuffer.handle, &rendertargetTransitionDependencyInfo);

        EndSubmitAndFreeSingleUseCommandBuffer(oneTimeCommandBuffer, 0, nullptr, 0, nullptr, nullptr);
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
            vkDestroyImage(vk_state->device, renderTarget->depthImage.handle, vk_state->vkAllocator);
        if (renderTarget->depthImage.memory)
            vkFreeMemory(vk_state->device, renderTarget->depthImage.memory, vk_state->vkAllocator);
        if (renderTarget->depthBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
            vkDestroySampler(vk_state->device, renderTarget->depthImage.sampler, vk_state->vkAllocator);
    }

    if (renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_DISPLAY || renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
    {
        if (renderTarget->colorImage.view)
            vkDestroyImageView(vk_state->device, renderTarget->colorImage.view, vk_state->vkAllocator);
        if (renderTarget->colorImage.handle)
            vkDestroyImage(vk_state->device, renderTarget->colorImage.handle, vk_state->vkAllocator);
        if (renderTarget->colorImage.memory)
            vkFreeMemory(vk_state->device, renderTarget->colorImage.memory, vk_state->vkAllocator);
        if (renderTarget->colorBufferUsage == RENDER_TARGET_USAGE_TEXTURE)
            vkDestroySampler(vk_state->device, renderTarget->colorImage.sampler, vk_state->vkAllocator);
    }
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
    viewport.y = (f32)renderTarget->extent.height;
    viewport.width = (f32)renderTarget->extent.width;
    viewport.height = -(f32)renderTarget->extent.height;
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
