
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

