#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 normal;
layout(location = 1) in vec2 texCoord;

//layout(set = 0, binding = 1) uniform sampler2D textures[100];

void main() {

    float light = clamp(dot(normalize(normal), vec3(1, 0, 0)), 0, 1);   // directional light
    light += 0.1;                                                       // Ambient light
    outColor = vec4(light, light, light, 1);
}