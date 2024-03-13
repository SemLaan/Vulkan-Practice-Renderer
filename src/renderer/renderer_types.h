#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "containers/darray.h"

#define TEXTURE_CHANNELS 4


// TODO: try not to use this for anything new because this will probably get removed
typedef struct Vertex
{
	vec3 position;
	vec3 normal;
	vec2 texCoord;
} Vertex;

// Handle to a vertex buffer
typedef struct VertexBuffer
{
	void* internalState;
} VertexBuffer;

// Handle to an index buffer
typedef struct IndexBuffer
{
	void* internalState;
} IndexBuffer;

// Handle to a texture
typedef struct Texture
{
	void* internalState;
} Texture;

// Handle to a shader
typedef struct Shader
{
	void* internalState;
} Shader;

// Handle to a material
typedef struct Material
{
	void* internalState;
} Material;

typedef struct GlobalUniformObject
{
	_Alignas(16) mat4 projView;
	_Alignas(16) vec3 viewPosition;
	_Alignas(16) vec3 directionalLight;
} GlobalUniformObject;

typedef struct PushConstantObject
{
	mat4 model;
} PushConstantObject;

