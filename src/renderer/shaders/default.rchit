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
	vec2 tex_coord;
};

const uint NULL_TEXTURE_INDEX = 0xFFFFFFFF;

struct Material
{
    // Backup values to use if texture index is NULL_TEXTURE_INDEX.
	vec4 color;
	float metallic;
	float roughness;
	float emission;

	// There is no corresponding texture for IOR.
	float ior;

	// Indices into textures[].
	uint color_index;
	uint metallic_index;
	uint roughness_index;
	uint emission_index;
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
layout(set = 1, binding = 3) uniform sampler2D textures[];

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

float SmithGGXMasking(float n_dot_v, float a2)
{
	float denom_c = sqrt(a2 + (1.0 - a2) * n_dot_v * n_dot_v) + n_dot_v;

	return 2.0 * n_dot_v / denom_c;
}

float SmithGGXMaskingShadowing(float n_dot_v, float n_dot_l, float a2)
{
	float denom_a = n_dot_v * sqrt(a2 + (1.0 - a2) * n_dot_l * n_dot_l);
	float denom_b = n_dot_l * sqrt(a2 + (1.0 - a2) * n_dot_v * n_dot_v);

	return 2.0 * n_dot_l * n_dot_v / (denom_a + denom_b);
}

vec3 Fresnel(float ior, float v_dot_h, float metallic, vec3 base_color)
{
	// Reflectance at normal incidence.
	float num = ior - 1.0;
	float denom = ior + 1.0;
	vec3 f0 = vec3((num * num) / (denom * denom));
	f0 = mix(f0, base_color, metallic);

	return f0 + (vec3(1.0) - f0) * pow(1.0 - v_dot_h, 5.0);
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
	vec3 fresnel = Fresnel(ior, v_dot_h, metallic, base_color);

	vec3 specular = (distribution * geometry * fresnel) / (4.0 * n_dot_l * n_dot_v);

	// Specular is colored only if metallic.
	vec3 specular_color = mix(specular, specular * base_color, metallic);

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

vec3 CookTorranceBrdfWeighted(vec3 n, vec3 v, vec3 l, vec3 base_color, float metallic, float roughness, float ior)
{
	vec3 h = normalize(v + l);
	float n_dot_v = dot(n, v);
	float n_dot_l = dot(n, l);
	float v_dot_h = dot(v, h);

	float a2 = roughness * roughness;
	float g1 = SmithGGXMasking(n_dot_v, a2);
	float g2 = SmithGGXMaskingShadowing(n_dot_v, n_dot_l, a2);
	vec3 fresnel = Fresnel(ior, v_dot_h, metallic, base_color);
	vec3 specular = fresnel * (g2 / g1);

	// Specular is colored only if metallic.
	vec3 specular_color = mix(specular, specular * base_color, metallic);

	return specular_color;
}

vec3 LambertianBrdfWeighted(vec3 base_color)
{
	vec3 rho_d = base_color;

	// Integrating over the unit hemisphere weighted by cos(theta) (weakening factor due to incident angle) is pi.
	// Integrating over the BRDF must result in 1.0, so we divide out the factor of pi.
	vec3 diffuse = rho_d / pi;
	
	return diffuse;
}

// Input Ve: view direction
// Input alpha_x, alpha_y: roughness parameters
// Input U1, U2: uniform random numbers
// Output Ne: normal sampled with PDF D_Ve(Ne) = G1(Ve) * max(0, dot(Ve, Ne)) * D(Ne) / Ve.z
vec3 SampleGgxVndf(vec3 Ve, float alpha_x, float alpha_y, float U1, float U2)
{
	// Section 3.2: transforming the view direction to the hemisphere configuration
	vec3 Vh = normalize(vec3(alpha_x * Ve.x, alpha_y * Ve.y, Ve.z));

	// Section 4.1: orthonormal basis (with special case if cross product is zero)
	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 0 ? vec3(-Vh.y, Vh.x, 0) * inversesqrt(lensq) : vec3(1,0,0);
	vec3 T2 = cross(Vh, T1);

	// Section 4.2: parameterization of the projected area
	float r = sqrt(U1);
	float phi = 2.0 * pi * U2;
	float t1 = r * cos(phi);
	float t2 = r * sin(phi);
	float s = 0.5 * (1.0 + Vh.z);
	t2 = (1.0 - s)*sqrt(1.0 - t1*t1) + s*t2;

	// Section 4.3: reprojection onto hemisphere
	vec3 Nh = t1*T1 + t2*T2 + sqrt(max(0.0, 1.0 - t1*t1 - t2*t2))*Vh;

	// Section 3.4: transforming the normal back to the ellipsoid configuration
	vec3 Ne = normalize(vec3(alpha_x * Nh.x, alpha_y * Nh.y, max(0.0, Nh.z)));
	return Ne;
}

// In tangent space (with z up).
vec3 SampleCosineWeighted(float r0, float r1)
{
	float phi = 2 * pi * r0;
	float theta = acos(sqrt(r1));
	return vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
}

// Get matrix to convert from tangent space of triangle with normal n, to world space.
mat3 TangentToWorldMatrix(vec3 n)
{
	// Want to create a matrix M with M: vec3(0, 0, 1) -> n. Since in the Eric Heitz paper, the hemisphere is truncated along the z-axis.
	// There is still another degree of freedom which will be arbitrary, but it will matter if anisotropic materials are implemented.
	vec3 z = n;
	vec3 not_z = vec3(1, 0, 0);
	not_z = (abs(dot(z, not_z)) > 0.999) ? vec3(0, 1, 0) : not_z;

	vec3 x = normalize(cross(z, not_z));
	vec3 y = normalize(cross(x, z));

	return mat3(x, y, z);
}

vec3 CookTorranceImportanceSample(vec3 normal, vec3 v, float roughness, mat3 tangent_to_world_mat, float r0, float r1)
{
	mat3 world_to_tangent_mat = transpose(tangent_to_world_mat);
	vec3 wm_tangent = SampleGgxVndf(world_to_tangent_mat * v, roughness, roughness, r0, r1);
	vec3 wm = tangent_to_world_mat * wm_tangent;
	vec3 wi = reflect(gl_WorldRayDirectionEXT, wm);
	return wi;
}

vec3 LambertianImportanceSample(vec3 normal, mat3 tangent_to_world_mat, float r0, float r1)
{
	vec3 wi_tangent = SampleCosineWeighted(r0, r1);
	vec3 wi = tangent_to_world_mat * wi_tangent;
	return wi;
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

	vec3 v = -gl_WorldRayDirectionEXT;

	uint seed = Hash(uvec4(gl_LaunchIDEXT.x, gl_LaunchIDEXT.y, payload.depth, payload.sample_number));
	float r0 = FloatConstruct(seed);
	seed = Hash(seed);
	float r1 = FloatConstruct(seed);
	seed = Hash(seed);
	float r2 = FloatConstruct(seed);

	mat3 tangent_to_world_mat = TangentToWorldMatrix(normal);

	vec3 brdf_weighted = vec3(0.0);
	vec3 wi = vec3(0.0);

	// Sample textures.
	vec2 tex_coord = v0.tex_coord * barycentrics.x + v1.tex_coord * barycentrics.y + v2.tex_coord * barycentrics.z;
	vec3 color = texture(textures[mat.color_index], tex_coord).rgb;
	float metallic = texture(textures[mat.metallic_index], tex_coord).r;
	float roughness = texture(textures[mat.roughness_index], tex_coord).r;
	float emission = texture(textures[mat.emission_index], tex_coord).r;

	vec3 fresnel = Fresnel(mat.ior, max(dot(normal, v), 0.0), metallic, color);

	float diff_probability = 0.5 * (1.0 - metallic);

	if (r2 < diff_probability)
	{
		// Diffuse.
		wi = LambertianImportanceSample(normal, tangent_to_world_mat, r0, r1);
		// Without cosine importance sampling, there is a constant factor of 2 * pi, and with cosine importance sampling it's a factor of pi.
		// We ignored the constant before, so now we have to make it half the brightness.
		brdf_weighted = 0.5 * (1.0 - fresnel.x) * LambertianBrdfWeighted(color);
		brdf_weighted *= 1.0 / diff_probability;
	}
	else
	{
		// Specular.
		wi = CookTorranceImportanceSample(normal, v, roughness, tangent_to_world_mat, r0, r1);
		brdf_weighted = CookTorranceBrdfWeighted(normal, v, wi, color, metallic, roughness, mat.ior);
		brdf_weighted *= 1.0 / (1.0 - diff_probability);
	}

	// Add the amount of emission that makes it back to the camera.
	payload.radiance += emission * color * payload.reflected_ratio;

	// Depending on microfacet that ray hits, it could bounce into the face, so in such cases assume it's absorbed.
	// This restricts the outgoing wi direction to be in the upper hemisphere surrounding the normal.
	if(dot(wi, normal) <= 0.0)
	{
		payload.done = 1;
		return;
	}

	payload.reflected_ratio *= brdf_weighted;
	payload.ray_origin = position;
	payload.ray_direction = wi;

	++payload.depth;
}
