#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "renderer/renderer.h"

typedef struct Text Text;

/// @brief Initializes the text renderer, should be called by the engine after renderer startup.
/// @return Bool that indicates whether startup was successful or not.
bool InitializeTextRenderer();

/// @brief Shuts down the text renderer, should be called by the engine before renderer shutdown.
void ShutdownTextRenderer();

/// @brief Loads a font for rendering text with.
/// @param fontName What to name the font in the engine.
/// @param fontFileString String with the filepath to the font file.
void TextLoadFont(const char* fontName, const char* fontFileString);// TODO: add parameter for specifying how to load the font (as bezier or fill)

/// @brief Creates a text object for rendering text, will be rendered if it is active when TextRender is called. Is active by default after it's created.
/// @param textString char* to the string.
/// @param fontName String of the name of the font to use.
/// @param transformation Transformation matrix that should be applied when rendering this text (the user should combine the MVP on CPU as this matrix will be directly applied to the text).
/// @param updateFrequency How often the user expects the text of this object to be changed.
/// @return Text* to the object.
Text* TextCreate(const char* textString, const char* fontName, mat4 transformation, UpdateFrequency updateFrequency);


void TextDestroy();// TODO:


void TextSetActive();//TODO:

/// @brief Updates the transform of a text object.
/// @param text The text to update.
/// @param transform The new transform.
void TextUpdateTransform(Text* text, mat4 transform);

/// @brief Renders all the text objects that are currently active.
/// Should be called after rendering the scene, the user should decide whether to call it before or after rendering post processing effects.
/// Should be called ONLY while rendering to a render target with color and depth.
void TextRender();

