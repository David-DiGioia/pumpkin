#version 460
#extension GL_EXT_ray_tracing : enable

struct HitPayload
{
	vec3 radiance;
	uint depth;         // Needed for random seed.
	uint sample_number; // Needed for random seed.
	uint done;
	vec3 ray_origin;
	vec3 ray_direction;
};

layout(location = 0) rayPayloadInEXT HitPayload payload;

void main()
{
    payload.radiance = vec3(0.5, 0.0, 0.2);
	payload.done = 1;
}
