#version 450

#include "defines.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texCoord;

layout(location = 1) out vec2 texCoord;


void main() {
	texCoord = v_texCoord;
	vec4 worldPosition = pc.model * vec4(v_position, 1);
	gl_Position = globalubo.projView * worldPosition;
}