#version 450
#include "defines.glsl"

layout(location = 0) in vec4 f_color;

layout(location = 0) out vec4 outColor;


layout(BIND 1) uniform UniformBufferObject
{
    vec4 color;
} ubo;


void main() {
    outColor = f_color;
}