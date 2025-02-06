#version 450
#include "defines.glsl"



layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(BIND 0) uniform sampler2D depthTex;
layout(BIND 1) uniform sampler2D normalTex;

layout(BIND 2) uniform UniformBufferObject
{
    float zNear;
    float zFar;
	float normalEdgeThreshold;
	int screenWidth;
	int screenHeight;
} ubo;

// Only use if the projection is perspective because ortho is already linear
float linearize_depth(float d,float zNear,float zFar)
{
    return 1/((1-zFar/zNear)*d+(zFar/zNear));
}


void main() {
	float onePixelX = 1.0 / float(ubo.screenWidth);
	float onePixelY = 1.0 / float(ubo.screenHeight);

	vec2 topLeftSampleCoordinate = vec2(texCoord.x - onePixelX, texCoord.y - onePixelY);
	vec2 bottomLeftSampleCoordinate = vec2(texCoord.x - onePixelX, texCoord.y + onePixelY);
	vec2 topRightSampleCoordinate = vec2(texCoord.x + onePixelX, texCoord.y - onePixelY);
	vec2 bottomRightSampleCoordinate = vec2(texCoord.x + onePixelX, texCoord.y + onePixelY);
 
	vec3 topLeftNormalSample = texture(normalTex, topLeftSampleCoordinate).rgb;
	vec3 bottomLeftNormalSample = texture(normalTex, bottomLeftSampleCoordinate).rgb;
	vec3 topRightNormalSample = texture(normalTex, topRightSampleCoordinate).rgb;
	vec3 bottomRightNormalSample = texture(normalTex, bottomRightSampleCoordinate).rgb;

	float normalDelta = length(topLeftNormalSample - bottomRightNormalSample) + length(topRightNormalSample - bottomLeftNormalSample);
	float normalEdgeValue = step(ubo.normalEdgeThreshold, normalDelta);

    outColor = vec4(1, 1, 1, normalEdgeValue);


	//float rawDepth = texture(depthTex, texCoord).r;
    //float linearDepth = linearize_depth(rawDepth, ubo.zNear, ubo.zFar);
    //float depthColor = 1 - rawDepth;
    //outColor = vec4(texture(normalTex, texCoord).rgb, 1);
	//outColor = vec4(texCoord, 0, 1);
}

