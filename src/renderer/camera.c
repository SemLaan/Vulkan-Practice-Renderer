#include "camera.h"

#include "core/platform.h"

// Recalculates the view matrix of the camera based on the position and rotation
// Recalculates the view projection matrix based on the view matrix and projection matrix
void CameraRecalculateViewAndViewProjection(Camera* camera)
{
	mat4 cameraViewTranslation = mat4_3Dtranslate(vec3_invert_sign(camera->position));
	mat4 cameraViewRotation = mat4_rotate_xyz(vec3_invert_sign(camera->rotation));
	camera->view = mat4_mul_mat4(cameraViewRotation, cameraViewTranslation);
	camera->viewProjection = mat4_mul_mat4(camera->projection, camera->view);
}

// Recalculates the inverse view projection matrix based on the position, rotation and inverse projection matrix
void CameraRecalculateInverseViewProjection(Camera* camera)
{
	mat4 cameraViewInverseTranslation = mat4_3Dtranslate(camera->position);
	mat4 cameraViewInverseRotation = mat4_rotate_xyz(camera->rotation);
	mat4 cameraInverseView = mat4_mul_mat4(cameraViewInverseRotation, cameraViewInverseTranslation);
	camera->inverseViewProjection = mat4_mul_mat4(cameraInverseView, camera->inverseProjection);
}

vec3 CameraGetForward(Camera* camera)
{
	return vec3_create(-camera->view.values[2 + COL4(0)], -camera->view.values[2 + COL4(1)], -camera->view.values[2 + COL4(2)]);
}

vec3 CameraGetRight(Camera* camera)
{
    return vec3_create(camera->view.values[0 + COL4(0)], camera->view.values[0 + COL4(1)], camera->view.values[0 + COL4(2)]);
}

vec3 CameraGetUp(Camera* camera)
{
	// TODO: 
	return vec3_create(0, 0, 0);
}

// Gives the point on the near plane that corresponds to the given screen position
// NOTE: Make sure the inverseViewProjection matrix is up to date by having called the recalculate 
// function after the most recent camera position or rotation change.
vec4 CameraScreenToWorldSpace(Camera* camera, vec2 screenPosition)
{
	vec2i windowSize = GetPlatformWindowSize();

    screenPosition.x = screenPosition.x / windowSize.x;
    screenPosition.y = screenPosition.y / windowSize.y;
    screenPosition.x = screenPosition.x * 2;
    screenPosition.y = screenPosition.y * -2;
    screenPosition.x -= 1;
    screenPosition.y += 1;

    vec4 position = {};
    position.x = screenPosition.x;
    position.y = screenPosition.y;
    position.z = 0.0f;
    position.w = 1.f;

	position = mat4_mul_vec4(camera->inverseViewProjection, position);

    return position;
}


