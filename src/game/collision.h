#pragma once
#include "defines.h"

#include "renderer/renderer.h"
#include "math/lin_alg.h"

typedef struct RaycastHit
{
	f32 hitDistance;
	u32 triangleFirstIndex;
	bool hit;
} RaycastHit;



RaycastHit RaycastMesh(vec3 origin, vec3 direction, MeshData mesh, mat4 modelMatrix, u32 positionOffset, u32 normalOffset);

