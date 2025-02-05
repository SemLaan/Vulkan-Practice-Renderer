#version 450
#include "defines.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec2 v_texCoord;

layout(location = 0) out vec2 texCoord;

void main() {
	gl_Position = vec4(v_position, 1);
	texCoord = v_texCoord;
}