#pragma once
#include "defines.h"


/// @brief The root debug UI node.
typedef struct DebugMenu DebugMenu;

/// @brief Initializes debug UI "subsystem". Should be called after text renderer startup.
/// @return bool - Whether or not initialization was successful or not.
bool InitializeDebugUI();


void ShutdownDebugUI();

/// @brief Should only be called by the engine, during the update loop, just before control is passed to the game code.
/// Handles user interaction with the menu.
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
/// @param pStateBool Pointer to a client bool where the state of the button will be stored, the Debug UI menu will update the bool in this location whenever the button is pressed or released. (Can be nullptr if not needed)
/// @param pSignalBool Pointer to a client bool that will be set to true the frame that the button is pressed, note: the client is expected to set it to false 
//  again the same frame otherwise new presses won't be signalled (releasing the button while still hovering over it counts as a press). (Can be nullptr if not needed)
void DebugUIAddButton(DebugMenu* menu, const char* text, bool* pStateBool, bool* pSignalBool);

