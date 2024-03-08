#pragma once
#include "defines.h"
#include "containers/darray.h"
#include "vulkan_types.h"


bool ReadFile(const char* filename, MemTag tag, char** out_data, u64* out_fileSize);

bool CreateShaderModule(const char* filename, RendererState* state, VkShaderModule* out_shaderModule);
