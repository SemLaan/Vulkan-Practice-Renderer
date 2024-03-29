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

// Enum with possible sampler configurations
typedef enum SamplerType
{
	SAMPLER_TYPE_NEAREST_CLAMP_EDGE,
	SAMPLER_TYPE_NEAREST_REPEAT,
	SAMPLER_TYPE_LINEAR_CLAMP_EDGE,
	SAMPLER_TYPE_LINEAR_REPEAT,
	SAMPLER_TYPE_ANISOTROPIC_CLAMP_EDGE,
	SAMPLER_TYPE_ANISOTROPIC_REPEAT,
	SAMPLER_TYPE_SHADOW,
} SamplerType;

typedef enum VertexAttributeType
{
	VERTEX_ATTRIBUTE_TYPE_FLOAT,
	VERTEX_ATTRIBUTE_TYPE_VEC2,
	VERTEX_ATTRIBUTE_TYPE_VEC3,
	VERTEX_ATTRIBUTE_TYPE_VEC4,
	VERTEX_ATTRIBUTE_TYPE_MAT4,
} VertexAttributeType;

#define MAX_VERTEX_ATTRIBUTES 15
// Tells the shader how to read in vertex data
typedef struct VertexBufferLayout
{
	u32 perVertexAttributeCount;
	u32 perInstanceAttributeCount;
	VertexAttributeType perVertexAttributes[MAX_VERTEX_ATTRIBUTES];
	VertexAttributeType perInstanceAttributes[MAX_VERTEX_ATTRIBUTES];
} VertexBufferLayout;

typedef struct ShaderCreateInfo
{
	const char* vertexShaderName;
	const char* fragmentShaderName;
	VertexBufferLayout vertexBufferLayout;
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

