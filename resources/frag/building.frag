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

uniform sampler2D buildingTexture;

in vec3 interpolated_pos;

out vec4 fragment_color;

// clamping a vec4 
vec4 clamp4 (vec4 v)
{
    return clamp (v, vec4 (0.0), vec4 (1.0));
}

void main()
{
    vec3 pos = interpolated_pos;

    vec3 dx = dFdx (pos);
    vec3 dy = dFdy (pos);
    vec3 normal = normalize (cross (dx, dy));
            
    vec2 building_uv;

    building_uv.x = (pos.x + pos.z) * 0.5;
    building_uv.y = pos.y * -0.5;

    vec4 res = texture(buildingTexture, building_uv);
    
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
    fragment_color = clamp4 (fragment_color);
}

