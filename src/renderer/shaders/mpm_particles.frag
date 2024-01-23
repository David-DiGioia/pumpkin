#version 460

layout (location = 0) in float in_mass;
layout (location = 1) in float in_mu;
layout (location = 2) in float in_lambda;
layout (location = 3) in vec3 in_position;
layout (location = 4) in vec3 in_velocity;
layout (location = 5) in float in_je;
layout (location = 6) in float in_jp;

layout (location = 0) out vec4 out_color;

layout (push_constant) uniform PushConstant {
    uint particle_color_mode;
    float max_value;
} constants;

// This makes the depth test happen earlier before we discard fragments for COLOR_MODE_NONE.
layout(early_fragment_tests) in;

const uint COLOR_MODE_FINAL_SHADING       = 0;
const uint COLOR_MODE_HIDDEN              = 1;
const uint COLOR_MODE_MASS                = 2;
const uint COLOR_MODE_MU                  = 3;
const uint COLOR_MODE_LAMBDA              = 4;
const uint COLOR_MODE_VELOCITY            = 5;
const uint COLOR_MODE_ELASTIC_COMPRESSIVE = 6;
const uint COLOR_MODE_ELASTIC_TENSILE     = 7;
const uint COLOR_MODE_PLASTIC_COMPRESSIVE = 8;
const uint COLOR_MODE_PLASTIC_TENSILE     = 9;

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
    case COLOR_MODE_FINAL_SHADING:
    case COLOR_MODE_HIDDEN:
        discard;
        break;
    case COLOR_MODE_MASS:
        out_color = vec4(Heatmap(in_mass, 0.0, constants.max_value), 1.0);
        break;
    case COLOR_MODE_MU:
        out_color = vec4(Heatmap(in_mu, 0.0, constants.max_value), 1.0);
        break;
    case COLOR_MODE_LAMBDA:
        out_color = vec4(Heatmap(in_lambda, 0.0, constants.max_value), 1.0);
        break;
    case COLOR_MODE_VELOCITY:
        out_color = vec4(abs(in_velocity / constants.max_value), 1.0);
        break;
    case COLOR_MODE_ELASTIC_COMPRESSIVE:
        out_color = vec4(Heatmap(1.0 - in_je, 0.0, constants.max_value), 1.0);
        break;
    case COLOR_MODE_ELASTIC_TENSILE:
        out_color = vec4(Heatmap(in_je - 1.0, 0.0, constants.max_value), 1.0);
        break;
    case COLOR_MODE_PLASTIC_COMPRESSIVE:
        out_color = vec4(Heatmap(1.0 - in_jp, 0.0, constants.max_value), 1.0);
        break;
    case COLOR_MODE_PLASTIC_TENSILE:
        out_color = vec4(Heatmap(in_jp - 1.0, 0.0, constants.max_value), 1.0);
        break;
    }
}
