#version 450

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_color;
layout(location = 2) in vec2 v_texCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 texCoord;
layout(location = 2) out flat uint textureIndex;

layout(set = 0, binding = 0) uniform UniformBufferObject
{
	mat4 projView;
} ubo;

layout(push_constant) uniform PushConstants
{
	mat4 model;
} pc;


void main() {
	fragColor = v_color;
	texCoord = v_texCoord;
	textureIndex = 0;
	gl_Position = ubo.projView * pc.model * vec4(v_position, 1);
}