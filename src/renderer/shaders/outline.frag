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
	// =================================================================== Calculating sample uv's
	// Calculating sample coordinates for one pixel away (diagonally) in all directions from the current pixel
	float onePixelX = 1.0 / float(ubo.screenWidth);
	float onePixelY = 1.0 / float(ubo.screenHeight);

	vec2 topLeftSampleCoordinate = vec2(texCoord.x - onePixelX, texCoord.y - onePixelY);
	vec2 bottomLeftSampleCoordinate = vec2(texCoord.x - onePixelX, texCoord.y + onePixelY);
	vec2 topRightSampleCoordinate = vec2(texCoord.x + onePixelX, texCoord.y - onePixelY);
	vec2 bottomRightSampleCoordinate = vec2(texCoord.x + onePixelX, texCoord.y + onePixelY);
 
	// ================================================================ Normal map sampling
	// Sampling the normals around the current pixel
	vec3 topLeftNormalSample = texture(normalTex, topLeftSampleCoordinate).rgb;
	vec3 bottomLeftNormalSample = texture(normalTex, bottomLeftSampleCoordinate).rgb;
	vec3 topRightNormalSample = texture(normalTex, topRightSampleCoordinate).rgb;
	vec3 bottomRightNormalSample = texture(normalTex, bottomRightSampleCoordinate).rgb;

	// ================================================================== Depth map sampling and linearization
	float topLeftDepthSample = texture(depthTex, topLeftSampleCoordinate).r;
	float bottomLeftDepthSample = texture(depthTex, bottomLeftSampleCoordinate).r;
	float topRightDepthSample = texture(depthTex, topRightSampleCoordinate).r;
	float bottomRightDepthSample = texture(depthTex, bottomRightSampleCoordinate).r;
	// Linearizing depth
	float zFarOverZNear = ubo.zFar / ubo.zNear;
	float oneMinusZFarOverZNear = 1 - zFarOverZNear;
	topLeftDepthSample = 1 / (oneMinusZFarOverZNear * topLeftDepthSample + zFarOverZNear);
	bottomLeftDepthSample = 1 / (oneMinusZFarOverZNear * bottomLeftDepthSample + zFarOverZNear);
	topRightDepthSample = 1 / (oneMinusZFarOverZNear * topRightDepthSample + zFarOverZNear);
	bottomRightDepthSample = 1 / (oneMinusZFarOverZNear * bottomRightDepthSample + zFarOverZNear);

	float closestDepthValue = min(min(topLeftDepthSample, bottomLeftDepthSample), min(topRightDepthSample, bottomRightDepthSample));

	// ===================================================================== Calculating whether this pixel is an edge based on the normals
	// Calculating a value that represents the size of the difference between the normals
	vec3 normalDeltaLeftDiagonal = topLeftNormalSample - bottomRightNormalSample;
	vec3 normalDeltaRightDiagonal = topRightNormalSample - bottomLeftNormalSample;
	float normalDelta = sqrt(max(dot(normalDeltaLeftDiagonal, normalDeltaLeftDiagonal), dot(normalDeltaRightDiagonal, normalDeltaRightDiagonal)));

	// Calculating whether this pixel is considered an edge or not
	float normalEdgeValue = step(ubo.normalEdgeThreshold, normalDelta);


	float lineVisibility = normalEdgeValue * (1 - closestDepthValue);

    outColor = vec4(lineVisibility.rrrr);
}

