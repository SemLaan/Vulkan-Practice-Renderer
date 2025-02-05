#version 450
#include "defines.glsl"



layout(location = 0) in vec3 normal;

layout(location = 0) out vec4 outColor;



void main() {
	vec3 norm = normalize(normal);
	norm = norm * 0.5 + 0.5;

    outColor = vec4(norm, 1);
}