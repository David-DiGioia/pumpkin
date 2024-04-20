#version 460

// Vertex attributes.
layout (location = 0) in vec3 vertex_position;

// Instance attributes.
layout (location = 1) in vec3 position;
layout (location = 2) in vec3 normal;

layout (location = 0) out vec3 out_color;

layout (set = 0, binding = 0) uniform CameraUBO {
    mat4 projection_view;
} camera_ubo;

const float normal_length = 0.01;

void main()
{
    out_color = abs(normal);
    vec3 final_position = position + vertex_position.x * normal * normal_length;
    //gl_Position = camera_ubo.projection_view * render_object_ubo.transform * vec4(final_position, 1.0);
    gl_Position = camera_ubo.projection_view * vec4(final_position, 1.0);
}
