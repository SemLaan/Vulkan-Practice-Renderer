#version 450
#include "defines.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;

layout(location = 0) out vec3 normal;

void main() {
	normal = (pc.model * vec4(v_normal, 0)).xyz;
	vec4 worldPosition = pc.model * vec4(v_position, 1);
	gl_Position = globalubo.projView * worldPosition;
}