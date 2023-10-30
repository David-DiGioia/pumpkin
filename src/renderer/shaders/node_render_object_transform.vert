#version 460

// Vertex attributes.
layout (location = 0) in vec3 vertex_position;

// Instance attributes.
layout (location = 1) in float mass;
layout (location = 2) in vec3 position;
layout (location = 3) in vec3 velocity;
layout (location = 4) in vec3 momentum;
layout (location = 5) in vec3 force;

layout (location = 0) out vec3 out_color;

layout (set = 0, binding = 0) uniform CameraUBO {
    mat4 projection_view;
} camera_ubo;

layout (set = 1, binding = 0) uniform RenderObjectUBO {
    mat4 transform;
} render_object_ubo;

layout (push_constant) uniform PushConstant {
    uint node_color_mode;
    float max_value;
} constants;

const uint COLOR_MODE_NONE     = 0;
const uint COLOR_MODE_MASS     = 1;
const uint COLOR_MODE_VELOCITY = 2;
const uint COLOR_MODE_MOMENTUM = 3;
const uint COLOR_MODE_FORCE    = 4;

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
    vec3 final_position;

    switch(constants.node_color_mode)
    {
    case COLOR_MODE_NONE:
        // We should never reach this case since the shader won't be invoked.
        break;
    case COLOR_MODE_MASS:
        // TODO: Probably will use this same shader, except with cube geometry.
        break;
    case COLOR_MODE_VELOCITY:
        // The vertex_position.x is either 0.0 or 1.0 for the two line vertices.
        final_position = position + vertex_position.x * velocity / constants.max_value;
        out_color = Heatmap(length(velocity), 0.0, constants.max_value);
        break;
    case COLOR_MODE_MOMENTUM:
        final_position = position + vertex_position.x * momentum / constants.max_value;
        out_color = Heatmap(length(momentum), 0.0, constants.max_value);
        break;
    case COLOR_MODE_FORCE:
        final_position = position + vertex_position.x * force / constants.max_value;
        out_color = Heatmap(length(force), 0.0, constants.max_value);
        break;
    }

    gl_Position = camera_ubo.projection_view * render_object_ubo.transform * vec4(final_position, 1.0);
}
