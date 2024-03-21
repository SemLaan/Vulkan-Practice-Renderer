#version 450

#include "defines.glsl"
#include "lighting.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in vec3 fragPosition;


layout(BIND 0) uniform UniformBufferObject
{
    mat4 lightTransform;
    vec4 color;
    float roughness;
} ubo;

layout(BIND 1) uniform sampler2D shadowMap;


void main() 
{
    vec3 norm = normalize(normal);

    //float rawDepth = 1 - texture(tex, vec2(texCoord.x, 1-texCoord.y)).r;
    vec4 shadowPosition = ubo.lightTransform * vec4(fragPosition, 1);
    vec2 shadowCoords = shadowPosition.xy * 0.5 + 0.5;
    float closestDepth = texture(shadowMap, vec2(shadowCoords.x, 1-shadowCoords.y)).r;

    float bias = max(0.001 * (1.0 - dot(norm, globalubo.directionalLight)), 0.0001);
    //float bias = 0.005;  
    float shadow = closestDepth > min(1-shadowPosition.z, 1) - bias ? 1.0 : 0.0;

    if (1-shadowPosition.z > 1)
        shadow = 1;

    float specular = BlinnPhongSpecular(norm, globalubo.viewPosition, fragPosition, globalubo.directionalLight, ubo.roughness);
    float diffuse = LambertianDiffuseSimple(norm, globalubo.directionalLight);
    // Ambient
    float light = 0.1 + (diffuse + specular) * shadow;

    outColor = ubo.color * vec4(light, light, light, 1);
    //outColor = vec4(shadowCoords.x, shadowCoords.y, 0, 1);
}