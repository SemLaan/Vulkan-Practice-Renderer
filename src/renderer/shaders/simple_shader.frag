#version 450

#include "defines.glsl"
#include "lighting.glsl"
#include "random.glsl"

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

layout(BIND 1) uniform sampler2DShadow shadowMapCompare;
layout(BIND 2) uniform sampler2D shadowMap;




void main() 
{
    vec3 norm = normalize(normal);

    // Calculating shadow
    vec4 shadowSpacePosition = ubo.lightTransform * vec4(fragPosition, 1); // TODO: this can be calculated in the vert shader and interpolated

    // PCSS
    float shadow = PCSS(shadowMapCompare, shadowMap, shadowSpacePosition.xyz, norm, globalubo.directionalLight, 6.28 * random(shadowSpacePosition.xy));

    // PCF
    vec2 shadowMapCoords = shadowSpacePosition.xy * 0.5 + 0.5;
    shadowMapCoords.y = 1 - shadowMapCoords.y;
    float fragDepth = min(1-shadowSpacePosition.z, 1);
    //float shadow = PercentageCloserFilter(shadowMapCoords, fragDepth, 0.006, shadowMapCompare, norm, globalubo.directionalLight, 6.28 * random(shadowSpacePosition.xy));

    // Hard shadow
    //float shadow = HardShadow(shadowSpacePosition.xyz, norm, globalubo.directionalLight, shadowMapCompare);

    if (1-shadowSpacePosition.z > 1)
        shadow = 1;

    // Specular
    float specular = BlinnPhongSpecular(norm, globalubo.viewPosition, fragPosition, globalubo.directionalLight, ubo.roughness);
    // Diffuse
    float diffuse = LambertianDiffuseSimple(norm, globalubo.directionalLight);
    // Ambient and total light
    float light = 0.1 + (diffuse + specular) * shadow;

    outColor = ubo.color * vec4(light, light, light, 1);
}