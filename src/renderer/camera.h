#pragma once
#include "defines.h"
#include "math/lin_alg.h"
#include "core/meminc.h"

// Don't touch internals
typedef struct Camera
{
    mat4 projection;
    mat4 view;
    mat4 projectionView;
    vec3 position;
    vec3 rotation;
    bool dirty;
} Camera;

Camera CameraCreateOrthographic(f32 left, f32 right, f32 bottom, f32 top, f32 near, f32 far);

mat4 CameraGetProjectionView(Camera* camera);

vec3 CameraGetPosition(Camera* camera);
void CameraSetPosition(Camera* camera, vec3 position);

vec3 CameraGetRotation(Camera* camera);
void CameraSetRotation(Camera* camera, vec3 rotation);

vec4 CameraScreenToWorldSpace(Camera* camera, vec2 screenPosition);
