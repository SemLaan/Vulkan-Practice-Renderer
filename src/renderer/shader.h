#pragma once
#include "defines.h"
#include "renderer_types.h"


/// @brief Creates a shader object that can be retrieved by callin ShaderGetRef.
/// @param shaderName String with the name of the shader.
/// @param pCreateInfo Struct with information about the shader.
void ShaderCreate(const char* shaderName, ShaderCreateInfo* pCreateInfo);

/// @brief Destroys a shader object with the corresponding name.
/// @param shaderName String with the name of the shader.
void ShaderDestroy(const char* shaderName);

/// @brief Returns a reference to the shader. NO REFERENCE COUNTING it is the programmers responsibility that this shader exists when it is needed.
/// @param shaderName String with the name of the shader.
/// @return Shader handle, internal state is nullptr if shader doesn't exist.
Shader ShaderGetRef(const char* shaderName);

