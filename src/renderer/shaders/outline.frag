#version 450
#include "defines.glsl"



layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

layout(BIND 0) uniform sampler2D depthTex;
layout(BIND 1) uniform sampler2D colorTex;

layout(BIND 2) uniform UniformBufferObject
{
    float zNear;
    float zFar;
} ubo;

// Only use if the projection is perspective because ortho is already linear
float linearize_depth(float d,float zNear,float zFar)
{
    return 1/((1-zFar/zNear)*d+(zFar/zNear));
}


void main() {
	float rawDepth = texture(depthTex, texCoord).r;
    float linearDepth = linearize_depth(rawDepth, ubo.zNear, ubo.zFar);
    float depthColor = 1 - rawDepth;
    outColor = vec4(depthColor, depthColor, depthColor, 1);
    //outColor = vec4(texture(colorTex, texCoord).rgb, 1);
	//outColor = vec4(texCoord, 0, 1);
}

