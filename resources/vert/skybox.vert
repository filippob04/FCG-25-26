#version 410 core

layout (location = 0) in vec3 pos;

uniform mat4 gWVP;
uniform mat4 model;

out vec3 TexCoord0; // vector that points to a specific pixel

void main() {
    vec4 centered_pos = model * vec4(pos, 1.0);

    vec4 WVP_Pos = gWVP * centered_pos;
    gl_Position = WVP_Pos.xyww;
    
    TexCoord0 = centered_pos.xyz;
}