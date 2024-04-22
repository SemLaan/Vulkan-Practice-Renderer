#pragma once
#include "defines.h"
#include "vulkan_types.h"

/// @brief Destroys a shader object. Does not remove it from the shaderMap, but it obviously invalidates the shader. 
/// This should only be used if the shader map is destroyed after.
/// @param shader Pointer to the VulkanShader to destroy.
void ShaderDestroyInternal(VulkanShader* shader);

 