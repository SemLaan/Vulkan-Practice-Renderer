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
	u32 framerateLimit;
} EngineInitSettings;

typedef struct GRGlobals
{
	Allocator* gameAllocator;
	Allocator* largeObjectAllocator;
	Arena* frameArena;
	Timer timer;
	f64 deltaTime;
	f64 previousFrameTime;
	u32 framerateLimit;
	bool appRunning;
	bool appSuspended;
} GRGlobals;

extern GRGlobals* global;

void EngineInit(EngineInitSettings settings);
bool EngineUpdate();
void EngineShutdown();



