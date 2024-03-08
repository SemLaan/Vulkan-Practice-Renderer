#include "game.h"
#include "renderer/shader.h"
#include "renderer/material.h"
#include "core/logger.h"

void GameInit()
{
    Shader shader = ShaderCreate();

    Material material = MaterialCreate(shader);

    MaterialDestroy(material);
    ShaderDestroy(shader);
    _DEBUG("beef");
}

void GameUpdateAndRender()
{
    
}

