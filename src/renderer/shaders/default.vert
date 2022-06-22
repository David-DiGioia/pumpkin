#version 460

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;

layout (location = 0) out vec3 out_normal;

layout (set = 0, binding = 0) uniform CameraUBO {
    mat4 view_projection;
} camera_ubo;

layout (set = 1, binding = 0) uniform RenderObjectUBO {
    mat4 transform;
} render_object_ubo;

void main()
{
    out_normal = normal;
    gl_Position = camera_ubo.view_projection * render_object_ubo.transform * vec4(position, 1.0);
}
