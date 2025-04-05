#include "text_renderer.h"
#include "msdf_helper_functions.h"

#include "containers/simplemap.h"
#include "math/lin_alg.h"
#include "renderer/texture.h"
#include "renderer/ui/font_loader.h"
#include <string.h>

// For determining the size of the allocations in the text pool allocator.
// It is asserted on text renderer startup that this is bigger than the text struct.
#define TEXT_STRING_ARENA_SIZE (KiB * 10)
#define TEXT_SHADER_NAME "sdf_glyph_shader"
#define RECT_VERTEX_COUNT 4
#define RECT_INDEX_COUNT 6
#define MAX_FONTMAP_ENTRIES 16
#define TAB_SIZE 4

typedef struct TextRendererState
{
	Allocator* textStringAllocator; // Freelist allocator for allocating text strings.
	SimpleMap* fontMap;             // Map with all the loaded fonts.
	VertexBuffer glyphRectVB;       // Vertex buffer for instanced glyph rendering.
	IndexBuffer glyphRectIB;        // Index buffer for instanced glyph rendering.
	u64 nextTextId;                 // Integer for giving each text object a unique id.
} TextRendererState;

static TextRendererState* state = nullptr;

static inline void MergeInstanceRanges(GlyphInstanceRange** pTextInstanceRanges, u32* instanceRangeCount);

bool InitializeTextRenderer()
{
	GRASSERT_DEBUG(state == nullptr); // If this fails init text renderer was called twice
	_INFO("Initializing text renderer subsystem...");

	// Creating the text renderer state struct and creating the basic data structures in it.
	state = Alloc(GetGlobalAllocator(), sizeof(*state));
	MemoryZero(state, sizeof(*state));

	state->fontMap = SimpleMapCreate(GetGlobalAllocator(), MAX_FONTMAP_ENTRIES);
	CreateFreelistAllocator("Text renderer text strings", GetGlobalAllocator(), TEXT_STRING_ARENA_SIZE, &state->textStringAllocator, true);
	state->nextTextId = 1;

	// Creating the char rect geometry for instancing characters
	struct CharRectVertex
	{
		vec2 position;
	};

	struct CharRectVertex charRectVertexData[RECT_VERTEX_COUNT] = {};
	charRectVertexData[0].position = vec2_create(0, 0);
	charRectVertexData[1].position = vec2_create(1, 0);
	charRectVertexData[2].position = vec2_create(0, 1);
	charRectVertexData[3].position = vec2_create(1, 1);

	u32 charRectIndexData[RECT_INDEX_COUNT] = { 0, 1, 2, 3, 2, 1 };

	state->glyphRectVB = VertexBufferCreate(charRectVertexData, sizeof(charRectVertexData));
	state->glyphRectIB = IndexBufferCreate(charRectIndexData, RECT_INDEX_COUNT);

	// Creating the bezier shader and material
	ShaderCreateInfo shaderCreateInfo = {};
	shaderCreateInfo.renderTargetStencil = false;
	shaderCreateInfo.renderTargetDepth = false;
	shaderCreateInfo.renderTargetColor = true;
	shaderCreateInfo.vertexShaderName = "text_sdf";
	shaderCreateInfo.fragmentShaderName = "text_sdf";
	shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 1;
	shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC2; // Quad 2d position
	shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 2;
	shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC4; // .xy = local position, .zw = local scale
	shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC4; // .xy = bottom left texture coord, .zw = top right texture coord

	ShaderCreate(TEXT_SHADER_NAME, &shaderCreateInfo);

	return true;
}

void ShutdownTextRenderer()
{
	DestroyFreelistAllocator(state->textStringAllocator);
	VertexBufferDestroy(state->glyphRectVB);
	IndexBufferDestroy(state->glyphRectIB);
	SimpleMapDestroy(state->fontMap);

	Free(GetGlobalAllocator(), state);
}

void TextLoadFont(const char* fontName, const char* fontFileString)
{
	// Loading glyph data
	GlyphData* glyphData = LoadFont(fontFileString);

	const char* renderableCharacters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ<>,./\\?|_-=+1234567890!@#$&*()~`:";
	u32 charCount = strlen(renderableCharacters);

	// Creating font struct
	Font* font = Alloc(GetGlobalAllocator(), sizeof(*font));
	MemoryZero(font, sizeof(*font));

	font->spaceAdvanceWidth = glyphData->advanceWidths[' '];
	font->refCount = 0;

	u32 glyphResolution = 32;
	u32 paddingPixels = 2;
	u32 emToPixels = glyphResolution - paddingPixels * 2;
	f32 pixelsToEm = 1.f / (f32)emToPixels;
	f32 paddingEm = pixelsToEm * (f32)paddingPixels;

	u32 textureAtlasGlyphsPerRow = (u32)ceilf(sqrtf((f32)charCount));

	font->xPadding = paddingEm;

	// Looping over the amount of renderable characters.
	// Getting the character at the index and storing it's advance width and size and calculating the size in pixels it takes up in the texture atlas.
	vec2i paddedPixelGlyphSizes[255] = {};
	for (int i = 0; i < charCount; i++)
	{
		u32 c = renderableCharacters[i]; // Current char value
		font->renderableCharacters[i] = c;

		font->advanceWidths[i] = glyphData->advanceWidths[c];
		font->glyphSizes[i] = glyphData->glyphSizes[c];

		paddedPixelGlyphSizes[i].x = glyphData->glyphSizes[c].x * emToPixels + paddingPixels * 2;
		paddedPixelGlyphSizes[i].y = glyphData->glyphSizes[c].y * emToPixels + paddingPixels * 2;
		font->glyphSizes[i].x = glyphData->glyphSizes[c].x + paddingEm * 2;
		font->glyphSizes[i].y = glyphData->glyphSizes[c].y + paddingEm * 2;
		font->yOffsets[i] = glyphData->glyphBottomLeftAnchor[c].y - paddingEm;
	}

	// Generating a packing for the texture atlas
	vec2i glyphAnchorPositions[255] = {};
	u32 binPackedHeight = Calculate2DBinPacking(glyphAnchorPositions, paddedPixelGlyphSizes, charCount, glyphResolution * textureAtlasGlyphsPerRow);

	u32 textureAtlasWidth = glyphResolution * textureAtlasGlyphsPerRow;
	u32 textureAtlasHeight = binPackedHeight;
	u8* texturePixelData = Alloc(GetGlobalAllocator(), sizeof(*texturePixelData) * TEXTURE_CHANNELS * textureAtlasWidth * textureAtlasHeight);
	MemoryZero(texturePixelData, sizeof(*texturePixelData) * TEXTURE_CHANNELS * textureAtlasWidth * textureAtlasHeight);
	for (u32 i = 0; i < textureAtlasHeight * textureAtlasWidth; i++)
		texturePixelData[i * TEXTURE_CHANNELS] = 255;

	f32 xAxisPixelToTextureCoord = 1.f / (f32)textureAtlasWidth;
	f32 yAxisPixelToTextureCoord = 1.f / (f32)textureAtlasHeight;

	// Generating the signed distance fields for the characters in the correct position in the texture atlas (in place)
	for (int i = 0; i < charCount; i++)
	{
		vec2i topRight = { glyphAnchorPositions[i].x + paddedPixelGlyphSizes[i].x, glyphAnchorPositions[i].y + paddedPixelGlyphSizes[i].y };
		CreateGlyphSDF(texturePixelData, TEXTURE_CHANNELS, textureAtlasWidth, textureAtlasHeight, font, glyphData, i, glyphAnchorPositions[i], topRight, paddingEm);

		// XY is bottom left texture coord, ZW is top right texture coord
		font->textureCoordinates[i].x = glyphAnchorPositions[i].x * xAxisPixelToTextureCoord;
		font->textureCoordinates[i].y = glyphAnchorPositions[i].y * yAxisPixelToTextureCoord;
		font->textureCoordinates[i].z = topRight.x * xAxisPixelToTextureCoord;
		font->textureCoordinates[i].w = topRight.y * yAxisPixelToTextureCoord;
	}

	font->characterCount = charCount;

	font->glyphTextureAtlas = TextureCreate(textureAtlasWidth, textureAtlasHeight, texturePixelData, TEXTURE_STORAGE_RGBA8UNORM);

	Free(GetGlobalAllocator(), texturePixelData);

	FreeGlyphData(glyphData);

	SimpleMapInsert(state->fontMap, fontName, font);
}

void TextUnloadFont(const char* fontName)
{
	Font* font = SimpleMapLookup(state->fontMap, fontName);
	GRASSERT_DEBUG(font->refCount == 0);
	TextureDestroy(font->glyphTextureAtlas);
	Free(GetGlobalAllocator(), font);
}

Font* TextGetFont(const char* fontName)
{
	return SimpleMapLookup(state->fontMap, fontName);
}

#define INITIAL_GPU_BUFFER_INSTANCE_CAPACITY 100
#define INITIAL_TEXT_BATCH_CAPACITY 10

TextBatch* TextBatchCreate(const char* fontName)
{
	TextBatch* textBatch = Alloc(GetGlobalAllocator(), sizeof(*textBatch));

	textBatch->font = SimpleMapLookup(state->fontMap, fontName);
	textBatch->font->refCount++;

	textBatch->glyphInstanceRanges = Alloc(GetGlobalAllocator(), sizeof(*textBatch->glyphInstanceRanges));
	textBatch->glyphInstanceRanges[0].startIndexInBytes = 0;
	textBatch->glyphInstanceRanges[0].instanceCount = 0;
	textBatch->instanceRangeCount = 1;

	textBatch->textIdArray = u64DarrayCreate(INITIAL_TEXT_BATCH_CAPACITY, GetGlobalAllocator());
	textBatch->textDataArray = TextDataDarrayCreate(INITIAL_TEXT_BATCH_CAPACITY, GetGlobalAllocator());
	textBatch->glyphInstanceData = GlyphInstanceDataDarrayCreate(INITIAL_GPU_BUFFER_INSTANCE_CAPACITY, GetGlobalAllocator());
	textBatch->gpuBufferInstanceCapacity = INITIAL_GPU_BUFFER_INSTANCE_CAPACITY;

	textBatch->glyphInstancesBuffer = VertexBufferCreate(textBatch->glyphInstanceData->data, sizeof(*textBatch->glyphInstanceData->data) * textBatch->gpuBufferInstanceCapacity);
	textBatch->textMaterial = MaterialCreate(ShaderGetRef(TEXT_SHADER_NAME));
	MaterialUpdateTexture(textBatch->textMaterial, "tex", textBatch->font->glyphTextureAtlas, SAMPLER_TYPE_LINEAR_CLAMP_EDGE);

	return textBatch;
}

void TextBatchDestroy(TextBatch* textBatch)
{
	textBatch->font->refCount--;

	// loop through all strings in the string darray and free them
	for (int i = 0; i < textBatch->textDataArray->size; i++)
	{
		Free(state->textStringAllocator, textBatch->textDataArray->data[i].string);
	}

	Free(GetGlobalAllocator(), textBatch->glyphInstanceRanges);
	DarrayDestroy(textBatch->textIdArray);
	DarrayDestroy(textBatch->textDataArray);
	DarrayDestroy(textBatch->glyphInstanceData);
	VertexBufferDestroy(textBatch->glyphInstancesBuffer);
	MaterialDestroy(textBatch->textMaterial);

	Free(GetGlobalAllocator(), textBatch);
}

u64 TextBatchAddText(TextBatch* textBatch, const char* text, vec2 position, f32 fontSize, bool variableText)
{
	// Storing the string and string length in the darrays for those, string length is stored without accounting for the null terminator but strings do have them.
	TextData textData = {};
	textData.stringLength = strlen(text);
	u32 stringLengthPlusNullTerminator = textData.stringLength + 1;
	textData.string = Alloc(state->textStringAllocator, sizeof(*text) * stringLengthPlusNullTerminator);
	MemoryCopy(textData.string, text, sizeof(*text) * stringLengthPlusNullTerminator);
	textData.firstGlyphInstanceIndex = textBatch->glyphInstanceData->size;
	textData.position = position;
	textData.fontSize = variableText ? fontSize : -1.f;	// If the text is not variable we don't need to store the font size, meaning we can set the font size to a value that indicates that the font size isn't variable

	// Looping through every char in the text and constructing the instance data for all the chars (position, scale, texture coords)
	vec2 nextGlyphPosition = position;
	nextGlyphPosition.x -= textBatch->font->xPadding * fontSize;
	for (int i = 0; i < textData.stringLength; i++)
	{
		// If the glyph is a tab, dont add it to the glyph instance array, or put a placeholder if the text is variable
		if (text[i] == '\t')
		{
			if (variableText)
			{
				GlyphInstanceData glyphInstance = {};
				glyphInstance.localPosition = nextGlyphPosition;
				glyphInstance.localScale = vec2_create(0, 0);
				glyphInstance.textureCoordinatePair = vec4_create(1, 1, 1, 1);

				GlyphInstanceDataDarrayPushback(textBatch->glyphInstanceData, &glyphInstance);
			}
			nextGlyphPosition.x += textBatch->font->spaceAdvanceWidth * TAB_SIZE * fontSize;
			continue;
		}

		// If the glyph is a space, dont add it to the glyph instance array, or put a placeholder if the text is variable
		if (text[i] == ' ')
		{
			if (variableText)
			{
				GlyphInstanceData glyphInstance = {};
				glyphInstance.localPosition = nextGlyphPosition;
				glyphInstance.localScale = vec2_create(0, 0);
				glyphInstance.textureCoordinatePair = vec4_create(1, 1, 1, 1);

				GlyphInstanceDataDarrayPushback(textBatch->glyphInstanceData, &glyphInstance);
			}
			nextGlyphPosition.x += textBatch->font->spaceAdvanceWidth * fontSize;
			continue;
		}

		// Finding the index for the glyph to get it's data from the font
		u32 glyphIndex = UINT32_MAX;
		for (int j = 0; j < textBatch->font->characterCount; j++)
		{
			if (text[i] == textBatch->font->renderableCharacters[j])
			{
				glyphIndex = j;
				break;
			}
		}

		GRASSERT_DEBUG(glyphIndex != UINT32_MAX);

		// If the glyph size is 0 by 0, don't add it to the glyph instance array, only add its advance width, or put a placeholder if the text is variable
		if (0 == (textBatch->font->glyphSizes[glyphIndex].x + textBatch->font->glyphSizes[glyphIndex].y))
		{
			if (variableText)
			{
				GlyphInstanceData glyphInstance = {};
				glyphInstance.localPosition = nextGlyphPosition;
				glyphInstance.localScale = vec2_create(0, 0);
				glyphInstance.textureCoordinatePair = vec4_create(1, 1, 1, 1);

				GlyphInstanceDataDarrayPushback(textBatch->glyphInstanceData, &glyphInstance);
			}
			nextGlyphPosition.x += textBatch->font->advanceWidths[glyphIndex];
			continue;
		}

		GlyphInstanceData glyphInstance = {};
		glyphInstance.localPosition = nextGlyphPosition;
		glyphInstance.localPosition.y += textBatch->font->yOffsets[glyphIndex] * fontSize;
		glyphInstance.localScale = vec2_mul_f32(textBatch->font->glyphSizes[glyphIndex], fontSize);
		glyphInstance.textureCoordinatePair = textBatch->font->textureCoordinates[glyphIndex];

		GlyphInstanceDataDarrayPushback(textBatch->glyphInstanceData, &glyphInstance);

		nextGlyphPosition.x += textBatch->font->advanceWidths[glyphIndex] * fontSize;
	}

	textData.glyphInstanceCount = textBatch->glyphInstanceData->size - textData.firstGlyphInstanceIndex;
	TextDataDarrayPushback(textBatch->textDataArray, &textData);

	// If the gpu vertex buffer is too small, recreate it and upload the buffer again
	if (textBatch->glyphInstanceData->size > textBatch->gpuBufferInstanceCapacity)
	{
		VertexBufferDestroy(textBatch->glyphInstancesBuffer);
		textBatch->gpuBufferInstanceCapacity = textBatch->glyphInstanceData->capacity;
		textBatch->glyphInstancesBuffer = VertexBufferCreate(textBatch->glyphInstanceData->data, sizeof(*textBatch->glyphInstanceData->data) * textBatch->gpuBufferInstanceCapacity);
	}
	else // If the gpu vertex buffer has space, upload the new data TODO: allow uploading only to a range rather than from 0 to a given point
	{
		VertexBufferUpdate(textBatch->glyphInstancesBuffer, textBatch->glyphInstanceData->data, sizeof(*textBatch->glyphInstanceData->data) * textBatch->gpuBufferInstanceCapacity);
	}

	// Putting the text in the instance range it belongs so its instances get rendered.
	// It either gets put in the last instance range, or in a new instance range appended to the end
	// if the last instance range doesn't continue up to the instance just before the first instance of this text.
	GlyphInstanceRange* lastInstanceRange = &textBatch->glyphInstanceRanges[textBatch->instanceRangeCount - 1];
	if ((lastInstanceRange->startIndexInBytes / sizeof(GlyphInstanceData)) + lastInstanceRange->instanceCount == textData.firstGlyphInstanceIndex)
	{
		lastInstanceRange->instanceCount += textData.glyphInstanceCount;
	}
	else
	{
		textBatch->instanceRangeCount++;
		textBatch->glyphInstanceRanges = Realloc(GetGlobalAllocator(), textBatch->glyphInstanceRanges, sizeof(*textBatch->glyphInstanceRanges) * textBatch->instanceRangeCount);
		textBatch->glyphInstanceRanges[textBatch->instanceRangeCount - 1].startIndexInBytes = textData.firstGlyphInstanceIndex * sizeof(GlyphInstanceData);
		textBatch->glyphInstanceRanges[textBatch->instanceRangeCount - 1].instanceCount = textData.glyphInstanceCount;
	}

	// Giving the text an id and returning its id
	u64DarrayPushback(textBatch->textIdArray, &state->nextTextId);
	state->nextTextId++;
	return state->nextTextId - 1;
}

u64 TextBatchAddTextMaxWidth(TextBatch* textBatch, const char* text, vec2 position, f32 fontSize, f32 maxLineWidth, f32* out_height)
{
	// Storing the string and string length in the darrays for those, string length is stored without accounting for the null terminator but strings do have them.
	TextData textData = {};
	textData.stringLength = strlen(text);
	u32 stringLengthPlusNullTerminator = textData.stringLength + 1;
	textData.string = Alloc(state->textStringAllocator, sizeof(*text) * stringLengthPlusNullTerminator);
	MemoryCopy(textData.string, text, sizeof(*text) * stringLengthPlusNullTerminator);
	textData.firstGlyphInstanceIndex = textBatch->glyphInstanceData->size;
	textData.position = position;
	textData.fontSize = -1.f;	// Indicating that text isn't variable

	// Looping through every char in the text and constructing the instance data for all the chars (position, scale, texture coords)
	vec2 nextGlyphPosition = position;
	nextGlyphPosition.x -= textBatch->font->xPadding * fontSize;
	u32 lastSpaceIndex = 0;
	u32 lastWordStartGlyphInstanceDataIndex = 0;
	u32 rollbackCounter = 0;
	*out_height = fontSize;
	for (int i = 0; i < textData.stringLength; i++)
	{
		// If the glyph is a tab, dont add it to the glyph instance array
		if (text[i] == '\t')
		{
			lastSpaceIndex = i;
			lastWordStartGlyphInstanceDataIndex = textBatch->glyphInstanceData->size;
			nextGlyphPosition.x += textBatch->font->spaceAdvanceWidth * TAB_SIZE * fontSize;
			continue;
		}

		// If the glyph is a space, dont add it to the glyph instance array
		if (text[i] == ' ')
		{
			lastSpaceIndex = i;
			lastWordStartGlyphInstanceDataIndex = textBatch->glyphInstanceData->size;
			nextGlyphPosition.x += textBatch->font->spaceAdvanceWidth * fontSize;
			continue;
		}

		// Finding the index for the glyph to get it's data from the font
		u32 glyphIndex = UINT32_MAX;
		for (int j = 0; j < textBatch->font->characterCount; j++)
		{
			if (text[i] == textBatch->font->renderableCharacters[j])
			{
				glyphIndex = j;
				break;
			}
		}

		GRASSERT_DEBUG(glyphIndex != UINT32_MAX);

		// If the glyph size is 0 by 0, don't add it to the glyph instance array, only add its advance width
		if (0 == (textBatch->font->glyphSizes[glyphIndex].x + textBatch->font->glyphSizes[glyphIndex].y))
		{
			nextGlyphPosition.x += textBatch->font->advanceWidths[glyphIndex];
			continue;
		}

		GlyphInstanceData glyphInstance = {};
		glyphInstance.localPosition = nextGlyphPosition;
		glyphInstance.localPosition.y += textBatch->font->yOffsets[glyphIndex] * fontSize;
		glyphInstance.localScale = vec2_mul_f32(textBatch->font->glyphSizes[glyphIndex], fontSize);
		glyphInstance.textureCoordinatePair = textBatch->font->textureCoordinates[glyphIndex];

		GlyphInstanceDataDarrayPushback(textBatch->glyphInstanceData, &glyphInstance);

		nextGlyphPosition.x += textBatch->font->advanceWidths[glyphIndex] * fontSize;

		// If the line is exceeded, rollback to the start of the previous word and put it on the next line
		if (nextGlyphPosition.x - (position.x - textBatch->font->xPadding * fontSize) > maxLineWidth)
		{
			nextGlyphPosition.x = (position.x - textBatch->font->xPadding * fontSize);
			nextGlyphPosition.y -= fontSize * 1.25;// vertical spacing factor
			*out_height += fontSize * 1.25;
			rollbackCounter++;
			GRASSERT(rollbackCounter < 1000);// This catches an infinite loop in case a single word is larger than a line

			i = lastSpaceIndex;
			DarrayPopRange(textBatch->glyphInstanceData, lastWordStartGlyphInstanceDataIndex, textBatch->glyphInstanceData->size - lastWordStartGlyphInstanceDataIndex);
		}
	}

	textData.glyphInstanceCount = textBatch->glyphInstanceData->size - textData.firstGlyphInstanceIndex;
	TextDataDarrayPushback(textBatch->textDataArray, &textData);

	// If the gpu vertex buffer is too small, recreate it and upload the buffer again
	if (textBatch->glyphInstanceData->size > textBatch->gpuBufferInstanceCapacity)
	{
		VertexBufferDestroy(textBatch->glyphInstancesBuffer);
		textBatch->gpuBufferInstanceCapacity = textBatch->glyphInstanceData->capacity;
		textBatch->glyphInstancesBuffer = VertexBufferCreate(textBatch->glyphInstanceData->data, sizeof(*textBatch->glyphInstanceData->data) * textBatch->gpuBufferInstanceCapacity);
	}
	else // If the gpu vertex buffer has space, upload the new data TODO: allow uploading only to a range rather than from 0 to a given point
	{
		VertexBufferUpdate(textBatch->glyphInstancesBuffer, textBatch->glyphInstanceData->data, sizeof(*textBatch->glyphInstanceData->data) * textBatch->gpuBufferInstanceCapacity);
	}

	// Putting the text in the instance range it belongs so its instances get rendered.
	// It either gets put in the last instance range, or in a new instance range appended to the end
	// if the last instance range doesn't continue up to the instance just before the first instance of this text.
	GlyphInstanceRange* lastInstanceRange = &textBatch->glyphInstanceRanges[textBatch->instanceRangeCount - 1];
	if ((lastInstanceRange->startIndexInBytes / sizeof(GlyphInstanceData)) + lastInstanceRange->instanceCount == textData.firstGlyphInstanceIndex)
	{
		lastInstanceRange->instanceCount += textData.glyphInstanceCount;
	}
	else
	{
		textBatch->instanceRangeCount++;
		textBatch->glyphInstanceRanges = Realloc(GetGlobalAllocator(), textBatch->glyphInstanceRanges, sizeof(*textBatch->glyphInstanceRanges) * textBatch->instanceRangeCount);
		textBatch->glyphInstanceRanges[textBatch->instanceRangeCount - 1].startIndexInBytes = textData.firstGlyphInstanceIndex * sizeof(GlyphInstanceData);
		textBatch->glyphInstanceRanges[textBatch->instanceRangeCount - 1].instanceCount = textData.glyphInstanceCount;
	}

	// Giving the text an id and returning its id
	u64DarrayPushback(textBatch->textIdArray, &state->nextTextId);
	state->nextTextId++;
	return state->nextTextId - 1;
}

void TextBatchRemoveText(TextBatch* textBatch, u64 textId)
{
	// Getting the index of the text that corresponds to the given text id
	u32 textIndex = UINT32_MAX;
	for (u32 i = 0; i < textBatch->textIdArray->size; i++)
	{
		if (textBatch->textIdArray->data[i] == textId)
		{
			textIndex = i;
			break;
		}
	}

	TextData* textData = &textBatch->textDataArray->data[textIndex];
	GlyphInstanceRange* glyphInstanceRanges = textBatch->glyphInstanceRanges;

	// Removing the glyph instances from the instance range they were in and updating the ranges that come after that, if they were active at all
	for (u32 i = 0; i < textBatch->instanceRangeCount; i++)
	{
		u64 rangeStartInstanceIndex = glyphInstanceRanges[i].startIndexInBytes / sizeof(GlyphInstanceData);
		// If the text data is in this range or a range after it
		if (textData->firstGlyphInstanceIndex >= rangeStartInstanceIndex)
		{
			// If the textData spans the entire range, remove the range
			if (textData->firstGlyphInstanceIndex == rangeStartInstanceIndex && textData->glyphInstanceCount == glyphInstanceRanges[i].instanceCount)
			{
				size_t copySize = (textBatch->instanceRangeCount - (i + 1)) * sizeof(*glyphInstanceRanges);
				if (copySize != 0)
				{
					MemoryCopy(&glyphInstanceRanges[i], &glyphInstanceRanges[i + 1], copySize);
				}
				textBatch->instanceRangeCount--;
				textBatch->glyphInstanceRanges = Realloc(GetGlobalAllocator(), textBatch->glyphInstanceRanges, sizeof(*textBatch->glyphInstanceRanges) * textBatch->instanceRangeCount);
				i--;
			}
			else if (textData->firstGlyphInstanceIndex < rangeStartInstanceIndex + glyphInstanceRanges[i].instanceCount) // If the textData is in the range, lower the count of the range
			{
				glyphInstanceRanges[i].instanceCount -= textData->glyphInstanceCount;
			}
		}
		else // if the text data was in a range before this,
		{
			glyphInstanceRanges[i].startIndexInBytes -= textData->glyphInstanceCount * sizeof(GlyphInstanceData);
		}
	}

	MergeInstanceRanges(&textBatch->glyphInstanceRanges, &textBatch->instanceRangeCount);

	// Removing the texts' glyph instances from the instances buffer
	DarrayPopRange(textBatch->glyphInstanceData, textData->firstGlyphInstanceIndex, textData->glyphInstanceCount);
	VertexBufferUpdate(textBatch->glyphInstancesBuffer, textBatch->glyphInstanceData->data, sizeof(*textBatch->glyphInstanceData->data) * textBatch->gpuBufferInstanceCapacity); // TODO: allow uploading only to a range rather than from 0 to a given point

	// Updating the firstGlyphInstanceIndex for all the texts after the one thats getting deleted
	u32 currentFirstInstanceIndex = textData->firstGlyphInstanceIndex;
	for (u32 i = textIndex + 1; i < textBatch->textDataArray->size; i++)
	{
		textBatch->textDataArray->data[i].firstGlyphInstanceIndex = currentFirstInstanceIndex;
		currentFirstInstanceIndex += textBatch->textDataArray->data[i].glyphInstanceCount;
	}

	// Freeing the string and removing the text from text data and text id arrays
	Free(state->textStringAllocator, textData->string);
	DarrayPopAt(textBatch->textDataArray, textIndex);
	DarrayPopAt(textBatch->textIdArray, textIndex);
}

void TextBatchUpdateTextPosition(TextBatch* textBatch, u64 textId, vec2 newPosition)
{
	// Getting the index of the text that corresponds to the given text id
	u32 textIndex = UINT32_MAX;
	for (u32 i = 0; i < textBatch->textIdArray->size; i++)
	{
		if (textBatch->textIdArray->data[i] == textId)
		{
			textIndex = i;
			break;
		}
	}

	vec2 positionDelta = vec2_sub_vec2(newPosition, textBatch->textDataArray->data[textIndex].position);
	for (int i = textBatch->textDataArray->data[textIndex].firstGlyphInstanceIndex; i < textBatch->textDataArray->data[textIndex].firstGlyphInstanceIndex + textBatch->textDataArray->data[textIndex].glyphInstanceCount; i++)
	{
		textBatch->glyphInstanceData->data[i].localPosition = vec2_add_vec2(textBatch->glyphInstanceData->data[i].localPosition, positionDelta);
	}
	VertexBufferUpdate(textBatch->glyphInstancesBuffer, textBatch->glyphInstanceData->data, sizeof(*textBatch->glyphInstanceData->data) * textBatch->gpuBufferInstanceCapacity); // TODO: allow uploading only to a range rather than from 0 to a given point
}

void TextBatchUpdateTextString(TextBatch* textBatch, u64 textId, const char* newText)
{
	// Getting the index of the text that corresponds to the given text id
	u32 textIndex = UINT32_MAX;
	for (u32 i = 0; i < textBatch->textIdArray->size; i++)
	{
		if (textBatch->textIdArray->data[i] == textId)
		{
			textIndex = i;
			break;
		}
	}

	TextData* textData = &textBatch->textDataArray->data[textIndex];

	GRASSERT_DEBUG(textData->fontSize >= 0);
	GRASSERT(textData->stringLength == strlen(newText));

	MemoryCopy(textData->string, newText, textData->stringLength + 1);

	vec2 nextGlyphPosition = textData->position;
	nextGlyphPosition.x -= textBatch->font->xPadding * textData->fontSize;
	for (u32 i = 0; i < textData->glyphInstanceCount; i++)
	{
		// If the glyph is a tab, dont add it to the glyph instance array, or put a placeholder if the text is variable
		if (newText[i] == '\t')
		{
			GlyphInstanceData glyphInstance = {};
			glyphInstance.localPosition = nextGlyphPosition;
			glyphInstance.localScale = vec2_create(0, 0);
			glyphInstance.textureCoordinatePair = vec4_create(1, 1, 1, 1);

			textBatch->glyphInstanceData->data[textData->firstGlyphInstanceIndex + i] = glyphInstance;
			nextGlyphPosition.x += textBatch->font->spaceAdvanceWidth * TAB_SIZE * textData->fontSize;
			continue;
		}

		// If the glyph is a space, dont add it to the glyph instance array, or put a placeholder if the text is variable
		if (newText[i] == ' ')
		{
			GlyphInstanceData glyphInstance = {};
			glyphInstance.localPosition = nextGlyphPosition;
			glyphInstance.localScale = vec2_create(0, 0);
			glyphInstance.textureCoordinatePair = vec4_create(1, 1, 1, 1);

			textBatch->glyphInstanceData->data[textData->firstGlyphInstanceIndex + i] = glyphInstance;
			nextGlyphPosition.x += textBatch->font->spaceAdvanceWidth * TAB_SIZE * textData->fontSize;
			continue;
		}

		// Finding the index for the glyph to get it's data from the font
		u32 glyphIndex = UINT32_MAX;
		for (int j = 0; j < textBatch->font->characterCount; j++)
		{
			if (newText[i] == textBatch->font->renderableCharacters[j])
			{
				glyphIndex = j;
				break;
			}
		}

		GRASSERT_DEBUG(glyphIndex != UINT32_MAX);

		// If the glyph size is 0 by 0, don't add it to the glyph instance array, only add its advance width, or put a placeholder if the text is variable
		if (0 == (textBatch->font->glyphSizes[glyphIndex].x + textBatch->font->glyphSizes[glyphIndex].y))
		{
			GlyphInstanceData glyphInstance = {};
			glyphInstance.localPosition = nextGlyphPosition;
			glyphInstance.localScale = vec2_create(0, 0);
			glyphInstance.textureCoordinatePair = vec4_create(1, 1, 1, 1);

			textBatch->glyphInstanceData->data[textData->firstGlyphInstanceIndex + i] = glyphInstance;
			nextGlyphPosition.x += textBatch->font->spaceAdvanceWidth * TAB_SIZE * textData->fontSize;
			continue;
		}

		GlyphInstanceData glyphInstance = {};
		glyphInstance.localPosition = nextGlyphPosition;
		glyphInstance.localPosition.y += textBatch->font->yOffsets[glyphIndex] * textData->fontSize;
		glyphInstance.localScale = vec2_mul_f32(textBatch->font->glyphSizes[glyphIndex], textData->fontSize);
		glyphInstance.textureCoordinatePair = textBatch->font->textureCoordinates[glyphIndex];

		textBatch->glyphInstanceData->data[textData->firstGlyphInstanceIndex + i] = glyphInstance;

		nextGlyphPosition.x += textBatch->font->advanceWidths[glyphIndex] * textData->fontSize;
	}

	VertexBufferUpdate(textBatch->glyphInstancesBuffer, textBatch->glyphInstanceData->data, sizeof(*textBatch->glyphInstanceData->data) * textBatch->gpuBufferInstanceCapacity); // TODO: allow uploading only to a range rather than from 0 to a given point
}

void TextBatchSetTextActive(TextBatch* textBatch, u64 textId, bool active)
{
	// Getting the index of the text that corresponds to the given text id
	u32 textIndex = UINT32_MAX;
	for (u32 i = 0; i < textBatch->textIdArray->size; i++)
	{
		if (textBatch->textIdArray->data[i] == textId)
		{
			textIndex = i;
			break;
		}
	}

	TextData* textData = &textBatch->textDataArray->data[textIndex];

	if (active) // If the text is set to active
	{
		for (u32 i = 0; i < textBatch->instanceRangeCount; i++)
		{
			u64 rangeStartInstanceIndex = textBatch->glyphInstanceRanges[i].startIndexInBytes / sizeof(GlyphInstanceData);
			if (textData->firstGlyphInstanceIndex >= rangeStartInstanceIndex)
			{
				// Making sure the text is not already active
				GRASSERT_DEBUG(textData->firstGlyphInstanceIndex + textData->glyphInstanceCount > rangeStartInstanceIndex + textBatch->glyphInstanceRanges[i].instanceCount);

				textBatch->instanceRangeCount++;
				textBatch->glyphInstanceRanges = Realloc(GetGlobalAllocator(), textBatch->glyphInstanceRanges, sizeof(*textBatch->glyphInstanceRanges) * textBatch->instanceRangeCount);
				size_t copySize = (textBatch->instanceRangeCount - (i + 2)) * sizeof(*textBatch->glyphInstanceRanges);
				if (copySize != 0)
				{
					MemoryCopy(&textBatch->glyphInstanceRanges[i + 2], &textBatch->glyphInstanceRanges[i + 1], copySize);
				}
				textBatch->glyphInstanceRanges[i + 1].instanceCount = textData->glyphInstanceCount;
				textBatch->glyphInstanceRanges[i + 1].startIndexInBytes = textData->firstGlyphInstanceIndex * sizeof(GlyphInstanceData);
				break;
			}
		}
	}
	else // If the text is set to inactive
	{
		for (u32 i = 0; i < textBatch->instanceRangeCount; i++)
		{
			u64 rangeStartInstanceIndex = textBatch->glyphInstanceRanges[i].startIndexInBytes / sizeof(GlyphInstanceData);
			// If this is the glyph instance range this text resides in
			if (textData->firstGlyphInstanceIndex >= rangeStartInstanceIndex)
			{
				// assertion that checks that this text is not already disabled
				GRASSERT_DEBUG(textData->firstGlyphInstanceIndex + textData->glyphInstanceCount <= rangeStartInstanceIndex + textBatch->glyphInstanceRanges[i].instanceCount);
				// If this text is at the start of the range, increase the range start instance index by the instance count of the text, and decrease the instance count of the range correspondingly
				if (textData->firstGlyphInstanceIndex == rangeStartInstanceIndex)
				{
					textBatch->glyphInstanceRanges[i].startIndexInBytes += textData->glyphInstanceCount * sizeof(GlyphInstanceData);
					textBatch->glyphInstanceRanges[i].instanceCount -= textData->glyphInstanceCount;
				}
				// If the text is at the end of the range, reduce the instance count of the range by the instance count of the text
				else if (textData->firstGlyphInstanceIndex + textData->glyphInstanceCount == rangeStartInstanceIndex + textBatch->glyphInstanceRanges[i].instanceCount)
				{
					textBatch->glyphInstanceRanges[i].instanceCount -= textData->glyphInstanceCount;
				}
				// If the text is somewhere in the middle of the range, split the range into two
				else
				{
					textBatch->instanceRangeCount++;
					textBatch->glyphInstanceRanges = Realloc(GetGlobalAllocator(), textBatch->glyphInstanceRanges, sizeof(*textBatch->glyphInstanceRanges) * textBatch->instanceRangeCount);
					size_t copySize = (textBatch->instanceRangeCount - (i + 2)) * sizeof(*textBatch->glyphInstanceRanges);
					if (copySize != 0)
					{
						MemoryCopy(&textBatch->glyphInstanceRanges[i + 2], &textBatch->glyphInstanceRanges[i + 1], copySize);
					}
					u64 newInstanceCount = textData->firstGlyphInstanceIndex - rangeStartInstanceIndex;
					textBatch->glyphInstanceRanges[i + 1].startIndexInBytes = (textData->firstGlyphInstanceIndex + textData->glyphInstanceCount) * sizeof(GlyphInstanceData);
					textBatch->glyphInstanceRanges[i + 1].instanceCount = textBatch->glyphInstanceRanges[i].instanceCount - (newInstanceCount + textData->glyphInstanceCount);
					textBatch->glyphInstanceRanges[i].instanceCount = newInstanceCount;
				}
				break;
			}
		}
	}

	MergeInstanceRanges(&textBatch->glyphInstanceRanges, &textBatch->instanceRangeCount);
}

void TextBatchRender(TextBatch* textBatch, mat4 viewProjection)
{
	MaterialBind(textBatch->textMaterial);
	VertexBuffer VBs[2] = { state->glyphRectVB, textBatch->glyphInstancesBuffer };

	for (u32 i = 0; i < textBatch->instanceRangeCount; i++)
	{
		u64 vbOffsets[2] = { 0, 0 };
		vbOffsets[1] = textBatch->glyphInstanceRanges[i].startIndexInBytes;
		DrawBufferRange(2, VBs, vbOffsets, state->glyphRectIB, &viewProjection, textBatch->glyphInstanceRanges[i].instanceCount);
	}
}

static inline void MergeInstanceRanges(GlyphInstanceRange** pGlyphInstanceRanges, u32* instanceRangeCount)
{
	if (*instanceRangeCount == 0)
		return;

	GlyphInstanceRange* glyphInstanceRanges = *pGlyphInstanceRanges;
	for (u32 i = 0; i < *instanceRangeCount - 1; i++)
	{
		u64 rangeStartInstanceIndex = glyphInstanceRanges[i].startIndexInBytes / sizeof(GlyphInstanceData);
		if (rangeStartInstanceIndex + glyphInstanceRanges[i].instanceCount == (glyphInstanceRanges[i + 1].startIndexInBytes / sizeof(GlyphInstanceData)))
		{
			glyphInstanceRanges[i].instanceCount += glyphInstanceRanges[i + 1].instanceCount;
			size_t copySize = (*instanceRangeCount - (i + 2)) * sizeof(*glyphInstanceRanges);
			if (copySize != 0)
			{
				MemoryCopy(&glyphInstanceRanges[i + 1], &glyphInstanceRanges[i + 2], copySize);
			}
			*instanceRangeCount -= 1;
			*pGlyphInstanceRanges = Realloc(GetGlobalAllocator(), *pGlyphInstanceRanges, sizeof(**pGlyphInstanceRanges) * *instanceRangeCount);
			glyphInstanceRanges = *pGlyphInstanceRanges;
			i--;
		}
	}
}
