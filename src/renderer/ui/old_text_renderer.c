
/*
#include "text_renderer.h"

#include "containers/simplemap.h"
#include "math/lin_alg.h"
#include "renderer/ui/font_loader.h"
#include <string.h>

// For determining the size of the allocations in the text pool allocator.
// It is asserted on text renderer startup that this is bigger than the text struct.
#define TEXT_BLOCK_SIZE 128
#define MAX_TEXT_OBJECTS 500
#define TEXT_STRING_ARENA_SIZE (KiB * 10)
#define MAX_BEZIER_INSTANCE_COUNT 20000
#define BEZIER_SHADER_NAME "bezier"

typedef struct Text
{
    mat4 transform;
    char* string;                    // TODO: remove if not used
    VertexBuffer instanceVB;         //
    UpdateFrequency updateFrequency; // TODO: use this
    u32 bezierInstanceCount;         //
    u32 id;                          // TODO: remove if not used
    bool enabled;
} Text;

/// @brief Struct of the per instance data for the bezier shader
typedef struct BezierInstance
{
    vec4 beginEndPoints;
    vec2 midPoint;
} BezierInstance;

/// @brief Struct with all the data necessary to render text with a font.
typedef struct Font
{
    GlyphData* glyphData;                  //
    BezierInstance* characterBeziers[255]; // Array of arrays with bezier curve data for each of the glyphs.
    u32 characterBezierCounts[255];        // Array of amount of bezier's for each character.
} Font;

DEFINE_DARRAY_TYPE_REF(Text);

typedef struct TextRendererState
{
    Allocator* textPool;            // Pool allocator for allocating text structs.
    Allocator* textStringAllocator; // Freelist allocator for allocating text strings.
    Material bezierMaterial;        // Material for instanced bezier rendering.
    VertexBuffer bezierVB;          // Vertex buffer for instanced bezier rendering.
    IndexBuffer bezierIB;           // Index buffer for instanced bezier rendering.
    SimpleMap* fontMap;             // Map with all the loaded fonts.
    TextRefDarray* textDarray;              // Darray with pointers to all text objects.
    u32 nextTextId;                 // Integer for giving each text object a unique id.
} TextRendererState;

static TextRendererState* state = nullptr;

bool InitializeTextRenderer()
{
    GRASSERT_DEBUG(state == nullptr); // If this fails init text renderer was called twice
    GRASSERT_DEBUG(TEXT_BLOCK_SIZE >= sizeof(Text));
    _INFO("Initializing text renderer subsystem...");

    // Creating the text renderer state struct and creating the basic data structures in it.
    state = Alloc(GetGlobalAllocator(), sizeof(*state), MEM_TAG_RENDERER_SUBSYS);
    MemoryZero(state, sizeof(*state));

    state->fontMap = SimpleMapCreate(GetGlobalAllocator(), 16);
    state->textDarray = TextRefDarrayCreate(MAX_TEXT_OBJECTS, GetGlobalAllocator());
    CreatePoolAllocator("text renderer text pool", GetGlobalAllocator(), TEXT_BLOCK_SIZE, MAX_TEXT_OBJECTS, &state->textPool);
    CreateFreelistAllocator("Text renderer text strings", GetGlobalAllocator(), TEXT_STRING_ARENA_SIZE, &state->textStringAllocator);
    state->nextTextId = 1;

    // Creating the bezier shader and material
    ShaderCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.renderTargetStencil = false;
    shaderCreateInfo.renderTargetDepth = true;
    shaderCreateInfo.renderTargetColor = true;
    shaderCreateInfo.vertexShaderName = "bezier";
    shaderCreateInfo.fragmentShaderName = "bezier";
    shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 1;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC2;
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 2;
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC4;
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC2;

    ShaderCreate("bezier", &shaderCreateInfo);
    state->bezierMaterial = MaterialCreate(ShaderGetRef("bezier"));

    // Generating the bezier strip mesh
    // The vertices in the strip are just vec2's, the x component indicates how far along in the bezier line the vertex is (between 0 and 1) and the y
    // component indicates on which side of the strip the vertex sits (1 is above, -1 is below).
    const int bezierResolution = 10;
    vec2 bezierVBData[bezierResolution * 2] = {};
    u32 bezierIBData[(bezierResolution - 1) * 2 * 3]; // bezierresolution -1 is amount of quads x2 is amount of tri's x3 is amount of indices

    for (int i = 0; i < bezierResolution; i++)
    {
        f32 t = (f32)i / (f32)(bezierResolution - 1);

        i32 vbIndex = i * 2;
        bezierVBData[vbIndex] = vec2_create(t, 1);
        bezierVBData[vbIndex + 1] = vec2_create(t, -1);

        if (i != bezierResolution - 1)
        {
            i32 ibIndex = i * 6;
            bezierIBData[ibIndex + 0] = i * 2;
            bezierIBData[ibIndex + 1] = (i * 2) + 1;
            bezierIBData[ibIndex + 2] = (i * 2) + 2;
            bezierIBData[ibIndex + 3] = (i * 2) + 1;
            bezierIBData[ibIndex + 4] = (i * 2) + 3;
            bezierIBData[ibIndex + 5] = (i * 2) + 2;
        }
    }

    state->bezierVB = VertexBufferCreate(bezierVBData, sizeof(vec2) * bezierResolution * 2);
    state->bezierIB = IndexBufferCreate(bezierIBData, (bezierResolution - 1) * 2 * 3);

    return true;
}

void ShutdownTextRenderer()
{
    DestroyPoolAllocator(state->textPool);
    DestroyFreelistAllocator(state->textStringAllocator);
    MaterialDestroy(state->bezierMaterial);
    SimpleMapDestroy(state->fontMap);
    DarrayDestroy(state->textDarray);

    Free(GetGlobalAllocator(), state);
}

void TextLoadFont(const char* fontName, const char* fontFileString)
{
    // Creating font struct
    Font* font = Alloc(GetGlobalAllocator(), sizeof(*font), MEM_TAG_RENDERER_SUBSYS);
    MemoryZero(font, sizeof(*font));

    // Loading glyph data
    GlyphData* glyphData = LoadFont(fontFileString);
    font->glyphData = glyphData;

    // Generating bezier data for all renderable characters
    const char* renderableCharacters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ<>,./\\?|_-=+1234567890!@#$&*()~`";
    u32 charCount = strlen(renderableCharacters);

    // Looping over the amount of renderable characters.
    // Getting the character at the index and making an array of its bezier curves.
    for (int i = 0; i < charCount; i++)
    {
        BezierInstance instanceData[MAX_BEZIER_INSTANCE_COUNT] = {};
        u32 bezierIndex = 0;
        u32 c = renderableCharacters[i]; // Current char value

        for (int contour = 0; contour < glyphData->contourCounts[c]; contour++)
        {
            u32 previousContourEnd;
            if (contour == 0)
                previousContourEnd = -1; // -1 because 0 is the start of the current contour
            else
                previousContourEnd = glyphData->endPointsOfContours[c][contour - 1];
            u32 currentContourStart = previousContourEnd + 1;
            u32 contourPointCount = glyphData->endPointsOfContours[c][contour] - previousContourEnd;

            // If the first point is on curve then the first off curve point is point 1 and otherwise point 0 is the first off curve point
            i32 firstOffCurvePoint = glyphData->firstPointOnCurve[c][contour] ? 1 : 0;

            for (int point = firstOffCurvePoint; point < contourPointCount; point += 2)
            {
                int previousPointIndex = ((point + contourPointCount - 1) % contourPointCount) + currentContourStart;
                int currentPoint = point + currentContourStart;
                int nextPointIndex = ((point + 1) % contourPointCount) + currentContourStart;

                instanceData[bezierIndex].beginEndPoints.x = glyphData->pointArrays[c][previousPointIndex].x;
                instanceData[bezierIndex].beginEndPoints.y = glyphData->pointArrays[c][previousPointIndex].y;
                instanceData[bezierIndex].beginEndPoints.z = glyphData->pointArrays[c][nextPointIndex].x;
                instanceData[bezierIndex].beginEndPoints.w = glyphData->pointArrays[c][nextPointIndex].y;
                instanceData[bezierIndex].midPoint = glyphData->pointArrays[c][currentPoint];
                bezierIndex++;
            }
        }

        font->characterBezierCounts[c] = bezierIndex;
        font->characterBeziers[c] = Alloc(GetGlobalAllocator(), sizeof(BezierInstance) * bezierIndex, MEM_TAG_RENDERER_SUBSYS);
        MemoryCopy(font->characterBeziers[c], instanceData, sizeof(BezierInstance) * bezierIndex);
    }

    SimpleMapInsert(state->fontMap, fontName, font);
}

Text* TextCreate(const char* textString, const char* fontName, mat4 transform, UpdateFrequency updateFrequency)
{
    // Getting the font object
    Font* font = SimpleMapLookup(state->fontMap, fontName);

    // Creating the text struct and filling in basic data about the text
    Text* text = Alloc(state->textPool, TEXT_BLOCK_SIZE, MEM_TAG_RENDERER_SUBSYS);
    MemoryZero(text, TEXT_BLOCK_SIZE);

    text->transform = transform;
    text->enabled = true;
    text->updateFrequency = updateFrequency;
    text->id = state->nextTextId;
    state->nextTextId++;

    // Copying the string into the text struct
    u32 stringLength = strlen(textString);
    text->string = Alloc(state->textStringAllocator, stringLength, MEM_TAG_RENDERER_SUBSYS);
    MemoryCopy(text->string, textString, stringLength);

    // Generating the instance vertex buffer data for the bezier curves of the entire string.
    // First calculating the size of the buffer.
    u32 totalBeziers = 0;
    for (int i = 0; i < stringLength; i++)
    {
        u32 c = textString[i];

        // If the character outline can't be found replace the character with a '?'.
        if (c != ' ' && font->characterBeziers[c] == nullptr)
            totalBeziers += font->characterBezierCounts['?'];
        else // Otherwise just add the amount of beziers of the current character to the total bezier count.
            totalBeziers += font->characterBezierCounts[c];
    }

    text->bezierInstanceCount = totalBeziers;

    // Allocate memory for the array
    u32 bezierCurvesArraySizeBytes = sizeof(BezierInstance) * totalBeziers;
    BezierInstance* bezierCurves = Alloc(GetGlobalAllocator(), bezierCurvesArraySizeBytes, MEM_TAG_RENDERER_SUBSYS);
    u32 bezierIndex = 0;
    f32 currentWidthOffset = 0;

    // Adding the bezier curves to the array and adjusting their positions based on the width offset of the character
    for (int i = 0; i < stringLength; i++)
    {
        u32 c = textString[i];

        // If the character outline can't be found replace the character with a '?'.
        if (c != ' ' && font->characterBeziers[c] == nullptr)
        {
            c = '?';
        }

        u32 nextCharBezierIndex = bezierIndex + font->characterBezierCounts[c];

        if (c != ' ')
        {
            MemoryCopy(&bezierCurves[bezierIndex], font->characterBeziers[c], font->characterBezierCounts[c] * sizeof(BezierInstance));

            // Looping over the individual bezier curves in the current characters curves and adding the offset to them.
            for (int j = bezierIndex; j < nextCharBezierIndex; j++)
            {
                bezierCurves[j].beginEndPoints.x += currentWidthOffset;
                bezierCurves[j].beginEndPoints.z += currentWidthOffset;
                bezierCurves[j].midPoint.x += currentWidthOffset;
            }
        }

        bezierIndex = nextCharBezierIndex;
        currentWidthOffset += font->glyphData->advanceWidths[c];
    }

    // Creating the vertex buffer
    text->instanceVB = VertexBufferCreate(bezierCurves, bezierCurvesArraySizeBytes);
    Free(GetGlobalAllocator(), bezierCurves);

    // Adding the text to the text darray
    TextRefDarrayPushback(state->textDarray, &text);

    return text;
}

void TextUpdateTransform(Text* text, mat4 transform)
{
    text->transform = transform;
}

void TextRender()
{
    // TODO: add a way to check with the renderer whether we are in an appropriate renderpass and assert that.

    u32 textCount = state->textDarray->size;

    MaterialBind(state->bezierMaterial);

    // Loop over all the text objects and render the active ones.
    for (int i = 0; i < textCount; i++)
    {
        Text* text = state->textDarray->data[i];

        // Skip this text if it's disabled
        if (!text->enabled)
            continue;

        // Render the text
        VertexBuffer instancedPair[2] = {state->bezierVB, text->instanceVB};
        Draw(2, instancedPair, state->bezierIB, &text->transform, text->bezierInstanceCount);
    }
}
*/