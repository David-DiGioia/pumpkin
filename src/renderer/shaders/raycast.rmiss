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
	rayhits.i[gl_LaunchIDEXT.x].instance_id = 0xFFFFFFFF;
	rayhits.i[gl_LaunchIDEXT.x].position = vec3(0.0);
}
