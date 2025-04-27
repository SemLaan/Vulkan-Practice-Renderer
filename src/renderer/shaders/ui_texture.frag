#version 450

#include "defines.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 texCoord;


layout(BIND 1) uniform sampler2D tex;

layout(BIND 2) uniform UniformBufferObject
{
	float glyphThresholdSize;
    float zNear;
    float zFar;
} ubo;

// Only use if the projection is perspective because ortho is already linear
float linearize_depth(float d,float zNear,float zFar)
{
    return 1/((1-zFar/zNear)*d+(zFar/zNear));
}

void main()
{
    //float rawDepth = texture(tex, vec2(texCoord.x, 1-texCoord.y)).r;
    //float linearDepth = linearize_depth(rawDepth, ubo.zNear, ubo.zFar);
    //float depthColor = 1 - rawDepth;
    //outColor = vec4(depthColor, depthColor, depthColor, 1);

	float sdfValue = texture(tex, texCoord).x;

	float halfThresholdSize = ubo.glyphThresholdSize / 2.0;
	float thresholdStart = 0.5 - halfThresholdSize;
	float thresholdProgress = (sdfValue - thresholdStart) / ubo.glyphThresholdSize;
	vec4 colorValue = vec4(clamp(mix(1, 0, thresholdProgress), 0, 1));

	//vec4 colorValue = vec4(0, 0, 0, 0);

	//if (sdfValue < 0.50)
	//	colorValue = vec4(1, 1, 1, 1);

	outColor = colorValue;
	//outColor = vec4(texture(tex, texCoord).xxx, 1);
}