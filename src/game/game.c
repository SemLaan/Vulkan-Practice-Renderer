#include "game.h"

#include "containers/darray.h"
#include "core/event.h"
#include "core/input.h"
#include "core/logger.h"
#include "core/platform.h"
#include "core/timer.h"
#include "math/lin_alg.h"
#include "renderer/material.h"
#include "renderer/obj_loader.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/render_target.h"

typedef struct Scene
{
    VertexBuffer* vertexBufferDarray;
    IndexBuffer* indexBufferDarray;
    mat4* modelMatrixDarray;
} Scene;

typedef struct GameState
{
    Scene scene;
    Timer timer;
    RenderTarget shadowMapRenderTarget;
    Shader simpleLightingShader;
    Shader simpleTextureShader;
    Material simpleLightingMaterial;
    Material simpleTextureMaterial;
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
    gameState->proj = mat4_perspective(45.0f, windowAspectRatio, 0.1f, 200.0f);
    return false;
}

void GameInit()
{
    gameState = Alloc(GetGlobalAllocator(), sizeof(*gameState), MEM_TAG_GAME);

    // Creating the scene
    {
        Scene* scene = &gameState->scene;
        scene->vertexBufferDarray = DarrayCreate(sizeof(VertexBuffer), 5, GetGlobalAllocator(), MEM_TAG_GAME);
        scene->indexBufferDarray = DarrayCreate(sizeof(IndexBuffer), 5, GetGlobalAllocator(), MEM_TAG_GAME);
        scene->modelMatrixDarray = DarrayCreate(sizeof(mat4), 5, GetGlobalAllocator(), MEM_TAG_GAME);

        VertexBuffer vb;
        IndexBuffer ib;
        mat4 modelMatrix;

        // Loading gun
        modelMatrix = mat4_3Dtranslate(vec3_create(0, 3, 0));
        LoadObj("models/beefy_gun.obj", &vb, &ib, false);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Loading quad
        modelMatrix = mat4_3Dscale(vec3_create(20, 1, 20));
        LoadObj("models/quad.obj", &vb, &ib, false);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Loading sphere
        modelMatrix = mat4_3Dtranslate(vec3_create(10, 1, 10));
        LoadObj("models/sphere.obj", &vb, &ib, false);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Loading cube
        modelMatrix = mat4_3Dtranslate(vec3_create(10, 1, -5));
        LoadObj("models/cube.obj", &vb, &ib, false);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);
    }

    // Initializing rendering state
    gameState->shadowMapRenderTarget = RenderTargetCreate(2460, 1440, RENDER_TARGET_USAGE_TEXTURE, RENDER_TARGET_USAGE_TEXTURE);
    gameState->simpleLightingShader = ShaderCreate("simple_shader");
    gameState->simpleLightingMaterial = MaterialCreate(gameState->simpleLightingShader);
    gameState->simpleTextureShader = ShaderCreate("simple_texture");
    gameState->simpleTextureMaterial = MaterialCreate(gameState->simpleTextureShader);
    MaterialUpdateTexture(gameState->simpleTextureMaterial, "tex", GetColorAsTexture(gameState->shadowMapRenderTarget));

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

    // Initializing camera
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    gameState->proj = mat4_perspective(45.0f, windowAspectRatio, 0.1f, 1000.0f);
    gameState->view = mat4_identity();
    gameState->camPosition = (vec3){0, -5, -10};
    gameState->camRotation = (vec3){0, 0, 0};

    gameState->mouseEnabled = false;
    gameState->perspectiveEnabled = true;

    RegisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    StartOrResetTimer(&gameState->timer);
}

void GameShutdown()
{
    UnregisterEventListener(EVCODE_WINDOW_RESIZED, OnWindowResize);

    MaterialDestroy(gameState->simpleTextureMaterial);
    ShaderDestroy(gameState->simpleTextureShader);
    RenderTargetDestroy(gameState->shadowMapRenderTarget);
    MaterialDestroy(gameState->simpleLightingMaterial);
    ShaderDestroy(gameState->simpleLightingShader);
    TextureDestroy(gameState->texture);

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        VertexBufferDestroy(gameState->scene.vertexBufferDarray[i]);
        IndexBufferDestroy(gameState->scene.indexBufferDarray[i]);
    }

    DarrayDestroy(gameState->scene.vertexBufferDarray);
    DarrayDestroy(gameState->scene.indexBufferDarray);
    DarrayDestroy(gameState->scene.modelMatrixDarray);

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
    MaterialUpdateProperty(gameState->simpleLightingMaterial, "color", &testColor);

    vec4 directionalLight = mat4_mul_vec4(mat4_rotate_z(/*TimerSecondsSinceStart(gameState->timer)*/-1), vec4_create(1, 0, 0, 1));

    vec3 camposcopy = gameState->camPosition;
    camposcopy.x = camposcopy.x;

    GlobalUniformObject globalUniformObject = {};
    globalUniformObject.viewPosition = vec3_invert_sign(camposcopy);
    globalUniformObject.projView = projView;
    globalUniformObject.directionalLight = vec4_to_vec3(directionalLight);
    UpdateGlobalUniform(&globalUniformObject);

    if (!BeginRendering())
        return;

    RenderTargetStartRendering(gameState->shadowMapRenderTarget);

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        Draw(gameState->simpleLightingMaterial, gameState->scene.vertexBufferDarray[i], gameState->scene.indexBufferDarray[i], &gameState->scene.modelMatrixDarray[i]);
    }

    RenderTargetStopRendering(gameState->shadowMapRenderTarget);

    RenderTargetStartRendering(GetMainRenderTarget());

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        if (i != 1)
            Draw(gameState->simpleLightingMaterial, gameState->scene.vertexBufferDarray[i], gameState->scene.indexBufferDarray[i], &gameState->scene.modelMatrixDarray[i]);
        else
            Draw(gameState->simpleTextureMaterial, gameState->scene.vertexBufferDarray[i], gameState->scene.indexBufferDarray[i], &gameState->scene.modelMatrixDarray[i]);
    }

    RenderTargetStopRendering(GetMainRenderTarget());

    EndRendering();
}
