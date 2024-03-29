#version 450

#include "defines.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texCoord;
layout(location = 3) in mat4 i_model;

layout(BIND 0) uniform UniformBufferObject
{
	mat4 shadowProjView;
} ubo;


void main() {
	gl_Position = ubo.shadowProjView * i_model * vec4(v_position, 1);
}