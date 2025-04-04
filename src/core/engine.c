#include "core/asserts.h"
#include "core/event.h"
#include "core/input.h"
#include "core/logger.h"
#include "core/meminc.h"
#include "core/platform.h"
#include "defines.h"
#include "game/game.h"
#include "renderer/renderer.h"
#include "renderer/ui/debug_ui.h"
#include "renderer/ui/text_renderer.h"
#include "renderer/ui/profiling_ui.h"
#include <stdio.h>
#include "engine.h"


GRGlobals* grGlobals = nullptr;

// Forward declarations
static bool OnQuit(EventCode type, EventData data);
static bool OnResize(EventCode type, EventData data);

#define ENGINE_TOTAL_MEMORY_RESERVE (300 * MiB)
#define FRAME_ARENA_SIZE (100 * MiB)
#define GAME_ALLOCATOR_SIZE (100 * MiB)

void EngineInit(EngineInitSettings settings)
{
	// ============================================ Startup ============================================
	// Initializing memory system first
	START_MEMORY_DEBUG_SUBSYS();
	InitializeMemory(ENGINE_TOTAL_MEMORY_RESERVE);

	// Setting up engine globals, before initializing the other subsystems because they might need the globals
	grGlobals = AlignedAlloc(GetGlobalAllocator(), sizeof(*grGlobals), CACHE_ALIGN);
	grGlobals->deltaTime = 0.f;
	grGlobals->framerateLimit = settings.framerateLimit;
	grGlobals->frameArena = Alloc(GetGlobalAllocator(), sizeof(*grGlobals->frameArena));
	*grGlobals->frameArena = ArenaCreate(GetGlobalAllocator(), FRAME_ARENA_SIZE);
	CreateFreelistAllocator("Game Allocator", GetGlobalAllocator(), GAME_ALLOCATOR_SIZE, &grGlobals->gameAllocator, false);

	RendererInitSettings rendererInitSettings = {};
	rendererInitSettings.presentMode = settings.presentMode;

	InitializeEvent();
	InitializeInput();
	InitializePlatform(settings.windowTitle, settings.startResolution.x, settings.startResolution.y);
	InitializeRenderer(rendererInitSettings);
	InitializeTextRenderer();
	InitializeDebugUI();
	InitializeProfilingUI();

	grGlobals->appRunning = true;
	grGlobals->appSuspended = false;
	StartOrResetTimer(&grGlobals->timer);
	grGlobals->previousFrameTime = TimerSecondsSinceStart(grGlobals->timer);

	RegisterEventListener(EVCODE_QUIT, OnQuit);
	RegisterEventListener(EVCODE_WINDOW_RESIZED, OnResize);
}

bool EngineUpdate()
{
	ArenaClear(grGlobals->frameArena);
	f64 currentTime = TimerSecondsSinceStart(grGlobals->timer);
	grGlobals->deltaTime = currentTime - grGlobals->previousFrameTime;
	while (grGlobals->deltaTime <= 1.f / (f32)grGlobals->framerateLimit)
	{
		currentTime = TimerSecondsSinceStart(grGlobals->timer);
		grGlobals->deltaTime = currentTime - grGlobals->previousFrameTime;
	}
	grGlobals->previousFrameTime = currentTime;

	PreMessagesInputUpdate();
	PlatformProcessMessage();
	PostMessagesInputUpdate();

	// TODO: sleep platform every loop if app suspended to not waste pc resources
	while (grGlobals->appSuspended)
	{
		PreMessagesInputUpdate();
		PlatformProcessMessage();
		PostMessagesInputUpdate();
	}

	if (GetKeyDown(KEY_F11) && !GetKeyDownPrevious(KEY_F11))
		ToggleFullscreen();

	UpdateProfilingUI();
	UpdateDebugUI();

	if (GetKeyDown(KEY_ESCAPE))
	{
		EventData evdata;
		InvokeEvent(EVCODE_QUIT, evdata);
	}

	return grGlobals->appRunning;
}

void EngineShutdown()
{
	// ============================================ Shutdown ============================================
	WaitForGPUIdle();

	UnregisterEventListener(EVCODE_QUIT, OnQuit);
	UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnResize);

	ShutdownProfilingUI();
	ShutdownDebugUI();
	ShutdownTextRenderer();
	ShutdownRenderer();
	ShutdownPlatform();
	ShutdownInput();
	ShutdownEvent();

	ArenaDestroy(grGlobals->frameArena, GetGlobalAllocator());
	Free(GetGlobalAllocator(), grGlobals->frameArena);
	DestroyFreelistAllocator(grGlobals->gameAllocator);
	Free(GetGlobalAllocator(), grGlobals);

	ShutdownMemory();
	SHUTDOWN_MEMORY_DEBUG_SUBSYS();

	WriteLogsToFile();
}


static bool OnQuit(EventCode type, EventData data)
{
	WaitForGPUIdle();
	grGlobals->appRunning = false;
	return false;
}

static bool OnResize(EventCode type, EventData data)
{
	if (data.u32[0] == 0 || data.u32[1] == 0)
	{
		grGlobals->appSuspended = true;
		_INFO("App suspended");
	}
	else if (grGlobals->appSuspended)
	{
		grGlobals->appSuspended = false;
		_INFO("App unsuspended");
	}
	return false;
}