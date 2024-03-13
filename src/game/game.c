#include "game.h"

#include "core/logger.h"
#include "core/platform.h"
#include "core/input.h"
#include "core/event.h"
#include "core/timer.h"
#include "renderer/material.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/obj_loader.h"
#include "renderer/texture.h"
#include "math/lin_alg.h"

typedef struct GameState
{
    Timer timer;
    VertexBuffer vertexBuffer;
    IndexBuffer indexBuffer;
    Shader shader;
    Material material;
    Texture texture;
    vec3 camPosition;
	vec3 camRotation;
	mat4 view;
	mat4 proj;
	bool mouseEnabled;
	bool perspectiveEnabled;
} GameState;

GameState* gameState = nullptr;

#define QUAD_VERT_COUNT 4
#define QUAD_INDEX_COUNT 6

bool OnWindowResize(EventCode type, EventData data)
{
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    gameState->proj = mat4_perspective(45.0f, windowAspectRatio, 0.1f, 1000.0f);
    return false;
}

void GameInit()
{
    gameState = Alloc(GetGlobalAllocator(), sizeof(*gameState), MEM_TAG_GAME);

    // Initializing rendering state
    gameState->shader = ShaderCreate("simple_shader");
    gameState->material = MaterialCreate(gameState->shader);

    u8 pixels[TEXTURE_CHANNELS * 2];
    pixels[0] = 255;
    pixels[1] = 0;
    pixels[2] = 255;
    pixels[3] = 255;
    pixels[4] = 125;
    pixels[5] = 255;
    pixels[6] = 255;
    pixels[7] = 255;

    gameState->texture = TextureCreate(2, 1, pixels);

    MaterialUpdateTexture(gameState->material, "albedo", gameState->texture);
    MaterialUpdateTexture(gameState->material, "heightMap", gameState->texture);
    
    LoadObj("models/beefy_gun.obj", &gameState->vertexBuffer, &gameState->indexBuffer, false);

    // Initializing camera
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    gameState->proj = mat4_perspective(45.0f, windowAspectRatio, 0.1f, 1000.0f);
    gameState->view = mat4_identity();
    gameState->camPosition = (vec3){0, 0, -10};
    gameState->camRotation = (vec3){0, 0, 0};

    gameState->mouseEnabled = false;
    gameState->perspectiveEnabled = true;

    RegisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    StartOrResetTimer(&gameState->timer);
}

void GameShutdown()
{
    UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    IndexBufferDestroy(gameState->indexBuffer);
    VertexBufferDestroy(gameState->vertexBuffer);
    MaterialDestroy(gameState->material);
    ShaderDestroy(gameState->shader);
    TextureDestroy(gameState->texture);

    Free(GetGlobalAllocator(), gameState);
}

void GameUpdateAndRender()
{
    // =========================== Update ===================================
    f32 mouseMoveSpeed = 3500;

    if (gameState->mouseEnabled)
    {
        gameState->camRotation.y -= GetMouseDistanceFromCenter().x / mouseMoveSpeed;
        gameState->camRotation.x -= GetMouseDistanceFromCenter().y / mouseMoveSpeed;
    }
    if (gameState->camRotation.x > 1.5f)
        gameState->camRotation.x = 1.5f;
    if (gameState->camRotation.x < -1.5f)
        gameState->camRotation.x = -1.5f;

    // Create the rotation matrix
    mat4 rotation = mat4_rotate_xyz(gameState->camRotation);

    vec3 forwardVector = {-rotation.values[2 + COL4(0)], -rotation.values[2 + COL4(1)], -rotation.values[2 + COL4(2)]};
    vec3 rightVector = {rotation.values[0 + COL4(0)], rotation.values[0 + COL4(1)], rotation.values[0 + COL4(2)]};

    vec3 frameMovement = {};

    if (GetKeyDown(KEY_A))
        frameMovement = vec3_add_vec3(frameMovement, rightVector);
    if (GetKeyDown(KEY_D))
        frameMovement = vec3_min_vec3(frameMovement, rightVector);
    if (GetKeyDown(KEY_S))
        frameMovement = vec3_add_vec3(frameMovement, forwardVector);
    if (GetKeyDown(KEY_W))
        frameMovement = vec3_min_vec3(frameMovement, forwardVector);
    if (GetKeyDown(KEY_SHIFT))
        frameMovement.y += 1;
    if (GetKeyDown(KEY_SPACE))
        frameMovement.y -= 1;
    gameState->camPosition = vec3_add_vec3(gameState->camPosition, vec3_div_float(frameMovement, 300.f));

    mat4 translate = mat4_3Dtranslate(gameState->camPosition);

    gameState->view = mat4_mul_mat4(rotation, translate);

    if (GetButtonDown(BUTTON_LEFTMOUSEBTN) && !GetButtonDownPrevious(BUTTON_LEFTMOUSEBTN))
    {
        gameState->mouseEnabled = !gameState->mouseEnabled;
        InputSetMouseCentered(gameState->mouseEnabled);
    }

    // ============================ Rendering ===================================
    mat4 projView = mat4_mul_mat4(gameState->proj, gameState->view);
    vec4 testColor = vec4_create(0.2, 0.4f, 1, 1);
    MaterialUpdateProperty(gameState->material, "color", &testColor);

    vec4 directionalLight = mat4_mul_vec4(mat4_rotate_z(TimerSecondsSinceStart(gameState->timer)), vec4_create(1, 0, 0, 1));

    GlobalUniformObject globalUniformObject = {};
    globalUniformObject.projView = projView;
    globalUniformObject.directionalLight = vec4_to_vec3(directionalLight);
    UpdateGlobalUniform(&globalUniformObject);

    if (!BeginRendering())
        return;

    PushConstantObject pushConstantValues = {};
    pushConstantValues.model = mat4_identity();
    Draw(gameState->material, gameState->vertexBuffer, gameState->indexBuffer, &pushConstantValues);

    EndRendering();
}
