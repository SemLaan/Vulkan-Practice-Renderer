#pragma once
#include "defines.h"
#include "core/logger.h"
#include "math/math_types.h"


bool InitializePlatform(const char* windowName, u32 windowWidth, u32 windowHeight);

void ShutdownPlatform();

// Exported for testing, shouldn't be used by applications
void PlatformProcessMessage();

void PlatformLogString(log_level level, const char* message);

vec2i GetPlatformWindowSize();

void SetMousePosition(vec2i position);
void SetWindowTitle(const char* windowName);

void ToggleFullscreen();
void SetFullscreen(bool enabled);

// Returns time since system boot in seconds
f64 PlatformGetTime();