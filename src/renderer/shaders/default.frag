#version 460

layout (location = 0) in vec3 normal;

layout (location = 0) out vec4 out_color;

void main()
{
    vec3 light_dir = normalize(vec3(3.0, -10.0, 0.0));
    vec3 color = vec3(0.7, 0.0, 0.0);
    float lightness = dot(-light_dir, normal);
    lightness = clamp(lightness, 0.0, 1.0);

    out_color = vec4(color * lightness, 1.0);
}