#pragma once
#include "defines.h"
#include "buffer.h"
#include "material.h"
#include "shader.h"



typedef enum UpdateFrequency
{
    UPDATE_FREQUENCY_STATIC,
    UPDATE_FREQUENCY_DYNAMIC,
} UpdateFrequency;

typedef enum GrPresentMode
{
	GR_PRESENT_MODE_FIFO,
	GR_PRESENT_MODE_MAILBOX,
} GrPresentMode;

typedef struct RendererInitSettings
{
	GrPresentMode presentMode;
} RendererInitSettings;

// ============================================= Engine functions ====================================================
bool InitializeRenderer(RendererInitSettings settings);
void ShutdownRenderer();

void WaitForGPUIdle();

void RecreateSwapchain();

bool BeginRendering();
void EndRendering();

void UpdateGlobalUniform(GlobalUniformObject* properties);
void Draw(u32 vertexBufferCount, VertexBuffer* clientVertexBuffers, IndexBuffer clientIndexBuffer, mat4* pushConstantValues, u32 instanceCount);
void DrawInstancedIndexed(u32 vertexBufferCount, VertexBuffer* clientVertexBuffers, IndexBuffer clientIndexBuffer, mat4* pushConstantValues, u32 instanceCount, u32 firstInstance);
void DrawBufferRange(u32 vertexBufferCount, VertexBuffer* clientVertexBuffers, u64* vbOffsets, IndexBuffer clientIndexBuffer, mat4* pushConstantValues, u32 instanceCount);

RenderTarget GetMainRenderTarget();

/// @brief Basic meshes are loaded by the engine and can be retrieved using this function.
/// @param meshName String with the mesh name. There are defines for the options that have the form: BASIC_MESH_NAME_(type).
/// @return Pointer to GPUMesh struct that contains the vertex and index buffer of the requested mesh.
GPUMesh* GetBasicMesh(const char* meshName);

vec4 ScreenToClipSpace(vec4 coordinates);
