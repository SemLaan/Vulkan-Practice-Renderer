#include "game.h"
#include "core/logger.h"
#include "renderer/material.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"

typedef struct GameState
{
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    Shader shader;
    Material material;
} GameState;

GameState* gameState = nullptr;

#define QUAD_VERT_COUNT 4
#define QUAD_INDEX_COUNT 6

void GameInit()
{
    gameState = Alloc(GetGlobalAllocator(), sizeof(*gameState), MEM_TAG_GAME);

    gameState->shader = ShaderCreate();

    gameState->material = MaterialCreate(gameState->shader);

    Vertex quadVertices[QUAD_VERT_COUNT] =
        {
            {{0.0, 0.0, 0}, {0, 0, 0}, {0, 1}},
            {{1.0, 0.0, 0}, {0, 0, 0}, {1, 1}},
            {{0.0, 1.0, 0}, {0, 0, 0}, {0, 0}},
            {{1.0, 1.0, 0}, {0, 0, 0}, {1, 0}},
        };

    gameState->vertexBuffer = VertexBufferCreate(quadVertices, sizeof(quadVertices));

    u32 quadIndices[QUAD_INDEX_COUNT] =
        {
            0, 1, 2,
            2, 1, 3,
        };

    gameState->indexBuffer = IndexBufferCreate(quadIndices, QUAD_INDEX_COUNT);
}

void GameUpdateAndRender()
{
    if (!BeginRendering())
        return;

    PushConstantObject pushConstantValues = {};
    //pushConstantValues.model = mat4();
    Draw(gameState->material, gameState->vertexBuffer, gameState->indexBuffer, &pushConstantValues);

    EndRendering();
}
