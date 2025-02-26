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
#include <stdio.h>
#include "engine.h"

static bool appRunning;
static bool appSuspended;

// Forward declarations
static bool OnQuit(EventCode type, EventData data);
static bool OnResize(EventCode type, EventData data);

void EngineInit()
{
    // ============================================ Startup ============================================
    START_MEMORY_DEBUG_SUBSYS();
    InitializeMemory(100 * MiB);
    InitializeEvent();
    InitializeInput();
    InitializePlatform("Beef", 200, 100);
    InitializeRenderer();
    InitializeTextRenderer();
    InitializeDebugUI();

    appRunning = true;
    appSuspended = false;

    RegisterEventListener(EVCODE_QUIT, OnQuit);
    RegisterEventListener(EVCODE_WINDOW_RESIZED, OnResize);
}

bool EngineUpdate()
{
    UpdateInput();
    PlatformProcessMessage();

    // TODO: sleep platform every loop if app suspended to not waste pc resources
    while (appSuspended)
    {
		UpdateInput();
    	PlatformProcessMessage();
    }

    if (GetKeyDown(KEY_F11) && !GetKeyDownPrevious(KEY_F11))
        ToggleFullscreen();

    UpdateDebugUI();

    if (GetKeyDown(KEY_ESCAPE))
    {
        EventData evdata;
        InvokeEvent(EVCODE_QUIT, evdata);
    }

	return appRunning;
}

void EngineShutdown()
{
	// ============================================ Shutdown ============================================
    WaitForGPUIdle();

    UnregisterEventListener(EVCODE_QUIT, OnQuit);
    UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnResize);

    ShutdownDebugUI();
    ShutdownTextRenderer();
    ShutdownRenderer();
    ShutdownPlatform();
    ShutdownInput();
    ShutdownEvent();
    ShutdownMemory();
    SHUTDOWN_MEMORY_DEBUG_SUBSYS();

    WriteLogsToFile();
}


static bool OnQuit(EventCode type, EventData data)
{
	WaitForGPUIdle();
    appRunning = false;
    return false;
}

static bool OnResize(EventCode type, EventData data)
{
    if (data.u32[0] == 0 || data.u32[1] == 0)
    {
        appSuspended = true;
        _INFO("App suspended");
    }
    else if (appSuspended)
    {
        appSuspended = false;
        _INFO("App unsuspended");
    }
    return false;
}