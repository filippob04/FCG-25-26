#version 410 core

uniform vec3 camera_pos;

struct Light {
    vec3 direct_pos;
    vec3 direct_val;
    vec3 ambient_val;
};
uniform Light light;

struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};
uniform Material material;

uniform sampler2D blend_map;
uniform sampler2D texture0;
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;

uniform float tiling_factor;

in vec3 interpolated_pos;
in vec3 interpolated_normal;
in vec2 texcoord;

out vec4 fragment_color;

// phong shading computation

void main()
{
    vec3 pos = interpolated_pos;
    vec3 normal = normalize(interpolated_normal);
            
    vec2 abs_uv = texcoord;

    vec2 tiled_uv = texcoord * tiling_factor; // tiled textures
    vec4 blend_data = texture(blend_map, abs_uv); // main

    // blend map
    vec4 base_color = texture(texture0, tiled_uv);
    vec4 R = texture(texture1, tiled_uv);
    vec4 G = texture(texture2, tiled_uv);
    vec4 B = texture(texture3, tiled_uv);

    vec4 res = base_color;
    res = mix (res, R, blend_data.r);
    res = mix (res, G, blend_data.g);
    res = mix (res, B, blend_data.b);
    
    // Ambient
    vec3 ambient = res.rgb * light.ambient_val;

    // Diffuse
    vec3 light_dir = normalize (light.direct_pos - pos);
    float diff = max (dot (normal, light_dir), 0.0);
    vec3 diffuse = res.rgb * diff * light.direct_val;

    // Specular
    vec3 view_dir = normalize (camera_pos - pos);
    vec3 reflect_dir = reflect (-light_dir, normal);
    float spec = pow (max (dot (view_dir, reflect_dir), 0.0), material.shininess);

    vec3 specular = material.specular * spec * light.direct_val;

    // final color
    fragment_color = vec4 (ambient + diffuse + specular, 1.0);
}
