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


/// @brief Creates a Debug UI menu instance.
/// @return Pointer to the menu.
DebugMenu* DebugUICreateMenu();

/// @brief Destroys the given Debug UI menu instance.
/// @param menu Pointer to the menu.
void DebugUIDestroyMenu(DebugMenu* menu);

/// @brief Renders the given Debug UI menu instance.
/// @param menu Pointer to the menu.
void DebugUIRenderMenu(DebugMenu* menu);

/// @brief Adds a button to the bottom of the Debug UI menu with some text.
/// @param menu Pointer to the menu that the button should be added to.
/// @param text Text that should be displayed in the button.
/// @param boolPointer Pointer to the location where the client bool is stored, the Debug UI menu will update the bool in this location whenever the button is pressed or released.
void DebugUIAddButton(DebugMenu* menu, const char* text, bool* boolPointer);

