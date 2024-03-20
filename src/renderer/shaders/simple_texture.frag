#version 450

layout(location = 0) out vec4 outColor;

layout(location = 1) in vec2 texCoord;

layout(set = 0, binding = 0) uniform GlobalUniformBufferObject
{
	mat4 projView;
    vec3 viewPosition;
	vec3 directionalLight;
} globalubo;

layout(set = 1, binding = 0) uniform sampler2D tex;

void main() 
{
    float depth = (1 - texture(tex, texCoord).r) * 255;
    //outColor = vec4(depth, depth, depth, 1);
    outColor = texture(tex, texCoord);
}