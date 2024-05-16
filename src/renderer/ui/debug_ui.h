#pragma once
#include "defines.h"


/// @brief The root debug UI node.
typedef struct DebugMenu DebugMenu;

/// @brief Initializes debug UI "subsystem". Should be called after text renderer startup.
/// @return bool - Whether or not initialization was successful or not.
bool InitializeDebugUI();


void ShutdownDebugUI();

/// @brief Should only be called by the engine, during the update loop, just before control is passed to the game code.
void UpdateDebugUI();


DebugMenu* DebugUICreateMenu();
void DebugUIDestroyMenu(DebugMenu* menu);

void DebugUIRenderMenu(DebugMenu* menu);



