#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

layout(location = 0) rayPayloadInEXT HitPayload payload;

void main()
{
	float light_intensity = 0.8;
	vec3 light_color = vec3(1.0, 1.0, 1.0);

    payload.radiance += light_intensity * light_color * payload.reflected_ratio;
	payload.done = 1;
}
