#version 450

#include "defines.glsl"

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texCoord;

layout(location = 0) out vec2 texCoord;


layout(BIND 0) uniform UniformBufferObject
{
	mat4 uiProjection;
} ubo;


void main() {
	texCoord = v_texCoord;
	gl_Position = ubo.uiProjection * pc.model * vec4(v_position, 1);
}