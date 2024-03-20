#version 450

#include "defines.glsl"
#include "lighting.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 fragPosition;


layout(set = 1, binding = 0) uniform UniformBufferObject
{
    vec4 color;
    float roughness;
} ubo;

void main() 
{
    vec3 norm = normalize(normal);

    float light = BlinnPhongSpecular(norm, globalubo.viewPosition, fragPosition, globalubo.directionalLight, ubo.roughness);
    light += LambertianDiffuseSimple(norm, globalubo.directionalLight);
    // Ambient
    light += 0.1;

    outColor = ubo.color * vec4(light, light, light, 1);
}