#version 460

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

layout (location = 0) out vec3 out_normal;

layout (set = 0, binding = 0) uniform CameraUBO {
    mat4 projection_view;
} camera_ubo;

layout (set = 1, binding = 0) uniform RenderObjectUBO {
    mat4 transform;
} render_object_ubo;

void main()
{
    // Ignore translation component of transform when transforming normal.
    vec3 normal_transformed = mat3(render_object_ubo.transform) * normal;
    out_normal = normalize(normal_transformed.xyz);

    gl_Position = camera_ubo.projection_view * render_object_ubo.transform * vec4(position, 1.0);
}
