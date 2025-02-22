#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "renderer/renderer.h"

typedef struct GlyphInstanceData
{
    // Each pair of vec2's is sent to the gpu as a single vec4 to save on vertex attributes
    vec2 localPosition;         // .xy = local position
    vec2 localScale;            // .zw = local scale
    vec4 textureCoordinatePair; // .xy = texture coordinates  .zw = texture coordinates
} GlyphInstanceData;

DEFINE_DARRAY_TYPE(GlyphInstanceData);

#define MAX_RENDERABLE_CHARACTERS_PER_FONT 255

/// @brief Struct with all the data necessary to render text with a font.
typedef struct Font
{
    Texture glyphTextureAtlas;                                    // Texture handle
    vec4 textureCoordinates[MAX_RENDERABLE_CHARACTERS_PER_FONT];  // Coordinates of the glyph location in the texture atlas for each glyph
    vec2 glyphSizes[MAX_RENDERABLE_CHARACTERS_PER_FONT];          // Glyph size of each glyph, size is in em space. Glyph size is { 0, 0 } if it represents a character with no glyph, like space
    u32 renderableCharacters[MAX_RENDERABLE_CHARACTERS_PER_FONT]; // array of ascii values of all the renderable characters for this font
    f32 advanceWidths[MAX_RENDERABLE_CHARACTERS_PER_FONT];        // Advance width of each glyph
    f32 yOffsets[MAX_RENDERABLE_CHARACTERS_PER_FONT];             // Y offsets of each glyph
    f32 xPadding;                                                 // Padding in the x direction, used to position an entire text instance at the correct position
    f32 spaceAdvanceWidth;                                        // Advance width of the space character, also used to calculate the advance width of tab
    u32 characterCount;                                           // Amount of characters this font can render
    u32 refCount;                                                 // References to this font, only used to check if all text batches are deleted before this font is deleted, doesn't automatically delete this font
} Font;

typedef struct TextData
{
    char* string;                // The batch this textdata belongs to has ownership over this string
    vec2 position;               // Position
    u32 stringLength;            // String length, null terminator not included
    u32 firstGlyphInstanceIndex; // Index of the first glyph instance in the glyphInstanceData array of the batch this text belongs to
    u32 glyphInstanceCount;      // Amount of glyphs needed to render this text (this is different from string length because spaces don't have to be rendered but do add to the string length)
} TextData;

DEFINE_DARRAY_TYPE(TextData);
DEFINE_DARRAY_TYPE(u64);

typedef struct GlyphInstanceRange
{
	u64 startIndexInBytes;
	u64 instanceCount;
} GlyphInstanceRange;

typedef struct TextBatch
{
    GlyphInstanceDataDarray* glyphInstanceData; // CPU side data for the gpu glyph quad instancing buffer, needs to be kept on CPU because text might need to be changed
    VertexBuffer glyphInstancesBuffer;          // GPU side buffer for instancing quads with glyphs
    TextDataDarray* textDataArray;              // Darray of all text elements in this batch
    u64Darray* textIdArray;                     // Darray of ids of all text elements in this batch
    Font* font;                                 // Reference to the font used to render text in this batch
	GlyphInstanceRange* glyphInstanceRanges;	// Ranges of glyph instances to render
    Material textMaterial;                      // Reference to material used for rendering all the text in this batch
    u32 gpuBufferInstanceCapacity;              // TODO: remove once vertex buffer resizing is a thing
	u32 instanceRangeCount;						// amount of glyph instance ranges
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

void TextUnloadFont(const char* fontName);

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
