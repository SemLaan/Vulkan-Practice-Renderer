#version 450

#include "defines.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texCoord;
layout(location = 3) in mat4 i_model;

layout(location = 0) out vec3 normal;
layout(location = 1) out vec2 texCoord;
layout(location = 2) out vec3 fragPosition;


void main() {
	normal = (i_model * vec4(v_normal, 0)).xyz;
	texCoord = v_texCoord;
	vec4 worldPosition = i_model * vec4(v_position, 1);
	fragPosition = vec3(worldPosition);
	gl_Position = globalubo.projView * worldPosition;
}