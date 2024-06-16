#version 460

// Vertex attributes.
layout (location = 0) in vec3 vertex_position;

// Instance attributes.
layout (location = 1) in vec3 position;
layout (location = 2) in vec3 predicted_position;
layout (location = 3) in vec3 velocity;
layout (location = 4) in vec3 debug_color;
layout (location = 5) in float inverse_mass;

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec3 out_velocity;
layout (location = 2) out vec3 out_debug_color;
layout (location = 3) out float out_mass;

layout (set = 0, binding = 0) uniform CameraUBO {
    mat4 projection_view;
} camera_ubo;

layout (set = 1, binding = 0) uniform RenderObjectUBO {
    mat4 transform;
} render_object_ubo;

void main()
{
    vec3 final_position = position + vertex_position;
    out_position = final_position;
    out_velocity = velocity;
    out_debug_color = debug_color;
    out_mass = 1.0 / inverse_mass;
    gl_Position = camera_ubo.projection_view * render_object_ubo.transform * vec4(final_position, 1.0);
}
