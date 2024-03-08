#include "vulkan_shader_loader.h"

#include "core/logger.h"
// TODO: make file io system so stdio doesn't have to be used
#include <stdio.h>


bool ReadFile(const char* filename, MemTag tag, char** out_data, u64* out_fileSize)
{
	FILE* file = fopen(filename, "rb");

	if (file == NULL)
	{
		_ERROR("Failed to open file");
		return false;
	}

	fseek(file, 0L, SEEK_END);

	*out_fileSize = ftell(file);
	*out_data = (char*)AlignedAlloc(vk_state->rendererAllocator, *out_fileSize, 64, tag);

	rewind(file);
	fread(*out_data, 1, *out_fileSize, file);
	fclose(file);

	return true;
}

bool CreateShaderModule(const char* filename, RendererState* state, VkShaderModule* out_shaderModule)
{
	char* fileData;
	u64 fileSize;
	ReadFile(filename, MEM_TAG_RENDERER_SUBSYS, &fileData, &fileSize);

	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = fileSize;
	// This cast from char* to u32* is possible because darray's are aligned on 64B
	createInfo.pCode = (u32*)fileData;

	if (VK_SUCCESS != vkCreateShaderModule(state->device, &createInfo, state->vkAllocator, out_shaderModule))
	{
		Free(vk_state->rendererAllocator, fileData);
		_ERROR("Shader module creation failed");
		return false;
	}

	Free(vk_state->rendererAllocator, fileData);

	return true;
}

