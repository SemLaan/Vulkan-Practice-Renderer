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

RenderTarget GetMainRenderTarget();
