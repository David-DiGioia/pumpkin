#version 460
#extension GL_EXT_ray_tracing : enable

struct Rayhit
{
	uint instance_id;
	vec3 position;
};

layout(set = 0, binding = 2) buffer Rayhits { Rayhit i[]; } rayhits;

void main()
{
	rayhits.i[gl_LaunchIDEXT.x].instance_id = gl_InstanceID;
	rayhits.i[gl_LaunchIDEXT.x].position = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
}
