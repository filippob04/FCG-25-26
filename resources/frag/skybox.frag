#version 410 core 

in vec3 texcoord;

out vec4 FragColor;

uniform samplerCube gCubemapTexture;

void main() {

    vec3 sky_color = texture(gCubemapTexture, texcoord).rgb;
    vec3 fog_color = vec3(0.6, 0.7, 0.8);

    float mix_factor = smoothstep(-0.05, 0.1, texcoord.y); // smooth transition between [-1,1]
    vec3 final_color = mix(fog_color, sky_color, mix_factor);

    FragColor = vec4(final_color, 1.0);
}