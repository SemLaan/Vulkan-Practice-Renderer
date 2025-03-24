#version 450

#include "defines.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texCoord;

layout(location = 3) in mat4 i_model;
layout(location = 7) in vec4 i_color;

layout(location = 0) out vec4 f_color;
layout(location = 1) out vec4 f_quadCoordAndSize;

layout(BIND 0) uniform UniformBufferObject
{
	mat4 menuView;
} ubo;


void main() {
	gl_Position = ubo.menuView * i_model * vec4(v_position, 1);
	f_color = i_color;
	f_quadCoordAndSize = vec4(v_position.x * i_model[0][0], v_position.y * i_model[1][1], i_model[0][0], i_model[1][1]);
}