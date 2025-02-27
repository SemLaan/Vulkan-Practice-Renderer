#pragma once
#include "../defines.h"
#include "memory/arena.h"
#include "timer.h"
#include "renderer/renderer.h"

typedef struct EngineInitSettings 
{
	const char* windowTitle;
	vec2i startResolution;
	GrPresentMode presentMode;
} EngineInitSettings;

typedef struct GRGlobals
{
	Allocator* gameAllocator;
	Arena* frameArena;
	Timer timer;
	f64 deltaTime;
	f64 previousFrameTime;
	bool appRunning;
	bool appSuspended;
} GRGlobals;

extern GRGlobals* grGlobals;

void EngineInit(EngineInitSettings settings);
bool EngineUpdate();
void EngineShutdown();



