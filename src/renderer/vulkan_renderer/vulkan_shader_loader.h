#pragma once
#include "defines.h"
#include "containers/darray.h"
#include "vulkan_types.h"


void GetUniformDataFromShader(const char* filename, UniformPropertiesData* ref_propertyData, UniformTexturesData* ref_textureData);
void FreeUniformData(UniformPropertiesData* propertyData, UniformTexturesData* textureData);


bool ReadFile(const char* filename, MemTag tag, char** out_data, u64* out_fileSize);

bool CreateShaderModule(const char* filename, RendererState* state, VkShaderModule* out_shaderModule);
