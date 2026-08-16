#version 410 core 

in vec2 texcoord;

out vec4 FragColor;

uniform sampler2D hudTexture;

void main() {
    FragColor = texture(hudTexture, texcoord);
}