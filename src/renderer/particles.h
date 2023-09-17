#pragma once

#include "glm/glm.hpp"

#include "descriptor_set.h"
#include "memory_allocator.h"

namespace renderer
{
	constexpr uint32_t PARTICLE_CHUNK_SIZE{64};  // Total size of particle chunk dimension.
	constexpr uint32_t PARTICLE_GROUP_COUNT{16}; // Number of workgroups in each dimension.

	enum class ParticleType : uint8_t
	{
		EMPTY,
		STONE,
	};

	// Encodes whether each of the 6 particle neighbors are occupied or not.
	enum class ParticleSidesFlagBits : uint8_t
	{
		X_POSITIVE = 0x01,
		X_NEGATIVE = 0x02,
		Y_POSITIVE = 0x04,
		Y_NEGATIVE = 0x08,
		Z_POSITIVE = 0x10,
		Z_NEGATIVE = 0x20,
		ALL_SIDES  = 0x3F,
	};

	// Particles that are not being simulated.
	struct StaticParticle
	{
		ParticleType type;
	};

	// Particles that are actively being simulated.
	struct Particle
	{
		glm::vec3 position;
		int geometry_index;
	};

	struct ParticleGenShaderResources
	{
		struct BuiltInUBO
		{
			glm::uvec3 chunk_coordinate;
		};

		DescriptorSetLayoutResource layout_resource;   // Layout resource for user-defined particle shaders.
		DescriptorSetResource descriptor_set_resource; // Descriptor set resource for user-defined particle gen shader.
		BufferResource built_in_ubo_buffer;            // Built-in data for the particle gen shader.
		BufferResource custom_ubo_buffer;              // User-defined ubo buffer for the particle gen shader.
		BufferResource particle_out_buffer;            // Shader outputs particles to this buffer.
		uint32_t shader_idx;                           // Index into user_compute_shaders_.
		bool should_invoke;                            // Set this to true to invoke particle gen on next frame.
	};

	// Convert a particle 1D buffer index into a 3D coordiante in the chunk.
	glm::uvec3 ParticleIndexToCoordinate(uint32_t index);

	void GenerateStaticParticleMesh(const std::vector<StaticParticle>& particles, float particle_width);
}
