#version 460

layout (location = 0) in float mass;
layout (location = 1) in float mu;
layout (location = 2) in float lambda;
layout (location = 3) in vec3 position;
layout (location = 4) in vec3 velocity;
layout (location = 5) in mat3 deformation_gradient;

layout (location = 0) out float out_mass;
layout (location = 1) out float out_mu;
layout (location = 2) out float out_lambda;
layout (location = 3) out vec3 out_position;
layout (location = 4) out vec3 out_velocity;
layout (location = 5) out float out_j;

layout (set = 0, binding = 0) uniform CameraUBO {
    mat4 projection_view;
} camera_ubo;

layout (set = 1, binding = 0) uniform RenderObjectUBO {
    mat4 transform;
} render_object_ubo;

void main()
{
    out_mass = mass;
    out_mu = mu;
    out_lambda = lambda;
    out_position = position;
    out_velocity = velocity;
    out_j = determinant(deformation_gradient);
    gl_Position = camera_ubo.projection_view * render_object_ubo.transform * vec4(position, 1.0);
}
