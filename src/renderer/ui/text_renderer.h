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


Text* TextCreate(const char* textString, const char* fontName, mat4 transformation, UpdateFrequency updateFrequency);


void TextDestroy();


void TextSetActive();


void TextUpdateTransform(mat4 transformation);


void TextRender();

