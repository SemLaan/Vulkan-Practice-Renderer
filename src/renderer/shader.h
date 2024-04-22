#pragma once
#include "defines.h"
#include "renderer_types.h"


// Creates a shader object and returns a handle to that shader
void ShaderCreate(const char* shaderName, ShaderCreateInfo* pCreateInfo);
// Destroys a shader object with the corresponding name
void ShaderDestroy(const char* shaderName);

// Returns a reference to the shader. NO REFERENCE COUNTING it is the programmers responsibility that this shader exists when it is needed.
Shader ShaderGetRef(const char* shaderName);

