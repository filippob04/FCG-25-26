#version 410 core

layout (location = 0) in vec3 pos;

uniform mat4 gWVP;
uniform mat4 model;

out vec3 texcoord; // vector that points to a specific pixel

void main() {
    vec4 p = model * vec4(pos, 1.0);

    gl_Position = (gWVP * p).xyww;
    
    texcoord = p.xyz;
}