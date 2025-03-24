#version 450
#include "defines.glsl"

layout(location = 0) in vec4 f_color;
layout(location = 1) in vec4 f_quadCoordAndSize;

layout(location = 0) out vec4 outColor;


layout(BIND 1) uniform UniformBufferObject
{
    vec4 color;
} ubo;


void main() {
	float left = 0.04;
	float right = f_quadCoordAndSize.z - 0.04;
	float top = f_quadCoordAndSize.w - 0.04;
	float bottom = 0.04;
	vec2 d = vec2(max(left - f_quadCoordAndSize.x, f_quadCoordAndSize.x - right), max(bottom - f_quadCoordAndSize.y, f_quadCoordAndSize.y - top));
	float signedDistance = length(max(vec2(0.0), d)) + min(0.0, max(d.x, d.y));

	signedDistance = max(0, signedDistance);
	signedDistance *= (1/0.04);
	signedDistance = 1 - signedDistance;
	if (signedDistance > 0)
		signedDistance = 1;

	//float signedDistance = 1;
	//if (max(d.x, d.y) > 0)
	//	signedDistance = 0;

    outColor = vec4(f_color.xyz, signedDistance);
}