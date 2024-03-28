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

layout(BIND 2) uniform sampler2D shadowMap;
layout(BIND 1) uniform sampler2DShadow shadowMapCompare;


mat2 MatrixFromAngle2D(float theta)
{
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);

    vec2 row1 = vec2(cosTheta, sinTheta);
    vec2 row2 = vec2(-sinTheta, cosTheta);
    mat2 result = mat2(row1, row2);
    return result;
}

void main() 
{
    vec3 norm = normalize(normal);

    // Calculating shadow
    vec4 shadowSpacePosition = ubo.lightTransform * vec4(fragPosition, 1); // TODO: this can be calculated in the vert shader and interpolated
    //vec2 shadowMapCoords = shadowSpacePosition.xy * 0.5 + 0.5;
    //shadowMapCoords.y = 1 - shadowMapCoords.y;
    //float bias = max(0.001 * (1.0 - dot(norm, globalubo.directionalLight)), 0.0001);
    //mat2 randomRotation = MatrixFromAngle2D(6.28 * random(shadowMapCoords));


    //float accumulatedShadow = 0;
    //for (uint i = 0; i < g_PoissonSamplesCount; i++)
    //{
        //vec2 tempCoord = shadowMapCoords + randomRotation * g_PoissonSamples[i] * 0.0008;
    //    vec2 tempCoord = shadowMapCoords + randomRotation * g_PoissonSamples[i] * 0.001;
        //float closestDepth = texture(shadowMap, vec2(tempCoord.x, tempCoord.y)).r;
        //accumulatedShadow += closestDepth > min(1-shadowSpacePosition.z, 1) - bias ? 1.0 : 0.0;
    //    accumulatedShadow += texture(shadowMapCompare, vec3(tempCoord, min(1-shadowSpacePosition.z, 1) - bias));
    //}

    //float shadow = accumulatedShadow / g_PoissonSamplesCount;
    //float shadow = HardShadow(shadowSpacePosition.xyz, norm, globalubo.directionalLight, shadowMapCompare);

    vec2 shadowMapCoords = shadowSpacePosition.xy * 0.5 + 0.5;
    shadowMapCoords.y = 1 - shadowMapCoords.y;

    float pcss = PCSS(shadowMapCompare, shadowMap, shadowSpacePosition.xyz, norm, globalubo.directionalLight);
    float shadow = PercentageCloserFilter(shadowMapCoords, shadowSpacePosition.z, 0.001, shadowMapCompare, norm, globalubo.directionalLight);

    outColor = vec4(pcss.rrr, 1);
    return;


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