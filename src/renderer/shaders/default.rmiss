#version 460
#extension GL_EXT_ray_tracing : enable

struct HitPayload
{
	vec3 radiance;
	vec3 reflected_ratio; // Ratio of light reflected to camera.
	uint depth;           // Needed for random seed.
	uint sample_number;   // Needed for random seed.
	uint done;
	vec3 ray_origin;
	vec3 ray_direction;
};

layout(location = 0) rayPayloadInEXT HitPayload payload;

void main()
{
	float light_intensity = 1.0;
	vec3 light_color = vec3(1.0, 1.0, 1.0);

    payload.radiance += light_intensity * light_color * payload.reflected_ratio;
	payload.done = 1;
}
