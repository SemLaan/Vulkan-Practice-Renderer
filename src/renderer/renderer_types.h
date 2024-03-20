#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "containers/darray.h"

#define TEXTURE_CHANNELS 4
#define DEFAULT_SHADER_NAME "default"

typedef enum RenderTargetUsage
{
	RENDER_TARGET_USAGE_TEXTURE,
	RENDER_TARGET_USAGE_DISPLAY,
	RENDER_TARGET_USAGE_DEPTH,
	RENDER_TARGET_USAGE_NONE
} RenderTargetUsage;

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

typedef struct ShaderCreateInfo
{
	const char* vertexShaderName;
	const char* fragmentShaderName;
	bool renderTargetColor;
	bool renderTargetDepth;
	bool renderTargetStencil;// TODO: this does nothing yet
} ShaderCreateInfo;

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

typedef struct RenderTarget
{
	void* internalState;
} RenderTarget;

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

