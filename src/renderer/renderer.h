#pragma once
#include "defines.h"
#include "buffer.h"


// ============================================= Engine functions ====================================================
bool InitializeRenderer();
void ShutdownRenderer();

void RecreateSwapchain();

bool RenderFrame();

// ============================================= Game code functions ====================================================


