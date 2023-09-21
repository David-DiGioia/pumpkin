#pragma once

#include "glm/glm.hpp"

#include "descriptor_set.h"
#include "memory_allocator.h"
#include "mesh.h"
#include "pipeline.h"

namespace renderer
{
	class VulkanRenderer;

	constexpr uint32_t PARTICLE_CHUNK_SIZE{ 64 };  // Total size of particle chunk dimension.
	constexpr uint32_t PARTICLE_CHUNK_VOLUME{ PARTICLE_CHUNK_SIZE * PARTICLE_CHUNK_SIZE * PARTICLE_CHUNK_SIZE };
	constexpr uint32_t PARTICLE_GROUP_COUNT{ 16 }; // Number of workgroups in each dimension.

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
		ALL_SIDES = 0x3F,
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

	// Convert a particle 1D buffer index into a 3D coordiante in the chunk.
	glm::uvec3 ParticleIndexToCoordinate(uint32_t index);

	class StaticParticleMeshGenerator
	{
	public:
		Mesh* Generate(const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags, float particle_width);

	private:
		struct Rectangle
		{
			uint32_t start_x;   // Inclusive.
			uint32_t end_x;     // Inclusive.
			uint32_t start_y;   // Inclusive.
			bool trianglulated; // Whether this rectangle has been added to the triangle buffer.
		};

		void TriangulateRectangle(uint32_t rect_idx, const glm::uvec3& coord);

		// Clear indices to a rectangle in rectangles_ between rectangle's start and end.
		void ClearRectangleIndices(uint32_t rect_idx, const Rectangle& rectangle);

		// Set indices to a rectangle in rectangles_ between rectangle's start and end.
		void SetRectangleIndices(uint32_t rect_idx, const Rectangle& rectangle);

		std::vector<uint32_t> rectangle_indices_{};  // rectangle_indices[j] contains the index into x_positive_partial_rectangles which contains this coordinate in its range. Otherwise contains null index.
		std::vector<Rectangle> rectangles_{};        // The WIP rectangles that have not been triangulated yet.
		Mesh* mesh_{};                               // The output mesh.
	};

	class ParticleContext
	{
	public:
		void Initialize(Context* context, VulkanRenderer* renderer);

		void CleanUp();

		RenderObjectHandle InvokeParticleGenShader();

		void SetParticleGenShader(uint32_t shader_idx, const std::vector<std::byte>& custom_ubo_buffer);

		DescriptorSetLayoutResource& GetParticleGenLayoutResource();

	private:
		void InitializeParticleGenShaderResources();

		void InitializeParticleNeighborsShaderResources();

		// Generates triangles for each individual particle as a cube. Can be done on host or device.
		RenderObjectHandle GenerateDynamicParticleMesh(const std::vector<Particle>& particles, float particle_width);

		// Genereates fewest triangles possible as a shell around particle mass. Good for particles not currently being simulated.
		RenderObjectHandle GenerateStaticParticleMesh(const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags, float particle_width);

		// Get the vertex data for a single particle, eg a cube.
		std::vector<Vertex> GetParticleVertices(float particle_width) const;

		// Get the index data for a single particle, eg a cube.
		std::vector<uint32_t> GetParticleIndices() const;

		std::vector<Particle> StaticParticleToDynamic(const std::vector<StaticParticle>& static_particles, const std::vector<uint8_t>& side_flags, float particle_width) const;

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
		}particle_gen_{};

		struct ParticleNeighborShaderResources
		{
			DescriptorSetLayoutResource layout_resource;
			DescriptorSetResource descriptor_set_resource;
			BufferResource* particle_in_buffer;            // Particles to calculate neighbors from. Not owned by this resource, which is why it's a pointer.
			BufferResource neighbor_out_buffer;            // Buffer to write neighbor data to.
			ComputePipeline pipeline;
		}particle_neighbors_{};

		Context* context_{};
		VulkanRenderer* renderer_{};
	};
}
