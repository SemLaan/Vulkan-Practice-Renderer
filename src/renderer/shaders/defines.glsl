#define BIND set = 1, binding = 

layout(set = 0, binding = 0) uniform GlobalUniformBufferObject
{
	mat4 projView;
    vec3 viewPosition;
	vec3 directionalLight;
} globalubo;

layout(push_constant) uniform PushConstants
{
	mat4 model;
} pc;