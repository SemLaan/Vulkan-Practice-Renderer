#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 fragPosition;

layout(set = 0, binding = 0) uniform GlobalUniformBufferObject
{
	mat4 projView;
    vec3 viewPosition;
	vec3 directionalLight;
} globalubo;

layout(set = 1, binding = 0) uniform UniformBufferObject
{
    vec4 color;
} ubo;

void main() 
{
    vec3 norm = normalize(normal);

    // Specular
    vec3 viewDir = normalize(globalubo.viewPosition - fragPosition);
    vec3 halfDir = normalize(viewDir + globalubo.directionalLight);

    float light = 0.5 * pow(max(dot(norm, halfDir), 0.0), 32);

    // Diffuse
    light += max(dot(norm, globalubo.directionalLight), 0);

    // Ambient
    light += 0.1;

    outColor = ubo.color * vec4(light, light, light, 1);
}