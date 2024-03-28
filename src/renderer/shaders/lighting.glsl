const uint POISSON_SAMPLE_COUNT = 18;

const vec2 g_PoissonSamples[POISSON_SAMPLE_COUNT] =
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


// Calculates the specular component of the light value using the blinn phong technique (assumes world space)
// Roughness is expected to be between 0 and 1
float BlinnPhongSpecular(vec3 norm, vec3 viewPosition, vec3 fragPosition, vec3 lightDirection, float roughness)
{
    // Specular
    vec3 viewDir = normalize(viewPosition - fragPosition);
    vec3 halfDir = normalize(viewDir + lightDirection);
    roughness = 1 - roughness;
    float specularExponent = 2 + 500 * (roughness * roughness);

    return 0.4 * pow(max(dot(norm, halfDir), 0.0), specularExponent);
}

// Calculates the diffuse component of the light value by simply taking the dot product of the light direction and the normal (assumes world space)
float LambertianDiffuseSimple(vec3 norm, vec3 lightDirection)
{
    return max(dot(norm, lightDirection), 0);
}

// Calculates whether the current fragment is in shadow or not, returns 0 if in shadow, 1 if out of shadow (normal and light direction expected to be world space)
float HardShadow(vec3 shadowSpaceFragPosition, vec3 normal, vec3 lightDirection, sampler2DShadow shadowMapCompare)
{
    vec2 shadowMapCoords = shadowSpaceFragPosition.xy * 0.5 + 0.5;
    shadowMapCoords.y = 1 - shadowMapCoords.y;
    float bias = max(0.001 * (1.0 - dot(normal, lightDirection)), 0.0001);

    float shadow = texture(shadowMapCompare, vec3(shadowMapCoords, min(1-shadowSpaceFragPosition.z, 1) - bias));
    return shadow;
}



// Returns a value from zero to one bases on how occluded this pixel is (zero is completely occluded)
float PercentageCloserFilter(vec2 shadowMapCoords, float fragmentDepth, float filterRadius, sampler2DShadow shadowMapCompare, vec3 normal, vec3 lightDirection)
{
    float sum = 0;
    float bias = max(0.001 * (1.0 - dot(normal, lightDirection)), 0.0001);

    for (int i = 0; i < POISSON_SAMPLE_COUNT; i++)
    {
        vec2 offset = g_PoissonSamples[i] * filterRadius;
        sum += texture(shadowMapCompare, vec3(shadowMapCoords + offset, min(1-fragmentDepth, 1) - bias));
    }

    return sum / POISSON_SAMPLE_COUNT;
}

const float LIGHT_SIZE = 0.001;
// Percentage closer soft shadows
float PCSS(sampler2DShadow shadowMapCompare, sampler2D shadowMap, vec3 shadowSpaceFragPosition, vec3 normal, vec3 lightDirection)
{
    vec2 shadowMapCoords = shadowSpaceFragPosition.xy * 0.5 + 0.5;
    shadowMapCoords.y = 1 - shadowMapCoords.y;
    float fragDepth = min(1-shadowSpaceFragPosition.z, 1);
    float bias = max(0.001 * (1.0 - dot(normal, lightDirection)), 0.0001);
    
    // Calculate avg blocker depth
    float blockerSum = 0;
    float blockerCount = 0;

    float blockerSearchRadius = 0.001;//LIGHT_SIZE * fragDepth;
    
    float shadowMapDepth = texture(shadowMap, shadowMapCoords).r;
    if (shadowMapDepth > fragDepth - bias)
    {
        return 1;
        blockerCount += 1;
        blockerSum += shadowMapDepth;
    }
    else
    {
        return shadowMapDepth;
    }

    for (int i = 0; i < POISSON_SAMPLE_COUNT; i++)
    {
        float shadowMapDepth = texture(shadowMap, shadowMapCoords + g_PoissonSamples[i] * blockerSearchRadius).r;
        if (shadowMapDepth > fragDepth - bias)
        {
            blockerCount += 1;
            blockerSum += shadowMapDepth;
        }
    }

    // Early out if no blockers
    if (blockerCount < 1)
        return 1;

    float avgBlockerDepth = blockerSum / blockerCount;

    return avgBlockerDepth;

    // Calculate penumbra ration


    // Calculate final shadow value with pcf

}

/*
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
        float closestDepth = texture(shadowMap, vec2(tempCoord.x, tempCoord.y)).r;
        accumulatedShadow += closestDepth > min(1-shadowSpacePosition.z, 1) - bias ? 1.0 : 0.0;
    }

    float shadow = accumulatedShadow / g_PoissonSamplesCount;
    //float shadow = texture(shadowMap, vec2(shadowMapCoords.x, shadowMapCoords.y)).r > min(1-shadowSpacePosition.z, 1) - bias ? 1.0 : 0.0;

    if (1-shadowSpacePosition.z > 1)
        shadow = 1;*/