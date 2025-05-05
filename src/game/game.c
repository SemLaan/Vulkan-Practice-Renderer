#include "game.h"

#include "containers/darray.h"
#include "core/engine.h"
#include "core/input.h"
#include "core/logger.h"
#include "game_rendering.h"
#include "world_generation.h"
#include "math/lin_alg.h"
#include "player_controller.h"
#include "raycast_demo.h"

int main()
{
    // ================================================================== Startup
	EngineInitSettings engineSettings = {};
	engineSettings.presentMode = GR_PRESENT_MODE_FIFO;
	engineSettings.startResolution = (vec2i){ .x = 200, .y = 200};
	engineSettings.windowTitle = "Beefbal Beefer 44";
	engineSettings.framerateLimit = 122;
    EngineInit(engineSettings);

	WorldGenerationInit();
    GameRenderingInit();
	RaycastDemoInit();
    PlayerControllerInit();

    // ================================================================= Game loop
    while (EngineUpdate())
    {
        // =========================== Update ===================================
		WorldGenerationUpdate();
        PlayerControllerUpdate();
		RaycastDemoUpdate();
        GameRenderingRender();
    }

    // ================================================================== Shutdown
    PlayerControllerShutdown();
	RaycastDemoShutdown();
    GameRenderingShutdown();
	WorldGenerationShutdown();

    EngineShutdown();
}
