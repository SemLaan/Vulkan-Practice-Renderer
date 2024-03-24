#pragma once
#include "defines.h"
#include "renderer_types.h"


// Creates a material object based of a shader and returns a handle to that object
Material MaterialCreate(Shader clientShader);
// Destroys a material object
void MaterialDestroy(Material clientMaterial);

// Updates the uniform values of the given material
void MaterialUpdateProperty(Material clientMaterial, const char* name, void* value);
// Updates the uniform values of the given material
void MaterialUpdateTexture(Material clientMaterial, const char* name, Texture clientTexture, SamplerType samplerType);
// Binds the material for rendering
void MaterialBind(Material clientMaterial);
