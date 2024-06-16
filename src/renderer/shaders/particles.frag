#version 460

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_velocity;
layout (location = 2) in vec3 in_debug_color;
layout (location = 3) in float in_mass;

layout (location = 0) out vec4 out_color;

layout (push_constant) uniform PushConstant {
    uint particle_color_mode;
    float max_value;
} constants;

// This makes the depth test happen earlier before we discard fragments for COLOR_MODE_NONE.
layout(early_fragment_tests) in;

const uint COLOR_MODE_FINAL_SHADING = 0;
const uint COLOR_MODE_HIDDEN        = 1;
const uint COLOR_MODE_MASS          = 2;
const uint COLOR_MODE_VELOCITY      = 3;
const uint COLOR_MODE_DEBUG_COLOR   = 4;

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
    case COLOR_MODE_VELOCITY:
        out_color = vec4(abs(in_velocity / constants.max_value), 1.0);
        break;
    case COLOR_MODE_DEBUG_COLOR:
        out_color = vec4(in_debug_color, 1.0);
        break;
    }
}
