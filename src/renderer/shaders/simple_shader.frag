#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 texCoord;

layout(set = 1, binding = 1) uniform UniformBufferObject
{
    vec4 color;
} ubo;

layout(set = 1, binding = 2) uniform sampler2D texSampler;

void main() {

    float light = clamp(dot(normalize(normal), vec3(1, 0, 0)), 0, 1);   // directional light
    light += 0.1;                                                       // Ambient light
    outColor = ubo.color * texture(texSampler, texCoord) * light;
}