#include "../shader.h"
#include "vulkan_shader.h"

#include "core/asserts.h"
#include "vulkan_buffer.h"
#include "vulkan_shader_loader.h"
#include "vulkan_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILEPATH_SIZE 100

void ShaderCreate(const char* shaderName, ShaderCreateInfo* pCreateInfo)
{
    Shader clientShader;
    clientShader.internalState = Alloc(vk_state->rendererAllocator, sizeof(VulkanShader));
    VulkanShader* shader = (VulkanShader*)clientShader.internalState;
    MemoryZero(shader, sizeof(*shader));

    bool fragmentShader = pCreateInfo->fragmentShaderName;
    if (!fragmentShader)
        pCreateInfo->fragmentShaderName = "no fragment shader";

    // Preparing filename strings
    char* compiledVertFilename = Alloc(vk_state->rendererAllocator, 4 * MAX_FILEPATH_SIZE);
    char* compiledFragFilename = compiledVertFilename + MAX_FILEPATH_SIZE;
    char* rawVertFilename = compiledFragFilename + MAX_FILEPATH_SIZE;
    char* rawFragFilename = rawVertFilename + MAX_FILEPATH_SIZE;

    u32 vertShaderNameLength = strlen(pCreateInfo->vertexShaderName);
    u32 fragShaderNameLength = strlen(pCreateInfo->fragmentShaderName);

#define SHADERS_PREFIX_LENGTH 8
    const char* shaderFolderPrefix = "shaders/";

    // Adding shader folder prefix
    MemoryCopy(compiledVertFilename, shaderFolderPrefix, SHADERS_PREFIX_LENGTH);
    MemoryCopy(compiledFragFilename, shaderFolderPrefix, SHADERS_PREFIX_LENGTH);
    MemoryCopy(rawVertFilename, shaderFolderPrefix, SHADERS_PREFIX_LENGTH);
    MemoryCopy(rawFragFilename, shaderFolderPrefix, SHADERS_PREFIX_LENGTH);

    // Adding filename
    MemoryCopy(compiledVertFilename + SHADERS_PREFIX_LENGTH, pCreateInfo->vertexShaderName, vertShaderNameLength);
    MemoryCopy(compiledFragFilename + SHADERS_PREFIX_LENGTH, pCreateInfo->fragmentShaderName, fragShaderNameLength);
    MemoryCopy(rawVertFilename + SHADERS_PREFIX_LENGTH, pCreateInfo->vertexShaderName, vertShaderNameLength);
    MemoryCopy(rawFragFilename + SHADERS_PREFIX_LENGTH, pCreateInfo->fragmentShaderName, fragShaderNameLength);

    // Adding different postfixes
    const char* vertPostfix = ".vert.spv";
    const char* fragPostfix = ".frag.spv";
    const char* rawVertPostfix = ".vert";
    const char* rawFragPostfix = ".frag";
    MemoryCopy(compiledVertFilename + SHADERS_PREFIX_LENGTH + vertShaderNameLength, vertPostfix, 10);
    MemoryCopy(compiledFragFilename + SHADERS_PREFIX_LENGTH + fragShaderNameLength, fragPostfix, 10);
    MemoryCopy(rawVertFilename + SHADERS_PREFIX_LENGTH + vertShaderNameLength, rawVertPostfix, 6);
    MemoryCopy(rawFragFilename + SHADERS_PREFIX_LENGTH + fragShaderNameLength, rawFragPostfix, 6);

    if (!fragmentShader)
        pCreateInfo->fragmentShaderName = nullptr;

    // ============================================================================================================================================================
    // ======================== Getting the properties/uniforms from the raw shader ==========================================================================
    // ============================================================================================================================================================
    GetUniformDataFromShader(rawVertFilename, &shader->vertUniformPropertiesData, &shader->vertUniformTexturesData);
    if (fragmentShader)
        GetUniformDataFromShader(rawFragFilename, &shader->fragUniformPropertiesData, &shader->fragUniformTexturesData);
    else
    {
        shader->fragUniformPropertiesData.propertyCount = 0;
        shader->fragUniformPropertiesData.uniformBufferSize = 0;
        shader->fragUniformTexturesData.textureCount = 0;
    }

    VkDeviceSize uniformBufferAlignmentRequirement = vk_state->deviceProperties.limits.minUniformBufferOffsetAlignment;

    // TODO: debug safety check if there are no properties in the vert and frag shader with the same name

    // Calculating the total required uniform buffer size (per frame not for all in flight frames that happens in vulkan_material.c)
    // If both the vertex and index buffer have a uniform buffer
    if (shader->vertUniformPropertiesData.propertyCount > 0 && shader->fragUniformPropertiesData.propertyCount > 0)
    {
        // Calculating the offset for the fragment uniform buffer
        shader->fragmentUniformBufferOffset = shader->vertUniformPropertiesData.uniformBufferSize;
        u32 alignmentPadding = (uniformBufferAlignmentRequirement - (shader->fragmentUniformBufferOffset % uniformBufferAlignmentRequirement)) % uniformBufferAlignmentRequirement;
        shader->fragmentUniformBufferOffset += alignmentPadding;

        // Adding the fragment uniform offset to all the fragment properties local offsets
        for (int i = 0; i < shader->fragUniformPropertiesData.propertyCount; i++)
        {
            shader->fragUniformPropertiesData.propertyOffsets[i] += shader->fragmentUniformBufferOffset;
        }

        shader->totalUniformDataSize = shader->fragmentUniformBufferOffset + shader->fragUniformPropertiesData.uniformBufferSize;
    }
    else // If only one or none of the vert and frag shader have a uniform buffer
    {
        // We know that at least one of vert's and frag's uniform buffer size is zero so we can add them without having to worry about offsets between them
        shader->totalUniformDataSize = shader->vertUniformPropertiesData.uniformBufferSize + shader->fragUniformPropertiesData.uniformBufferSize;
    }

    // Making sure that if multiple uniform buffers are stored in one buffer the offset between them is correct
    shader->totalUniformDataSize += (uniformBufferAlignmentRequirement - (shader->totalUniformDataSize % uniformBufferAlignmentRequirement)) % uniformBufferAlignmentRequirement;

    // ============================================================================================================================================================
    // ======================== Creating descriptor set layout ==========================================================================
    // ============================================================================================================================================================
    // TODO: check if bindings are consecutive and start with zero
    VkDescriptorSetLayoutBinding descriptorSetLayoutBindings[10] = {};

    u32 bindingCount = 0;
    if (shader->vertUniformPropertiesData.propertyCount > 0)
    {
        descriptorSetLayoutBindings[bindingCount].binding = shader->vertUniformPropertiesData.bindingIndex;
        descriptorSetLayoutBindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorSetLayoutBindings[bindingCount].descriptorCount = 1;
        descriptorSetLayoutBindings[bindingCount].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        descriptorSetLayoutBindings[bindingCount].pImmutableSamplers = nullptr;
        bindingCount++;
    }

    if (shader->fragUniformPropertiesData.propertyCount > 0)
    {
        descriptorSetLayoutBindings[bindingCount].binding = shader->fragUniformPropertiesData.bindingIndex;
        descriptorSetLayoutBindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorSetLayoutBindings[bindingCount].descriptorCount = 1;
        descriptorSetLayoutBindings[bindingCount].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        descriptorSetLayoutBindings[bindingCount].pImmutableSamplers = nullptr;
        bindingCount++;
    }

    for (int i = 0; i < shader->vertUniformTexturesData.textureCount; i++)
    {
        descriptorSetLayoutBindings[bindingCount].binding = shader->vertUniformTexturesData.bindingIndices[i];
        descriptorSetLayoutBindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorSetLayoutBindings[bindingCount].descriptorCount = 1;
        descriptorSetLayoutBindings[bindingCount].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        descriptorSetLayoutBindings[bindingCount].pImmutableSamplers = nullptr;
        bindingCount++;
    }

    for (int i = 0; i < shader->fragUniformTexturesData.textureCount; i++)
    {
        descriptorSetLayoutBindings[bindingCount].binding = shader->fragUniformTexturesData.bindingIndices[i];
        descriptorSetLayoutBindings[bindingCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorSetLayoutBindings[bindingCount].descriptorCount = 1;
        descriptorSetLayoutBindings[bindingCount].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        descriptorSetLayoutBindings[bindingCount].pImmutableSamplers = nullptr;
        bindingCount++;
    }

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
    descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCreateInfo.pNext = nullptr;
    descriptorSetLayoutCreateInfo.flags = 0;
    descriptorSetLayoutCreateInfo.bindingCount = bindingCount;
    descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings;

    if (VK_SUCCESS != vkCreateDescriptorSetLayout(vk_state->device, &descriptorSetLayoutCreateInfo, vk_state->vkAllocator, &shader->descriptorSetLayout))
    {
        GRASSERT_MSG(false, "Vulkan descriptor set layout creation failed");
    }

    // ============================================================================================================================================================
    // ======================== Creating pipeline layout ==========================================================================
    // ============================================================================================================================================================
    // Push constants
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstantObject);

    VkDescriptorSetLayout descriptorSetLayouts[2];
    descriptorSetLayouts[0] = vk_state->globalDescriptorSetLayout;
    descriptorSetLayouts[1] = shader->descriptorSetLayout;

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = 2;
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts;
    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    if (VK_SUCCESS != vkCreatePipelineLayout(vk_state->device, &pipelineLayoutCreateInfo, vk_state->vkAllocator, &shader->pipelineLayout))
    {
        GRASSERT_MSG(false, "Vulkan pipeline layout creation failed");
    }

    // ============================================================================================================================================================
    // ======================== Creating the graphics pipeline ==========================================================================
    // ============================================================================================================================================================

    // Loading shaders
    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    CreateShaderModule(compiledVertFilename, vk_state, &vertShaderModule);
    if (fragmentShader)
        CreateShaderModule(compiledFragFilename, vk_state, &fragShaderModule);

    // Shader stages
    VkPipelineShaderStageCreateInfo shaderStagesCreateInfo[2] = {};
    shaderStagesCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStagesCreateInfo[0].pNext = nullptr;
    shaderStagesCreateInfo[0].flags = 0;
    shaderStagesCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStagesCreateInfo[0].module = vertShaderModule;
    shaderStagesCreateInfo[0].pName = "main";
    shaderStagesCreateInfo[0].pSpecializationInfo = nullptr;

    if (fragmentShader)
    {
        shaderStagesCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStagesCreateInfo[1].pNext = nullptr;
        shaderStagesCreateInfo[1].flags = 0;
        shaderStagesCreateInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStagesCreateInfo[1].module = fragShaderModule;
        shaderStagesCreateInfo[1].pName = "main";
        shaderStagesCreateInfo[1].pSpecializationInfo = nullptr;
    }

    // ======================================================== Vertex input ===========================================================================
    // Creating lookup tables
    u32 attributeSizes[20] = {};
    attributeSizes[VERTEX_ATTRIBUTE_TYPE_FLOAT] = 4;
    attributeSizes[VERTEX_ATTRIBUTE_TYPE_VEC2] = 8;
    attributeSizes[VERTEX_ATTRIBUTE_TYPE_VEC3] = 12;
    attributeSizes[VERTEX_ATTRIBUTE_TYPE_VEC4] = 16;
    VkFormat attributeFormats[20] = {};
    attributeFormats[VERTEX_ATTRIBUTE_TYPE_FLOAT] = VK_FORMAT_R32_SFLOAT;
    attributeFormats[VERTEX_ATTRIBUTE_TYPE_VEC2] = VK_FORMAT_R32G32_SFLOAT;
    attributeFormats[VERTEX_ATTRIBUTE_TYPE_VEC3] = VK_FORMAT_R32G32B32_SFLOAT;
    attributeFormats[VERTEX_ATTRIBUTE_TYPE_VEC4] = VK_FORMAT_R32G32B32A32_SFLOAT;

    // ================ Preprocessing vertex attributes to turn matrices into 4 vec4's
    VertexBufferLayout vbLayoutCopy = pCreateInfo->vertexBufferLayout;
    {
        // Per vertex attributes
        u32 attribIndex = 0;
        for (int i = 0; i < pCreateInfo->vertexBufferLayout.perVertexAttributeCount; i++)
        {
            if (pCreateInfo->vertexBufferLayout.perVertexAttributes[i] == VERTEX_ATTRIBUTE_TYPE_MAT4)
            {
                vbLayoutCopy.perVertexAttributeCount += 3; // The matrix goes from being one attrib to being four so we add three
                vbLayoutCopy.perVertexAttributes[attribIndex] = VERTEX_ATTRIBUTE_TYPE_VEC4;
                vbLayoutCopy.perVertexAttributes[attribIndex+1] = VERTEX_ATTRIBUTE_TYPE_VEC4;
                vbLayoutCopy.perVertexAttributes[attribIndex+2] = VERTEX_ATTRIBUTE_TYPE_VEC4;
                vbLayoutCopy.perVertexAttributes[attribIndex+3] = VERTEX_ATTRIBUTE_TYPE_VEC4;
                attribIndex += 4;
            }
            else
            {
                vbLayoutCopy.perVertexAttributes[attribIndex] = pCreateInfo->vertexBufferLayout.perVertexAttributes[i];
                attribIndex++;
            }
        }
        // Per instance attributes
        attribIndex = 0;
        for (int i = 0; i < pCreateInfo->vertexBufferLayout.perInstanceAttributeCount; i++)
        {
            if (pCreateInfo->vertexBufferLayout.perInstanceAttributes[i] == VERTEX_ATTRIBUTE_TYPE_MAT4)
            {
                vbLayoutCopy.perInstanceAttributeCount += 3; // The matrix goes from being one attrib to being four so we add three
                vbLayoutCopy.perInstanceAttributes[attribIndex] = VERTEX_ATTRIBUTE_TYPE_VEC4;
                vbLayoutCopy.perInstanceAttributes[attribIndex+1] = VERTEX_ATTRIBUTE_TYPE_VEC4;
                vbLayoutCopy.perInstanceAttributes[attribIndex+2] = VERTEX_ATTRIBUTE_TYPE_VEC4;
                vbLayoutCopy.perInstanceAttributes[attribIndex+3] = VERTEX_ATTRIBUTE_TYPE_VEC4;
                attribIndex += 4;
            }
            else
            {
                vbLayoutCopy.perInstanceAttributes[attribIndex] = pCreateInfo->vertexBufferLayout.perInstanceAttributes[i];
                attribIndex++;
            }
        }
    }

    // Filling in vulkan stucts for vertex attributes
    u32 totalAttributeCount = vbLayoutCopy.perVertexAttributeCount + vbLayoutCopy.perInstanceAttributeCount;
    GRASSERT(totalAttributeCount < MAX_VERTEX_ATTRIBUTES);
    VkVertexInputAttributeDescription attributeDescriptions[MAX_VERTEX_ATTRIBUTES] = {};
    u32 vertexStride = 0;
    u32 instanceStride = 0;

    for (int i = 0; i < totalAttributeCount; i++)
    {
        attributeDescriptions[i].location = i;

        if (i < vbLayoutCopy.perVertexAttributeCount) // if it's a vertex attribute
        {
            attributeDescriptions[i].binding = 0;
            attributeDescriptions[i].format = attributeFormats[vbLayoutCopy.perVertexAttributes[i]];
            attributeDescriptions[i].offset = vertexStride;
            vertexStride += attributeSizes[vbLayoutCopy.perVertexAttributes[i]];
        }
        else // if it's an instance attribute
        {
            attributeDescriptions[i].binding = 1;
            attributeDescriptions[i].format = attributeFormats[vbLayoutCopy.perInstanceAttributes[i - vbLayoutCopy.perVertexAttributeCount]];
            attributeDescriptions[i].offset = instanceStride;
            instanceStride += attributeSizes[vbLayoutCopy.perInstanceAttributes[i - vbLayoutCopy.perVertexAttributeCount]];
        }
    }

    VkVertexInputBindingDescription vertexBindingDescriptions[2] = {};
    vertexBindingDescriptions[0].binding = 0;
    vertexBindingDescriptions[0].stride = vertexStride;
    vertexBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vertexBindingDescriptions[1].binding = 1;
    vertexBindingDescriptions[1].stride = instanceStride;
    vertexBindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    u32 vertexBindingDescriptionCount = 1;
    if (vbLayoutCopy.perInstanceAttributeCount > 0)
        vertexBindingDescriptionCount++;

    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {};
    vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputCreateInfo.pNext = nullptr;
    vertexInputCreateInfo.flags = 0;
    vertexInputCreateInfo.vertexBindingDescriptionCount = vertexBindingDescriptionCount;
    vertexInputCreateInfo.pVertexBindingDescriptions = vertexBindingDescriptions;
    vertexInputCreateInfo.vertexAttributeDescriptionCount = totalAttributeCount;
    vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescriptions;

    // Input assembler
    VkPipelineInputAssemblyStateCreateInfo inputAssemblerCreateInfo = {};
    inputAssemblerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblerCreateInfo.pNext = nullptr;
    inputAssemblerCreateInfo.flags = 0;
    inputAssemblerCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblerCreateInfo.primitiveRestartEnable = VK_FALSE;

    // Viewport state
    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
    viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportStateCreateInfo.pNext = nullptr;
    viewportStateCreateInfo.flags = 0;
    viewportStateCreateInfo.viewportCount = 1;
    viewportStateCreateInfo.pViewports = nullptr; // dynamic state
    viewportStateCreateInfo.scissorCount = 1;
    viewportStateCreateInfo.pScissors = nullptr; // dynamic state

    // Rasterization state
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
    rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizerCreateInfo.pNext = nullptr;
    rasterizerCreateInfo.flags = 0;
    rasterizerCreateInfo.depthClampEnable = VK_FALSE;
    rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
    rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	if (pCreateInfo->cullMode == CULL_FRONT)
		rasterizerCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
    rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizerCreateInfo.depthBiasEnable = VK_FALSE;
    rasterizerCreateInfo.depthBiasConstantFactor = 0.0f;
    rasterizerCreateInfo.depthBiasClamp = 0.0f;
    rasterizerCreateInfo.depthBiasSlopeFactor = 0.0f;
    rasterizerCreateInfo.lineWidth = 1.0f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {};
    multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisamplingCreateInfo.pNext = nullptr;
    multisamplingCreateInfo.flags = 0;
    multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisamplingCreateInfo.sampleShadingEnable = VK_FALSE;
    multisamplingCreateInfo.minSampleShading = 1.0f;
    multisamplingCreateInfo.pSampleMask = nullptr;
    multisamplingCreateInfo.alphaToCoverageEnable = VK_FALSE;
    multisamplingCreateInfo.alphaToOneEnable = VK_FALSE;

    // Depth/stencil
    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
    depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilCreateInfo.pNext = nullptr;
    depthStencilCreateInfo.flags = 0;
    if (pCreateInfo->renderTargetDepth)
    {
        depthStencilCreateInfo.depthTestEnable = VK_TRUE;
        depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
    }
    else
    {
        depthStencilCreateInfo.depthTestEnable = VK_FALSE;
        depthStencilCreateInfo.depthWriteEnable = VK_FALSE;
    }
    depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
    depthStencilCreateInfo.minDepthBounds = 0;
    depthStencilCreateInfo.maxDepthBounds = 0;
    if (pCreateInfo->renderTargetStencil)
        depthStencilCreateInfo.stencilTestEnable = VK_TRUE;
    else
        depthStencilCreateInfo.stencilTestEnable = VK_FALSE;

    // Blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (pCreateInfo->renderTargetColor)
        colorBlendAttachment.blendEnable = VK_TRUE;
    else
        colorBlendAttachment.blendEnable = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo blendStateCreateInfo = {};
    blendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendStateCreateInfo.pNext = nullptr;
    blendStateCreateInfo.flags = 0;
    blendStateCreateInfo.logicOpEnable = VK_FALSE;
    blendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
    blendStateCreateInfo.attachmentCount = 1;
    blendStateCreateInfo.pAttachments = &colorBlendAttachment;

    // Dynamic states
    VkDynamicState dynamicStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
    dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicStateCreateInfo.pNext = nullptr;
    dynamicStateCreateInfo.flags = 0;
    dynamicStateCreateInfo.dynamicStateCount = 2;
    dynamicStateCreateInfo.pDynamicStates = dynamicStates;

    // Render target
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCreateInfo.pNext = nullptr;
    pipelineRenderingCreateInfo.viewMask = 0;
    if (pCreateInfo->renderTargetColor)
        pipelineRenderingCreateInfo.colorAttachmentCount = 1;
    else
        pipelineRenderingCreateInfo.colorAttachmentCount = 0;
    pipelineRenderingCreateInfo.pColorAttachmentFormats = &vk_state->renderTargetColorFormat;
    pipelineRenderingCreateInfo.depthAttachmentFormat = vk_state->renderTargetDepthFormat;
    pipelineRenderingCreateInfo.stencilAttachmentFormat = vk_state->renderTargetDepthFormat;

    // Pipeline create info
    VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
    graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
    graphicsPipelineCreateInfo.flags = 0;
    if (fragmentShader)
        graphicsPipelineCreateInfo.stageCount = 2;
    else
        graphicsPipelineCreateInfo.stageCount = 1;
    graphicsPipelineCreateInfo.pStages = shaderStagesCreateInfo;
    graphicsPipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
    graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblerCreateInfo;
    graphicsPipelineCreateInfo.pTessellationState = nullptr;
    graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
    graphicsPipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
    graphicsPipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
    graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;
    graphicsPipelineCreateInfo.pColorBlendState = &blendStateCreateInfo;
    graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
    graphicsPipelineCreateInfo.layout = shader->pipelineLayout;
    graphicsPipelineCreateInfo.renderPass = nullptr;
    graphicsPipelineCreateInfo.subpass = 0;
    graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
    graphicsPipelineCreateInfo.basePipelineIndex = -1;

    if (VK_SUCCESS != vkCreateGraphicsPipelines(vk_state->device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, vk_state->vkAllocator, &shader->pipelineObject))
    {
        GRASSERT_MSG(false, "Vulkan graphics pipeline creation failed");
    }

    vkDestroyShaderModule(vk_state->device, vertShaderModule, vk_state->vkAllocator);
    if (fragmentShader)
        vkDestroyShaderModule(vk_state->device, fragShaderModule, vk_state->vkAllocator);

    Free(vk_state->rendererAllocator, compiledVertFilename);

    _DEBUG("Shader created successfully");

    SimpleMapInsert(vk_state->shaderMap, shaderName, clientShader.internalState);
}

void ShaderDestroy(const char* shaderName)
{
    VulkanShader* shader = SimpleMapDelete(vk_state->shaderMap, shaderName);

    FreeUniformData(&shader->vertUniformPropertiesData, &shader->vertUniformTexturesData);
    FreeUniformData(&shader->fragUniformPropertiesData, &shader->fragUniformTexturesData);

    if (shader->pipelineObject)
        vkDestroyPipeline(vk_state->device, shader->pipelineObject, vk_state->vkAllocator);
    if (shader->pipelineLayout)
        vkDestroyPipelineLayout(vk_state->device, shader->pipelineLayout, vk_state->vkAllocator);
    if (shader->descriptorSetLayout)
        vkDestroyDescriptorSetLayout(vk_state->device, shader->descriptorSetLayout, vk_state->vkAllocator);

    Free(vk_state->rendererAllocator, shader);
}

Shader ShaderGetRef(const char* shaderName)
{
    VulkanShader* shader = SimpleMapLookup(vk_state->shaderMap, shaderName);
    Shader clientShader = {shader};
    return clientShader;
}

void ShaderDestroyInternal(VulkanShader* shader)
{
    FreeUniformData(&shader->vertUniformPropertiesData, &shader->vertUniformTexturesData);
    FreeUniformData(&shader->fragUniformPropertiesData, &shader->fragUniformTexturesData);

    if (shader->pipelineObject)
        vkDestroyPipeline(vk_state->device, shader->pipelineObject, vk_state->vkAllocator);
    if (shader->pipelineLayout)
        vkDestroyPipelineLayout(vk_state->device, shader->pipelineLayout, vk_state->vkAllocator);
    if (shader->descriptorSetLayout)
        vkDestroyDescriptorSetLayout(vk_state->device, shader->descriptorSetLayout, vk_state->vkAllocator);

    Free(vk_state->rendererAllocator, shader);
}

