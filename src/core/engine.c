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


GRGlobals* global = nullptr;

// Forward declarations
static bool OnQuit(EventCode type, EventData data);
static bool OnResize(EventCode type, EventData data);

#define ENGINE_TOTAL_MEMORY_RESERVE (800 * MiB)
#define FRAME_ARENA_SIZE (100 * MiB)
#define GAME_ALLOCATOR_SIZE (100 * MiB)
#define LARGE_OBJECT_ALLOCATOR_SIZE (50 * MiB)

void EngineInit(EngineInitSettings settings)
{
	// ============================================ Startup ============================================
	// Initializing memory system first
	START_MEMORY_DEBUG_SUBSYS();
	InitializeMemory(ENGINE_TOTAL_MEMORY_RESERVE);

	// Setting up engine globals, before initializing the other subsystems because they might need the globals
	global = AlignedAlloc(GetGlobalAllocator(), sizeof(*global), CACHE_ALIGN);
	global->deltaTime = 0.f;
	global->framerateLimit = settings.framerateLimit;
	global->frameArena = Alloc(GetGlobalAllocator(), sizeof(*global->frameArena));
	*global->frameArena = ArenaCreate(GetGlobalAllocator(), FRAME_ARENA_SIZE);
	CreateFreelistAllocator("Game Allocator", GetGlobalAllocator(), GAME_ALLOCATOR_SIZE, &global->gameAllocator, false);
	CreateFreelistAllocator("Large Object Allocator", GetGlobalAllocator(), LARGE_OBJECT_ALLOCATOR_SIZE, &global->largeObjectAllocator, false);

	RendererInitSettings rendererInitSettings = {};
	rendererInitSettings.presentMode = settings.presentMode;

	InitializeEvent();
	InitializeInput();
	InitializePlatform(settings.windowTitle, settings.startResolution.x, settings.startResolution.y);
	InitializeRenderer(rendererInitSettings);
	InitializeTextRenderer();
	InitializeDebugUI();
	InitializeProfilingUI();

	global->appRunning = true;
	global->appSuspended = false;
	StartOrResetTimer(&global->timer);
	global->previousFrameTime = TimerSecondsSinceStart(global->timer);

	RegisterEventListener(EVCODE_QUIT, OnQuit);
	RegisterEventListener(EVCODE_WINDOW_RESIZED, OnResize);
}

bool EngineUpdate()
{
	ArenaClear(global->frameArena);
	f64 currentTime = TimerSecondsSinceStart(global->timer);
	global->deltaTime = currentTime - global->previousFrameTime;
	while (global->deltaTime <= 1.f / (f32)global->framerateLimit)
	{
		currentTime = TimerSecondsSinceStart(global->timer);
		global->deltaTime = currentTime - global->previousFrameTime;
	}
	global->previousFrameTime = currentTime;

	PreMessagesInputUpdate();
	PlatformProcessMessage();
	PostMessagesInputUpdate();

	// TODO: sleep platform every loop if app suspended to not waste pc resources
	while (global->appSuspended)
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

	return global->appRunning;
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

	ArenaDestroy(global->frameArena, GetGlobalAllocator());
	Free(GetGlobalAllocator(), global->frameArena);
	DestroyFreelistAllocator(global->largeObjectAllocator);
	DestroyFreelistAllocator(global->gameAllocator);
	Free(GetGlobalAllocator(), global);

	ShutdownMemory();
	SHUTDOWN_MEMORY_DEBUG_SUBSYS();

	WriteLogsToFile();
}


static bool OnQuit(EventCode type, EventData data)
{
	WaitForGPUIdle();
	global->appRunning = false;
	return false;
}

static bool OnResize(EventCode type, EventData data)
{
	if (data.u32[0] == 0 || data.u32[1] == 0)
	{
		global->appSuspended = true;
		_INFO("App suspended");
	}
	else if (global->appSuspended)
	{
		global->appSuspended = false;
		_INFO("App unsuspended");
	}
	return false;
}