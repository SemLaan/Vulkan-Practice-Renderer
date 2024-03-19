#pragma once
#include "defines.h"

#include "renderer_types.h"


RenderTarget RenderTargetCreate(u32 width, u32 height, RenderTargetUsage colorBufferUsage, RenderTargetUsage depthBufferUsage);
void RenderTargetDestroy(RenderTarget clientRenderTarget);

void RenderTargetStartRendering(RenderTarget clientRenderTarget);
void RenderTargetStopRendering(RenderTarget clientRenderTarget);

Texture GetColorAsTexture(RenderTarget clientRenderTarget);
Texture GetDepthAsTexture(RenderTarget clientRenderTarget);

