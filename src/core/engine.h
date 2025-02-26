#pragma once
#include "../defines.h"
#include "memory/arena.h"
#include "timer.h"

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

void EngineInit();
bool EngineUpdate();
void EngineShutdown();



