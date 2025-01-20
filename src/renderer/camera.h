#pragma once
#include "defines.h"
#include "math/lin_alg.h"
#include "core/meminc.h"

// Camera struct, can represent both perspective and orthographic camera's
typedef struct Camera
{
	mat4 inverseProjection;			// Responsibility to update this is on the user
	mat4 inverseViewProjection;		// Used in CameraScreenToWorldSpace, can be updated by calling recalculate inverse view projection
    mat4 projection;				// Responsibility to update this is on the user
    mat4 view;						// View matrix, can be updated by calling recalculate view and view projection
    mat4 viewProjection;			// View projection matrix, can be updated by calling recalculate view and view projection
    vec3 position;					// Current position of the camera
    vec3 rotation;					// Current rotation of the camera
} Camera;


// Recalculates the view matrix of the camera based on the position and rotation
// Recalculates the view projection matrix based on the view matrix and projection matrix
void CameraRecalculateViewAndViewProjection(Camera* camera);
// Recalculates the inverse view projection matrix based on the position, rotation and inverse projection matrix
void CameraRecalculateInverseViewProjection(Camera* camera);

vec3 CameraGetForward(Camera* camera);
vec3 CameraGetRight(Camera* camera);
vec3 CameraGetUp(Camera* camera);

// Gives the point on the near plane that corresponds to the given screen position
// NOTE: Make sure the inverseViewProjection matrix is up to date by having called the recalculate 
// function after the most recent camera position or rotation change.
vec4 CameraScreenToWorldSpace(Camera* camera, vec2 screenPosition);
