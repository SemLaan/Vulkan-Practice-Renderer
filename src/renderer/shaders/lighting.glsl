
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

// Calculates whether the current fragment is in shadow or not, returns 0 if in shadow, 1 if out of shadow
float HardShadow(vec3 shadowSpacePosition, vec3 normal, vec3 lightDirection, sampler2D shadowMap)
{
    vec2 shadowMapCoords = shadowSpacePosition.xy * 0.5 + 0.5;
    shadowMapCoords.y = 1 - shadowMapCoords.y;
    float bias = max(0.001 * (1.0 - dot(normal, lightDirection)), 0.0001);

    float shadow = texture(shadowMap, vec2(shadowMapCoords.x, shadowMapCoords.y)).r > min(1-shadowSpacePosition.z, 1) - bias ? 1.0 : 0.0;
    return shadow;
}

// 
float PercentageCloserFilter()
{
    return 1;
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