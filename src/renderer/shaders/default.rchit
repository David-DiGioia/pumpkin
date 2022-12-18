#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

struct HitPayload
{
	vec3 radiance;
	vec3 attenuation;
	int  done;
	vec3 ray_origin;
	vec3 ray_direction;
};

struct Vertex
{
	vec3 position;
	vec3 normal;
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

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 1, binding = 0) buffer SceneDescription { ObjectBuffers i[]; } scene_description;

const float pi = 3.14159265359;

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
	return diffuse * (vec3(1.0) - fresnel) + specular_color;
}

void main()
{
	// Custom index is used to store index to device address of mesh data.
	ObjectBuffers object_resource = scene_description.i[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT];

	// Cast the uint64_t buffer addresses (from vkGetDeviceAddress()) to the buffer references declared above.
	Vertices vertices = Vertices(object_resource.vertices);
	Indices indices = Indices(object_resource.indices);

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

	vec3 base_color = vec3(0.7, 0.2, 0.1);
	vec3 light_color = vec3(1.0);
	float metallic = 0.0;
	float roughness = 0.3;
	float ior = 1.53;
	vec3 emission = vec3(0.0);

	// Integrate over hemisphere.
	vec3 sum = vec3(0.0);
	for (int i = 0; i < 1; ++i)
	{
		// Hardcoded (to) light direction.
		vec3 light_direction = normalize(vec3(1, 1, 1));
		vec3 brdf = CookTorranceBrdf(normal, -gl_WorldRayDirectionEXT, light_direction, base_color, metallic, roughness, ior);
		float n_dot_l = dot(normal, light_direction);
		
		float light_intensity = 0.0;

		// Shadow ray.
		if (n_dot_l > 0)
		{
			uint  flags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
			is_shadowed   = true;

			traceRayEXT(tlas,            // topLevel
						flags,           // rayFlags
						0xFF,            // cullMask
						0,               // sbtRecordOffset
						0,               // sbtRecordStride
						1,               // missIndex
						position,        // origin
						0.001,           // Tmin
						light_direction, // direction
						1e32,            // Tmax
						1);              // payload

			if (!is_shadowed) {
				light_intensity = 1.0;
			}
		}

		sum += brdf * light_color * light_intensity * n_dot_l;
	}
	vec3 reflected_radiance = sum / 1.0; // Divide by number of samples in the integration.

	payload.radiance = emission + reflected_radiance;
	payload.ray_origin = position;
	payload.ray_direction = reflect(gl_WorldRayDirectionEXT, normal);
}
