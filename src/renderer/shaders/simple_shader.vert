#version 450

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_texCoord;

layout(location = 0) out vec3 normal;
layout(location = 1) out vec2 texCoord;

layout(set = 0, binding = 0) uniform GlobalUniformBufferObject
{
	mat4 projView;
	vec3 directionalLight;
} globalubo;


layout(set = 1, binding = 3) uniform sampler2D heightMap;

layout(push_constant) uniform PushConstants
{
	mat4 model;
} pc;


void main() {
	normal = (pc.model * vec4(v_normal, 0)).xyz;
	texCoord = v_texCoord;
	//vec3 displacedPosition = v_position + v_normal * texture(heightMap, v_texCoord).r;
	gl_Position = globalubo.projView * pc.model * vec4(v_position, 1);
}