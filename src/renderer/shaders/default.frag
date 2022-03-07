#version 460

layout (location = 0) out vec4 out_color;

void main()
{
    vec3 color = vec3(1.0, 0.0, 0.0);
    out_color = vec4(color, 1.0);
}