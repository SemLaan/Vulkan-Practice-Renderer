#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "renderer/renderer.h"

DEFINE_DARRAY_TYPE_REF(char);
DEFINE_DARRAY_TYPE(u32);

// TODO: remove if not needed
// typedef struct Text
//{
// u64 id;
// char* string;
// bool enabled;
//} Text;

typedef struct GlyphInstanceData
{
    // Each pair of vec2's is sent to the gpu as a single vec4 to save on vertex attributes
    vec2 localPosition;                // .xy = local position
    vec2 localScale;                   // .zw = local scale
    vec4 textureCoordinatePair; // .xy = texture coordinates  .zw = texture coordinates
} GlyphInstanceData;

DEFINE_DARRAY_TYPE(GlyphInstanceData);

#define MAX_RENDERABLE_CHARACTERS_PER_FONT 255

/// @brief Struct with all the data necessary to render text with a font.
typedef struct Font
{
	Texture textureMap;
	vec4 textureCoordinates[MAX_RENDERABLE_CHARACTERS_PER_FONT];
	vec2 glyphSizes[MAX_RENDERABLE_CHARACTERS_PER_FONT];	// Glyph size is { 0, 0 } if it represents a character with no glyph, like space
	u32 renderableCharacters[MAX_RENDERABLE_CHARACTERS_PER_FONT];
	f32 advanceWidths[MAX_RENDERABLE_CHARACTERS_PER_FONT];
	f32 yOffsets[MAX_RENDERABLE_CHARACTERS_PER_FONT];
	f32 xPadding;
	f32 spaceAdvanceWidth;
	u32 characterCount;
} Font;

typedef struct TextBatch
{
    GlyphInstanceDataDarray* glyphInstanceData;
    VertexBuffer glyphInstancesBuffer;
    charRefDarray* strings;
    u32Darray* stringLengths;
	Font* font;
    Material textMaterial;         //
    u32 gpuBufferInstanceCapacity; // TODO: remove once vertex buffer resizing is a thing
} TextBatch;

/// @brief Initializes the text renderer, should be called by the engine after renderer startup.
/// @return Bool that indicates whether startup was successful or not.
bool InitializeTextRenderer();

/// @brief Shuts down the text renderer, should be called by the engine before renderer shutdown.
void ShutdownTextRenderer();

/// @brief Loads a font for rendering text with.
/// @param fontName What to name the font in the engine.
/// @param fontFileString String with the filepath to the font file.
void TextLoadFont(const char* fontName, const char* fontFileString);

void TextUnloadFont(const char* fontName); //TODO: 

Font* TextGetFont(const char* fontName);


TextBatch* TextBatchCreate(const char* fontName);
void TextBatchDestroy(TextBatch* textBatch);

/// @brief Adds text to a text batch
/// @param textBatch
/// @param text The text at the pointer will be copied, what happens to the text pointer after this function call doesn't matter
/// @param position
/// @return ID of the text added, used for removing specific texts from a text batch
u64 TextBatchAddText(TextBatch* textBatch, const char* text, vec2 position, f32 fontSize);

void TextBatchRemoveText(TextBatch* textBatch, u64 textId);

void TextBatchUpdateTextPosition(TextBatch* textBatch, u64 textId, vec2 newPosition);

void TextBatchSetTextActive(TextBatch* textBatch, u64 textId, bool active);

void TextBatchRender(TextBatch* textBatch, mat4 viewProjection);
