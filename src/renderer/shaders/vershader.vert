#version 450

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texCoord;

layout(location = 0) out vec3 normal;
layout(location = 1) out vec2 texCoord;

layout(set = 0, binding = 0) uniform UniformBufferObject
{
	mat4 projView;
} ubo;

layout(push_constant) uniform PushConstants
{
	mat4 model;
} pc;


void main() {
	normal = v_normal;
	texCoord = v_texCoord;
	gl_Position = ubo.projView * pc.model * vec4(v_position, 1);
}