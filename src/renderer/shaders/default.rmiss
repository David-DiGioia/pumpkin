#version 460
#extension GL_EXT_ray_tracing : enable

struct HitPayload
{
	vec3 radiance;
	vec3 attenuation;
	int  done;
	vec3 ray_origin;
	vec3 ray_direction;
};

layout(location = 0) rayPayloadInEXT HitPayload payload;

void main()
{
    payload.radiance = vec3(0.0, 0.0, 0.2);
	payload.done = 1;
}
