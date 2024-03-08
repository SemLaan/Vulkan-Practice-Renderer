#pragma once
#include "defines.h"
#include "renderer_types.h"


// Creates a shader object and returns a handle to that shader
Shader ShaderCreate();
// Destroys a shader object
void ShaderDestroy(Shader clientShader);
