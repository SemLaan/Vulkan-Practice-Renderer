#pragma once
#include "containers/darray.h"
#include "defines.h"
#include "math/math_types.h"

#define TEXTURE_CHANNELS 4
#define DEFAULT_SHADER_NAME "default"
#define BASIC_MESH_NAME_QUAD "QUAD"
#define BASIC_MESH_NAME_SPHERE "SPHERE"
#define BASIC_MESH_NAME_CUBE "CUBE_"
#define BASIC_MESH_NAME_FULL_SCREEN_TRIANGLE "FULL_SCREEN_TRI"

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

/// @brief Struct with a handle to a vertex buffer and an index buffer that make up a mesh.
typedef struct GPUMesh
{
	VertexBuffer vertexBuffer;
	IndexBuffer indexBuffer;
} GPUMesh;

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

#define CULL_BACK 0
#define CULL_FRONT 1

typedef struct ShaderCreateInfo
{
    const char* vertexShaderName;          // String with the filepath of the vertex shader.
    const char* fragmentShaderName;        // String with the filepath of the fragment shader. If this is nullptr the pipeline will be created without a fragment shader, this can be usefull for certain rendering techniques (e.g. shadowmaps).
    VertexBufferLayout vertexBufferLayout; // Struct with information about the per vertex and instanced vertex layout.
	u32 cullMode;						   // Leave zeroed for back face, 1 for front face
    bool renderTargetColor;                // Whether or not the render target has a color buffer.
    bool renderTargetDepth;                // Whether or not the render target has a depth buffer (also determines if depth testing is on or off).
    bool renderTargetStencil;              // TODO: this does nothing yet
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
    _Alignas(16) mat4 viewProjection;
    _Alignas(16) vec3 viewPosition;
    _Alignas(16) vec3 directionalLight;
} GlobalUniformObject;

typedef struct PushConstantObject
{
    mat4 model;
} PushConstantObject;
