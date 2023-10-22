#version 460

layout (location = 0) in float in_mass;
layout (location = 1) in float in_mu;
layout (location = 2) in float in_lambda;
layout (location = 3) in vec3 in_position;
layout (location = 4) in vec3 in_velocity;
layout (location = 5) in float in_j;

layout (location = 0) out vec3 out_color;

void main()
{
    out_color = vec3(in_j);
}
