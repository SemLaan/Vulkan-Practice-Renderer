#include <stdio.h>
#include "defines.h"
#include "core/meminc.h"
#include "core/logger.h"
#include "core/asserts.h"
#include "core/event.h"
#include "core/input.h"
#include "core/platform.h"

static bool appRunning;
static bool appSuspended;

int main()
{
    // ============================================ Startup ============================================
	START_MEMORY_DEBUG_SUBSYS();
    InitializeMemory(4 * MiB);
    InitializeEvent();
    InitializeInput();
    InitializePlatform("Beef", 200, 100);

    appRunning = true;
    appSuspended = false;

    RegisterEventListener(EVCODE_QUIT, OnQuit);
    RegisterEventListener(EVCODE_WINDOW_RESIZED, OnResize);

    // ============================================ Run ============================================
    _INFO("Testing beef: %i", 34);

    while (appRunning)
    {

        UpdateInput();
		PlatformProcessMessage();

        // TODO: sleep platform every loop if app suspended to not waste pc resources
		if (!appSuspended)
		{
            // TODO: game update and render
			//RenderFrame();
			if (GetKeyDown(KEY_ESCAPE))
            {
                EventData evdata;
				InvokeEvent(EVCODE_QUIT, evdata);
            }
		}
    }
    

    // ============================================ Shutdown ============================================
    UnregisterEventListener(EVCODE_QUIT, OnQuit);
    UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnResize);

    ShutdownPlatform();
    ShutdownInput();
    ShutdownEvent();
    ShutdownMemory();
	SHUTDOWN_MEMORY_DEBUG_SUBSYS();

    WriteLogsToFile();

    return 0;
}

static bool OnQuit(EventCode type, EventData data)
{
	appRunning = false;
	return false;
}

static bool OnResize(EventCode type, EventData data)
{
	if (data.u32[0] == 0 || data.u32[1] == 0)
	{
		appSuspended = true;
		GRINFO("App suspended");
	}
	else if (appSuspended)
	{
		appSuspended = false;
		GRINFO("App unsuspended");
	}
	return false;
}