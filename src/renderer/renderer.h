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

// ============================================= Engine functions ====================================================
bool InitializeRenderer();
void ShutdownRenderer();

void WaitForGPUIdle();

void RecreateSwapchain();

bool BeginRendering();
void EndRendering();

void UpdateGlobalUniform(GlobalUniformObject* properties);
void Draw(u32 vertexBufferCount, VertexBuffer* clientVertexBuffers, IndexBuffer clientIndexBuffer, mat4* pushConstantValues, u32 instanceCount);
void DrawBufferRange(u32 vertexBufferCount, VertexBuffer* clientVertexBuffers, u64* vbOffsets, IndexBuffer clientIndexBuffer, mat4* pushConstantValues, u32 instanceCount);

RenderTarget GetMainRenderTarget();

/// @brief Basic meshes are loaded by the engine and can be retrieved using this function.
/// @param meshName String with the mesh name. There are defines for the options that have the form: BASIC_MESH_NAME_(type).
/// @return Pointer to MeshData struct that contains the vertex and index buffer of the requested mesh.
MeshData* GetBasicMesh(const char* meshName);

vec4 ScreenToClipSpace(vec4 coordinates);
