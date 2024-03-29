#pragma once
#include "defines.h"

#include "renderer_types.h"


// Creates a handle to a render target
RenderTarget RenderTargetCreate(u32 width, u32 height, RenderTargetUsage colorBufferUsage, RenderTargetUsage depthBufferUsage);
// Destroys the given render target
void RenderTargetDestroy(RenderTarget clientRenderTarget);

// All following draw commands will be drawn to this render target 
// NOTE: no other render target needs to be rendering before calling this function
void RenderTargetStartRendering(RenderTarget clientRenderTarget);
// Ends rendering and resolves the render target
void RenderTargetStopRendering(RenderTarget clientRenderTarget);

Texture GetColorAsTexture(RenderTarget clientRenderTarget);
Texture GetDepthAsTexture(RenderTarget clientRenderTarget);

