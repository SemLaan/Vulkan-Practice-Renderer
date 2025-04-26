#version 450
#include "defines.glsl"

layout(location = 0) in vec4 f_color;
layout(location = 1) in vec4 f_quadCoordAndSize;

layout(location = 0) out vec4 outColor;


layout(BIND 1) uniform UniformBufferObject
{
    vec4 color;
	vec4 other; // x: line thickness, y = corner Radius, z = transparency transition thickness
} ubo;


void main() {
	vec3 finalColor = f_color.xyz;
	float left = ubo.other.y;
	float right = f_quadCoordAndSize.z - ubo.other.y;
	float top = f_quadCoordAndSize.w - ubo.other.y;
	float bottom = ubo.other.y;
	vec2 d = vec2(max(left - f_quadCoordAndSize.x, f_quadCoordAndSize.x - right), max(bottom - f_quadCoordAndSize.y, f_quadCoordAndSize.y - top));
	float signedDistance = length(max(vec2(0.0), d)) + min(0.0, max(d.x, d.y));

	if (signedDistance < ubo.other.y && signedDistance >= ubo.other.y - ubo.other.x)
		finalColor = ubo.color.xyz;

	float alpha = clamp(mix(1, 0, (signedDistance - ubo.other.y + ubo.other.z) / ubo.other.z), 0, 1);

	outColor = ToLinearRGB(vec4(finalColor, alpha));
}