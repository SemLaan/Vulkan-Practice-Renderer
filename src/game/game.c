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
#include "renderer/render_target.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/ui/font_loader.h"

typedef struct Scene
{
    VertexBuffer sphereVB;
    IndexBuffer sphereIB;
    VertexBuffer instancedVB;
    VertexBuffer* vertexBufferDarray;
    IndexBuffer* indexBufferDarray;
    mat4* modelMatrixDarray;
    u32 instanceCount;
} Scene;

typedef struct GameState
{
    Scene scene;
    Timer timer;
    RenderTarget shadowMapRenderTarget;
    Shader lightingShader;
    Shader instancedLightingShader;
    Shader uiTextureShader;
    Shader shadowShader;
    Shader instancedShadowShader;
    Material instancedShadowMaterial;
    Material shadowMaterial;
    Material lightingMaterial;
    Material instancedLightingMaterial;
    Material uiTextureMaterial;
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
#define MAX_INSTANCE_COUNT 20000

bool OnWindowResize(EventCode type, EventData data)
{
    vec2i windowSize = GetPlatformWindowSize();
    float windowAspectRatio = windowSize.x / (float)windowSize.y;
    gameState->proj = mat4_perspective(45.0f, windowAspectRatio, 1.0f, 200.0f);
    return false;
}

void GameInit()
{
    //GlyphData* glyphData = LoadFont("Roboto-Black.ttf");
    //GlyphData* glyphData = LoadFont("Adorable Handmade.ttf");
    GlyphData* glyphData = LoadFont("Nicolast.ttf");
    const char* testString = "Beefy text testing!?.";
    u32 testStringLength = 21;

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
        modelMatrix = mat4_3Dtranslate(vec3_create(0, 10, 10));
        LoadObj("models/sphere.obj", &scene->sphereVB, &scene->sphereIB, false);

        //scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        //scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        //scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Loading cube
        modelMatrix = mat4_3Dtranslate(vec3_create(10, 1, -5));
        LoadObj("models/cube.obj", &vb, &ib, false);

        scene->vertexBufferDarray = DarrayPushback(scene->vertexBufferDarray, &vb);
        scene->indexBufferDarray = DarrayPushback(scene->indexBufferDarray, &ib);
        scene->modelMatrixDarray = DarrayPushback(scene->modelMatrixDarray, &modelMatrix);

        // Text rendering test
        u32 textPointCount = 0;
        mat4 instanceData[MAX_INSTANCE_COUNT] = {};
        u32 pointIndex = 0;
        f32 currentCharacterOffset = 0;

        for (int i = 0; i < testStringLength; i++)
        {
            u32 c = testString[i];

            if (c == ' ')
            {
                currentCharacterOffset += glyphData->advanceWidths[c];
                continue;
            }

            textPointCount += glyphData->pointCounts[c];
            GRASSERT(textPointCount < MAX_INSTANCE_COUNT);

            for (int point = 0; point < glyphData->pointCounts[c]; point++)
            {
                mat4 translation = mat4_2Dtranslate(vec2_add_vec2(vec2_mul_f32(glyphData->pointsArrays[c][point], 5), (vec2){currentCharacterOffset * 5, 10}));
                mat4 scale = mat4_3Dscale(vec3_create(0.3f, 0.3f, 0.3f));
                instanceData[pointIndex] = mat4_mul_mat4(translation, scale);

                pointIndex++;
            }

            currentCharacterOffset += glyphData->advanceWidths[c];
            _DEBUG("aw: %f", glyphData->advanceWidths[c]);
        }

        scene->instanceCount = textPointCount;
        scene->instancedVB = VertexBufferCreate(instanceData, sizeof(instanceData));
    }

    // Initializing rendering state
    gameState->shadowMapRenderTarget = RenderTargetCreate(4000, 4000, RENDER_TARGET_USAGE_NONE, RENDER_TARGET_USAGE_TEXTURE);

    ShaderCreateInfo shaderCreateInfo = {};
    shaderCreateInfo.renderTargetStencil = false;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributeCount = 3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[0] = VERTEX_ATTRIBUTE_TYPE_VEC3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[1] = VERTEX_ATTRIBUTE_TYPE_VEC3;
    shaderCreateInfo.vertexBufferLayout.perVertexAttributes[2] = VERTEX_ATTRIBUTE_TYPE_VEC2;

    shaderCreateInfo.vertexShaderName = "simple_shader";
    shaderCreateInfo.fragmentShaderName = "simple_shader";
    shaderCreateInfo.renderTargetColor = true;
    shaderCreateInfo.renderTargetDepth = true;
    gameState->lightingShader = ShaderCreate(&shaderCreateInfo);

    shaderCreateInfo.vertexShaderName = "ui_texture";
    shaderCreateInfo.fragmentShaderName = "ui_texture";
    shaderCreateInfo.renderTargetColor = true;
    shaderCreateInfo.renderTargetDepth = false;
    gameState->uiTextureShader = ShaderCreate(&shaderCreateInfo);

    shaderCreateInfo.vertexShaderName = "shadow";
    shaderCreateInfo.fragmentShaderName = nullptr;
    shaderCreateInfo.renderTargetColor = false;
    shaderCreateInfo.renderTargetDepth = true;
    gameState->shadowShader = ShaderCreate(&shaderCreateInfo);

    shaderCreateInfo.vertexBufferLayout.perInstanceAttributeCount = 1;
    shaderCreateInfo.vertexBufferLayout.perInstanceAttributes[0] = VERTEX_ATTRIBUTE_TYPE_MAT4;
    shaderCreateInfo.vertexShaderName = "shadow_instanced";

    gameState->instancedShadowShader = ShaderCreate(&shaderCreateInfo);

    shaderCreateInfo.vertexShaderName = "simple_shader_instanced";
    shaderCreateInfo.fragmentShaderName = "simple_shader";
    shaderCreateInfo.renderTargetColor = true;
    shaderCreateInfo.renderTargetDepth = true;

    gameState->instancedLightingShader = ShaderCreate(&shaderCreateInfo);

    gameState->shadowMaterial = MaterialCreate(gameState->shadowShader);
    gameState->instancedShadowMaterial = MaterialCreate(gameState->instancedShadowShader);
    gameState->instancedLightingMaterial = MaterialCreate(gameState->instancedLightingShader);
    gameState->lightingMaterial = MaterialCreate(gameState->lightingShader);
    gameState->uiTextureMaterial = MaterialCreate(gameState->uiTextureShader);
    MaterialUpdateTexture(gameState->uiTextureMaterial, "tex", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_LINEAR_CLAMP_EDGE);
    MaterialUpdateTexture(gameState->lightingMaterial, "shadowMap", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_NEAREST_CLAMP_EDGE);
    MaterialUpdateTexture(gameState->lightingMaterial, "shadowMapCompare", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_SHADOW);
    MaterialUpdateTexture(gameState->instancedLightingMaterial, "shadowMap", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_NEAREST_CLAMP_EDGE);
    MaterialUpdateTexture(gameState->instancedLightingMaterial, "shadowMapCompare", GetDepthAsTexture(gameState->shadowMapRenderTarget), SAMPLER_TYPE_SHADOW);

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
    gameState->proj = mat4_perspective(45.0f, windowAspectRatio, 1.0f, 200.0f);
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

    MaterialDestroy(gameState->instancedLightingMaterial);
    MaterialDestroy(gameState->instancedShadowMaterial);
    ShaderDestroy(gameState->instancedLightingShader);
    ShaderDestroy(gameState->instancedShadowShader);
    MaterialDestroy(gameState->shadowMaterial);
    ShaderDestroy(gameState->shadowShader);
    MaterialDestroy(gameState->uiTextureMaterial);
    ShaderDestroy(gameState->uiTextureShader);
    RenderTargetDestroy(gameState->shadowMapRenderTarget);
    MaterialDestroy(gameState->lightingMaterial);
    ShaderDestroy(gameState->lightingShader);
    TextureDestroy(gameState->texture);

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        VertexBufferDestroy(gameState->scene.vertexBufferDarray[i]);
        IndexBufferDestroy(gameState->scene.indexBufferDarray[i]);
    }

    VertexBufferDestroy(gameState->scene.sphereVB);
    VertexBufferDestroy(gameState->scene.instancedVB);
    IndexBufferDestroy(gameState->scene.sphereIB);

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
    f32 roughness = 0; // sin(TimerSecondsSinceStart(gameState->timer)) / 2 + 0.5f;
    MaterialUpdateProperty(gameState->lightingMaterial, "color", &testColor);
    MaterialUpdateProperty(gameState->lightingMaterial, "roughness", &roughness);
    MaterialUpdateProperty(gameState->instancedLightingMaterial, "color", &testColor);
    MaterialUpdateProperty(gameState->instancedLightingMaterial, "roughness", &roughness);
    

    vec2i windowSize = GetPlatformWindowSize();
    f32 windowAspectRatio = windowSize.x / (float)windowSize.y;
    mat4 uiProj = mat4_orthographic(0, 10 * windowAspectRatio, 0, 10, -1, 1);
    MaterialUpdateProperty(gameState->uiTextureMaterial, "uiProjection", &uiProj);

    //vec3 lightRotationVec = vec3_create(0.5f + sin(TimerSecondsSinceStart(gameState->timer))/2, TimerSecondsSinceStart(gameState->timer), 0);
    vec3 lightRotationVec = vec3_create(0.5f, PI/2, 0);
    mat4 shadowRotation = mat4_rotate_xyz(vec3_invert_sign(lightRotationVec));

    vec3 directionalLight = {shadowRotation.values[2 + COL4(0)], shadowRotation.values[2 + COL4(1)], shadowRotation.values[2 + COL4(2)]};

    f32 zNear = 1;
    f32 zFar = 200;
    mat4 shadowProj = mat4_orthographic(-20, 20, -20, 20, zNear, zFar);
    mat4 shadowTranslate = mat4_3Dtranslate(vec3_create(0, 0, -40));
    mat4 shadowView = mat4_mul_mat4(shadowTranslate, shadowRotation);
    mat4 shadowProjView = mat4_mul_mat4(shadowProj, shadowView);
    MaterialUpdateProperty(gameState->shadowMaterial, "shadowProjView", &shadowProjView);
    MaterialUpdateProperty(gameState->lightingMaterial, "lightTransform", &shadowProjView);
    MaterialUpdateProperty(gameState->instancedShadowMaterial, "shadowProjView", &shadowProjView);
    MaterialUpdateProperty(gameState->instancedLightingMaterial, "lightTransform", &shadowProjView);
    MaterialUpdateProperty(gameState->uiTextureMaterial, "zNear", &zNear);
    MaterialUpdateProperty(gameState->uiTextureMaterial, "zFar", &zFar);

    GlobalUniformObject globalUniformObject = {};
    globalUniformObject.viewPosition = vec3_invert_sign(gameState->camPosition);
    globalUniformObject.projView = projView;
    globalUniformObject.directionalLight = directionalLight;
    UpdateGlobalUniform(&globalUniformObject);

    // ======================================================= Rendering ================================================
    if (!BeginRendering())
        return;

    // ================== Rendering shadowmap
    RenderTargetStartRendering(gameState->shadowMapRenderTarget);

    MaterialBind(gameState->shadowMaterial);

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        Draw(1, &gameState->scene.vertexBufferDarray[i], gameState->scene.indexBufferDarray[i], &gameState->scene.modelMatrixDarray[i], 1);
    }

    VertexBuffer instancedVBPair[2] = {gameState->scene.sphereVB, gameState->scene.instancedVB};
    MaterialBind(gameState->instancedShadowMaterial);
    Draw(2, instancedVBPair, gameState->scene.sphereIB, nullptr, gameState->scene.instanceCount);

    RenderTargetStopRendering(gameState->shadowMapRenderTarget);

    // ================== Rendering scene
    RenderTargetStartRendering(GetMainRenderTarget());

    MaterialBind(gameState->lightingMaterial);

    for (int i = 0; i < DarrayGetSize(gameState->scene.vertexBufferDarray); i++)
    {
        Draw(1, &gameState->scene.vertexBufferDarray[i], gameState->scene.indexBufferDarray[i], &gameState->scene.modelMatrixDarray[i], 1);
    }

    MaterialBind(gameState->instancedLightingMaterial);
    Draw(2, instancedVBPair, gameState->scene.sphereIB, nullptr, gameState->scene.instanceCount);

    MaterialBind(gameState->uiTextureMaterial);

    mat4 translation = mat4_2Dtranslate((vec2){windowAspectRatio, 1});
    mat4 rotate = mat4_rotate_x(-PI / 2);
    mat4 scale = mat4_2Dscale((vec2){windowAspectRatio, 1});
    mat4 modelMatrix = mat4_mul_mat4(translation, mat4_mul_mat4(rotate, scale));
    Draw(1, &gameState->scene.vertexBufferDarray[1], gameState->scene.indexBufferDarray[1], &modelMatrix, 1);

    RenderTargetStopRendering(GetMainRenderTarget());

    EndRendering();
}
