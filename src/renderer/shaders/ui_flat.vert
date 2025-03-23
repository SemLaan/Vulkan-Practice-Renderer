#version 450
#include "defines.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texCoord;


void main() {
	gl_Position = pc.model * vec4(v_position, 1);
}