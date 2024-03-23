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

layout(BIND 1) uniform sampler2DShadow shadowMap;


const uint g_PoissonSamplesCount = 18;

const vec2 g_PoissonSamples[g_PoissonSamplesCount] =
{
  { -0.220147, 0.976896 },
  { -0.735514, 0.693436 },
  { -0.200476, 0.310353 },
  { 0.180822, 0.454146 },
  { 0.292754, 0.937414 },
  { 0.564255, 0.207879 },
  { 0.178031, 0.024583 },
  { 0.613912,-0.205936 },
  { -0.385540,-0.070092 },
  { 0.962838, 0.378319 },
  { -0.886362, 0.032122 },
  { -0.466531,-0.741458 },
  { 0.006773,-0.574796 },
  { -0.739828,-0.410584 },
  { 0.590785,-0.697557 },
  { -0.081436,-0.963262 },
  { 1.000000,-0.100160 },
  { 0.622430, 0.680868 }
};

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
    vec2 shadowMapCoords = shadowSpacePosition.xy * 0.5 + 0.5;
    shadowMapCoords.y = 1 - shadowMapCoords.y;
    float bias = max(0.001 * (1.0 - dot(norm, globalubo.directionalLight)), 0.0001);
    mat2 randomRotation = MatrixFromAngle2D(6.28 * random(shadowMapCoords));


    float accumulatedShadow = 0;
    for (uint i = 0; i < g_PoissonSamplesCount; i++)
    {
        //vec2 tempCoord = shadowMapCoords + randomRotation * g_PoissonSamples[i] * 0.0008;
        vec2 tempCoord = shadowMapCoords + randomRotation * g_PoissonSamples[i] * 0.001;
        //float closestDepth = texture(shadowMap, vec2(tempCoord.x, tempCoord.y)).r;
        //accumulatedShadow += closestDepth > min(1-shadowSpacePosition.z, 1) - bias ? 1.0 : 0.0;
        accumulatedShadow += texture(shadowMap, vec3(tempCoord, min(1-shadowSpacePosition.z, 1) - bias));
    }

    float shadow = accumulatedShadow / g_PoissonSamplesCount;
    //float shadow = HardShadow(shadowSpacePosition.xyz, norm, globalubo.directionalLight, shadowMap);

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