#pragma once
#include "defines.h"
#include "math/math_types.h"

#define DEBUG_UI_FONT_NAME "debug_font"
#define DEBUG_UI_DEFAULT_MENU_GROUP_NAME "default"

/// @brief The root debug UI node.
typedef struct DebugMenu DebugMenu;

/// @brief Initializes debug UI "subsystem". Should be called after text renderer startup.
/// @return bool - Whether or not initialization was successful or not.
bool InitializeDebugUI();


void ShutdownDebugUI();

/// @brief Should only be called by the engine, during the update loop, just before control is passed to the game code.
/// Handles user interaction with the menu.
void UpdateDebugUI();

bool DebugUIGetInputConsumed();


/// @brief Creates a Debug UI menu instance.
/// @return Pointer to the menu.
DebugMenu* DebugUICreateMenu(const char* title, const char* menuGroupName, u32 priority);

/// @brief Destroys the given Debug UI menu instance.
/// @param menu Pointer to the menu.
void DebugUIDestroyMenu(DebugMenu* menu);

/// @brief Renders the active Debug UI menu instances.
void DebugUIRenderMenus();

void DebugUISetMaterialValues(DebugMenu* menu, vec4 color, vec4 other);

void DebugUIMenuSetActive(DebugMenu* menu, bool active);

/// @brief Adds a button to the bottom of the Debug UI menu with some text.
/// @param menu Pointer to the menu that the button should be added to.
/// @param text Text that should be displayed in the button.
/// @param pStateBool Pointer to a client bool where the state of the button will be stored, the Debug UI menu will update the bool in this location whenever the button is pressed or released. (Can be nullptr if not needed)
/// @param pSignalBool Pointer to a client bool that will be set to true the frame that the button is pressed, note: the client is expected to set it to false 
//  again the same frame otherwise new presses won't be signalled (releasing the button while still hovering over it counts as a press). (Can be nullptr if not needed)
void DebugUIAddButton(DebugMenu* menu, const char* text, bool* pStateBool, bool* pSignalBool);

/// @brief Adds a toggle button to the bottom of the Debug UI menu with some text.
/// @param menu Pointer to the menu that the button should be added to.
/// @param text Text that should be displayed in the button.
/// @param pStateBool Pointer to a bool that will hold the state of the button
void DebugUIAddToggleButton(DebugMenu* menu, const char* text, bool* pStateBool);

/// @brief Adds a slider that controls a float value to the bottom of the Debug UI menu with some text.
/// @param menu Pointer to the menu that the button should be added to.
/// @param text Text that should be displayed in the button.
/// @param minValue Minimum value of the slider.
/// @param maxValue Maximum value of the slider.
/// @param pSliderValue Pointer to where the current value of the slider should be stored. If the value being stored in this position is between minValue and maxValue the slider will start at this value, otherwise the slider will start at minValue.
void DebugUIAddSliderFloat(DebugMenu* menu, const char* text, f32 minValue, f32 maxValue, f32* pSliderValue);

/// @brief Adds a slider that controls an int value to the bottom of the Debug UI menu with some text.
/// @param menu Pointer to the menu that the button should be added to.
/// @param text Text that should be displayed in the button.
/// @param minValue Minimum value of the slider.
/// @param maxValue Maximum value of the slider.
/// @param pSliderValue Pointer to where the current value of the slider should be stored. If the value being stored in this position is between minValue and maxValue the slider will start at this value, otherwise the slider will start at minValue.
void DebugUIAddSliderInt(DebugMenu* menu, const char* text, i64 minValue, i64 maxValue, i64* pSliderValue);

/// @brief Adds a slider that controls a discrete int value to the bottom of the Debug UI menu with some text.
/// @param menu Pointer to the menu that the button should be added to.
/// @param text Text that should be displayed in the button.
/// @param discreteValuesArray Array of the discrete values that this slider can represent, this array should remain valid for as long as this slider exists.
/// @param discreteValueCount Amount of elements in the array.
/// @param pSliderValue Pointer to where the current value of the slider should be stored. If the value being stored in this position is between minValue and maxValue the slider will start at this value, otherwise the slider will start at minValue.
void DebugUIAddSliderDiscrete(DebugMenu* menu, const char* text, i64* discreteValuesArray, u64 discreteValueCount, i64* pSliderValue);

/// @brief Adds a slider that controls a float value and shows it's value on a logarithmic scale to the user to the bottom of the Debug UI menu with some text.
/// @param menu Pointer to the menu that the button should be added to.
/// @param text Text that should be displayed in the button.
/// @param base Base of the logarithm used to scale the sliders values.
/// @param minValue Minimum value of the slider.
/// @param maxValue Maximum value of the slider.
/// @param pSliderValue Pointer to where the current value of the slider should be stored. If the value being stored in this position is between minValue and maxValue the slider will start at this value, otherwise the slider will start at minValue.
void DebugUIAddSliderLog(DebugMenu* menu, const char* text, f32 base, f32 minValue, f32 maxValue, f32* pSliderValue);

