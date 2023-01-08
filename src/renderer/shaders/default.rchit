#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_GOOGLE_include_directive : enable

#include "common.glsl"

struct Vertex
{
	vec3 position;
	vec3 normal;
};

struct Material
{
	vec4 color;
	float metallic;
	float roughness;
	float ior;
	float emission;
};

struct ObjectBuffers
{
	uint64_t vertices;
	uint64_t indices;
};

hitAttributeEXT vec3 attribs;

layout(location = 0) rayPayloadInEXT HitPayload payload;
layout(location = 1) rayPayloadEXT bool is_shadowed;

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices { uvec3 i[]; };
layout(buffer_reference, scalar) buffer MaterialIndices { uint i[]; };

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 1, binding = 0) buffer SceneDescription { ObjectBuffers i[]; } scene_description;
layout(set = 1, binding = 1) buffer Materials { Material i[]; } materials;
layout(set = 1, binding = 2) buffer MaterialIndexBuffers { uint64_t i[]; } material_index_buffers; // Addresses to material index buffers. One buffer for each Vulkan instance in the order of the geometries it contains.

float NormalDistributionGgx(float n_dot_h, float roughness)
{
	float alpha = roughness * roughness;
	float alpha_squared = alpha * alpha;
	float denom = n_dot_h * n_dot_h * (alpha_squared - 1.0) + 1.0;

	return alpha_squared / (pi * denom * denom);
}

float GeometricAttenuation(float n_dot_h, float n_dot_v, float n_dot_l, float v_dot_h)
{
	float occlusion = (2.0 * n_dot_h * n_dot_v) / v_dot_h;
	float shadowing = (2.0 * n_dot_h * n_dot_v) / v_dot_h;
	return min(1.0, min(occlusion, shadowing));
}

float Fresnel(float ior, float v_dot_h)
{
	// Reflectance at normal incidence.
	float num = ior - 1.0;
	float denom = ior + 1.0;
	float f0 = (num * num) / (denom * denom);

	return f0 + (1.0 - f0) * pow(1.0 - v_dot_h, 5.0);
}

vec3 CookTorranceBrdf(vec3 n, vec3 v, vec3 l, vec3 base_color, float metallic, float roughness, float ior)
{
	vec3 h = normalize(v + l);
	float n_dot_h = dot(n, h);
	float n_dot_v = dot(n, v);
	float n_dot_l = dot(n, l);
	float v_dot_h = dot(v, h);

	float distribution = NormalDistributionGgx(n_dot_h, roughness);
	float geometry = GeometricAttenuation(n_dot_h, n_dot_v, n_dot_l, v_dot_h);
	float fresnel = Fresnel(ior, v_dot_h);

	float epsilon = 0.0001;
	float specular = (distribution * geometry * fresnel) / (4.0 * n_dot_l * n_dot_v);

	// Specular is colored only if metallic.
	vec3 specular_color = mix(vec3(specular), specular * base_color, metallic);

	vec3 rho_d = base_color;

	// There is no diffuse reflectance for metals.
	rho_d *= (1.0 - metallic);

	// Integrating over the unit hemisphere weighted by cos(theta) (weakening factor due to incident angle) is pi.
	// Integrating over the BRDF must result in 1.0, so we divide out the factor of pi.
	vec3 diffuse = rho_d / pi;
	
	// Conserve energy by only having diffuse color where light is not reflected by fresnel.
	// (Not sure why distribution is not taken into account).
	return n_dot_l * (diffuse * (vec3(1.0) - fresnel) + specular_color);
}

void main()
{
	// Custom index is used to store index to device address of mesh data.
	ObjectBuffers object_resource = scene_description.i[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];
	uint64_t material_indices_address = material_index_buffers.i[gl_InstanceID];

	// Cast the uint64_t buffer addresses (from vkGetDeviceAddress()) to the buffer references declared above.
	Vertices vertices = Vertices(object_resource.vertices);
	Indices indices = Indices(object_resource.indices);
	MaterialIndices material_indices = MaterialIndices(material_indices_address);

	uint material_index = material_indices.i[gl_GeometryIndexEXT];
	Material mat = materials.i[material_index];

	// Indices of the triangle.
	uvec3 ind = indices.i[gl_PrimitiveID];

	// Vertices of the triangle.
	Vertex v0 = vertices.v[ind.x];
	Vertex v1 = vertices.v[ind.y];
	Vertex v2 = vertices.v[ind.z];

	// Barcentric coordinates of the triangle.
	const vec3 barycentrics = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);

	// Compute the normal at hit position.
	vec3 normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
	normal = normalize(gl_ObjectToWorldEXT * vec4(normal, 0.0)); // Transform the normal to world space. w = 0 to ignore position information.
	
	// Flip the normal around if it's a backfacing triangle.
	if (gl_HitKindEXT == gl_HitKindBackFacingTriangleEXT) {
		normal *= -1;
	}

	// Compute the hit position.
	vec3 position = v0.position * barycentrics.x + v1.position * barycentrics.y + v2.position * barycentrics.z;
	position = gl_ObjectToWorldEXT * vec4(position, 1.0); // Transform the position to world space.

	uint seed = Hash(uvec4(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, payload.depth, payload.sample_number));
	payload.ray_direction = RandomPointOnUnitHemiSphere(seed, normal);

	vec3 brdf = CookTorranceBrdf(normal, -gl_WorldRayDirectionEXT, payload.ray_direction, mat.color.xyz, mat.metallic, mat.roughness, mat.ior);
		
	// Add the amount of emission that makes it back to the camera.
	payload.radiance += mat.emission * mat.color.xyz * payload.reflected_ratio;
	payload.reflected_ratio *= brdf;
	payload.ray_origin = position;

	++payload.depth;
}
