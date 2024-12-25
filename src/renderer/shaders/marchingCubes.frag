#version 450
#include "defines.glsl"
#include "lighting.glsl"



layout(location = 0) in vec3 normal;
layout(location = 1) in vec3 fragPosition;

layout(location = 0) out vec4 outColor;

layout(BIND 0) uniform UniformBufferObject
{
    vec4 color;
	float roughness;
} ubo;


void main() {

	vec3 norm = normalize(normal);

	// Specular
    float specular = BlinnPhongSpecular(norm, globalubo.viewPosition, fragPosition, globalubo.directionalLight, ubo.roughness);
    // Diffuse
    float diffuse = LambertianDiffuseSimple(norm, globalubo.directionalLight);
    // Ambient and total light
    float light = 0.1 + (diffuse + specular);

    outColor = ubo.color * vec4(light, light, light, 1);
}