#version 460

layout (location = 0) in float out_mass;
layout (location = 1) in float out_mu;
layout (location = 2) in float out_lambda;
layout (location = 3) in vec3 out_position;
layout (location = 4) in vec3 out_velocity;
layout (location = 5) in float out_j;

layout (location = 0) out vec3 out_color;

void main()
{
    out_color = vec3(1.0, 0.0, 0.0);
}
