#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 texCoord;
layout(location = 2) in flat uint textureIndex;

//layout(set = 0, binding = 1) uniform sampler2D textures[100];

void main() {
    outColor = vec4(1,1,1,1);//texture(textures[textureIndex], texCoord);
}