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


mat2 MatrixFromAngle2D(float theta)
{
    float sinTheta = sin(theta);
    float cosTheta = cos(theta);

    vec2 row1 = vec2(cosTheta, sinTheta);
    vec2 row2 = vec2(-sinTheta, cosTheta);
    mat2 result = mat2(row1, row2);
    return result;
}


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

    float shadow = texture(shadowMapCompare, vec3(shadowMapCoords, min(1-shadowSpaceFragPosition.z, 1) - bias*0.5));
    return shadow;
}



// Returns a value from zero to one bases on how occluded this pixel is (zero is completely occluded)
float PercentageCloserFilter(vec2 shadowMapCoords, float fragmentDepth, float filterRadius, sampler2DShadow shadowMapCompare, vec3 normal, vec3 lightDirection, float samplerRotationValue)
{
    float sum = 0;
    float bias = max(0.001 * (1.0 - dot(normal, lightDirection)), 0.0001);
    mat2 randomRotationMatrix = MatrixFromAngle2D(samplerRotationValue);

    for (int i = 0; i < POISSON_SAMPLE_COUNT; i++)
    {
        vec2 offset = randomRotationMatrix * g_PoissonSamples[i] * filterRadius;
        sum += texture(shadowMapCompare, vec3(shadowMapCoords + offset, fragmentDepth - bias - length(offset)));
    }

    return sum / POISSON_SAMPLE_COUNT;
}

const float LIGHT_SIZE = 0.006;
// Percentage closer soft shadows
float PCSS(sampler2DShadow shadowMapCompare, sampler2D shadowMap, vec3 shadowSpaceFragPosition, vec3 normal, vec3 lightDirection, float samplerRotationValue)
{
    vec2 shadowMapCoords = shadowSpaceFragPosition.xy * 0.5 + 0.5;
    shadowMapCoords.y = 1 - shadowMapCoords.y;
    float fragDepth = min(1-shadowSpaceFragPosition.z, 1);
    float bias = max(0.001 * (1.0 - dot(normal, lightDirection)), 0.0001);
    mat2 randomRotationMatrix = MatrixFromAngle2D(samplerRotationValue);

    // Calculate avg blocker depth
    float blockerSum = 0;
    float blockerCount = 0;

    float blockerSearchRadius = LIGHT_SIZE;// * (fragDepth - 0.01) / fragDepth;

    for (int i = 0; i < POISSON_SAMPLE_COUNT; i++)
    {
        vec2 offset = randomRotationMatrix * g_PoissonSamples[i] * blockerSearchRadius;
        float shadowMapDepth = texture(shadowMap, shadowMapCoords + offset).r;
        if (shadowMapDepth <= fragDepth - bias - length(offset))
        {
            blockerCount += 1;
            blockerSum += shadowMapDepth;
        }
    }

    // Early out if no blockers
    if (blockerCount < 1)
        return 1;

    float avgBlockerDepth = blockerSum / blockerCount;

    // Calculate filter radius
    float penumbraRatio = (fragDepth - avgBlockerDepth) / avgBlockerDepth;
    float filterRadius = penumbraRatio * LIGHT_SIZE / fragDepth;

    // Calculate final shadow value with pcf
    float pcfSum = 0;

    for (int i = 0; i < POISSON_SAMPLE_COUNT; i++)
    {
        vec2 offset = randomRotationMatrix * g_PoissonSamples[i] * filterRadius;
        pcfSum += texture(shadowMapCompare, vec3(shadowMapCoords + offset, fragDepth - bias - length(offset)));
    }

    return pcfSum / POISSON_SAMPLE_COUNT;
}
