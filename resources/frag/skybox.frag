#version 410 core 

in vec3 texcoord;

out vec4 FragColor;

uniform samplerCube gCubemapTexture;

void main() {
    FragColor = texture(gCubemapTexture, texcoord);
}