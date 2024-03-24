#pragma once
#include "defines.h"
#include "buffer.h"
#include "material.h"
#include "shader.h"


// ============================================= Engine functions ====================================================
bool InitializeRenderer();
void ShutdownRenderer();

void WaitForGPUIdle();

void RecreateSwapchain();

bool BeginRendering();
void EndRendering();

void UpdateGlobalUniform(GlobalUniformObject* properties);
void Draw(Material clientMaterial, VertexBuffer clientVertexBuffer, IndexBuffer clientIndexBuffer, mat4* pushConstantValues);

RenderTarget GetMainRenderTarget();
