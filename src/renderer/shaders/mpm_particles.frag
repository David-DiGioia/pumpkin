#version 460

layout (location = 0) in float in_mass;
layout (location = 1) in float in_mu;
layout (location = 2) in float in_lambda;
layout (location = 3) in vec3 in_position;
layout (location = 4) in vec3 in_velocity;
layout (location = 5) in float in_j;

layout (location = 0) out vec3 out_color;

const float PI = 3.14159265359;

vec3 Heatmap(float val, float lower, float upper)
{
    // Map val from [lower, upper] to [0, 1].
    val = (val - lower) / upper - lower;
    // Map val from [0, 1] to [0, pi / 2].
    val *= PI / 2.0;

    return vec3(sin(val), sin(val * 2.0), cos(val));
}

void main()
{
    //out_color = Heatmap(1.0 - in_j, 0.0, 1.0);
    //out_color = in_velocity;
}
