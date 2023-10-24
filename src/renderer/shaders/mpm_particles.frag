#version 460

layout (location = 0) in float in_mass;
layout (location = 1) in float in_mu;
layout (location = 2) in float in_lambda;
layout (location = 3) in vec3 in_position;
layout (location = 4) in vec3 in_velocity;
layout (location = 5) in float in_j;

layout (location = 0) out vec3 out_color;

layout (push_constant) uniform PushConstant {
    uint particle_color_mode;
    float max_value;
} constants;

layout(early_fragment_tests) in;

const uint COLOR_MODE_NONE               = 0;
const uint COLOR_MODE_MASS               = 1;
const uint COLOR_MODE_MU                 = 2;
const uint COLOR_MODE_LAMBDA             = 3;
const uint COLOR_MODE_VELOCITY           = 4;
const uint COLOR_MODE_COMPRESSIVE_STRAIN = 5;
const uint COLOR_MODE_TENSILE_STRAIN     = 6;

const float PI = 3.14159265359;

vec3 Heatmap(float val, float lower, float upper)
{
    val = clamp(val, lower, upper);
    // Map val from [lower, upper] to [0, 1].
    val = (val - lower) / upper - lower;
    // Map val from [0, 1] to [0, pi / 2].
    val *= PI / 2.0;

    return vec3(sin(val), sin(val * 2.0), cos(val));
}

void main()
{
    switch (constants.particle_color_mode)
    {
    case COLOR_MODE_NONE:
        discard;
        break;
    case COLOR_MODE_MASS:
        out_color = Heatmap(in_mass, 0.0, constants.max_value);
        break;
    case COLOR_MODE_MU:
        out_color = Heatmap(in_mu, 0.0, constants.max_value);
        break;
    case COLOR_MODE_LAMBDA:
        out_color = Heatmap(in_lambda, 0.0, constants.max_value);
        break;
    case COLOR_MODE_VELOCITY:
        out_color = abs(in_velocity / constants.max_value);
        break;
    case COLOR_MODE_COMPRESSIVE_STRAIN:
        out_color = Heatmap(1.0 - in_j, 0.0, 1.0);
        break;
    case COLOR_MODE_TENSILE_STRAIN:
        out_color = Heatmap(in_j - 1.0, 0.0, constants.max_value);
        break;
    }
}
