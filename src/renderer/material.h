#pragma once
#include "defines.h"
#include "renderer_types.h"


// Creates a material object based of a shader and returns a handle to that object
Material MaterialCreate(Shader clientShader);
// Destroys a material object
void MaterialDestroy(Material clientMaterial);

// Updates the uniform values of the given material
void MaterialUpdateProperties(Material clientMaterial, GlobalUniformObject* properties);
// Binds the material for rendering
void MaterialBind(Material clientMaterial);
