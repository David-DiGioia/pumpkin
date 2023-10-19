#pragma once

#include "glm/glm.hpp"

#include "descriptor_set.h"
#include "memory_allocator.h"
#include "mesh.h"
#include "pipeline.h"
#include "mpm.h"

namespace renderer
{
	class VulkanRenderer;

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

	// Convert a particle 1D buffer index into a 3D coordinate in the chunk.
	glm::uvec3 ParticleIndexToCoordinate(uint32_t index);

	// Convert 3D coordinate in the chunk into a 1D buffer index.
	uint32_t CoordinateToParticleIndex(const glm::uvec3& coord);

	class StaticParticleMeshGenerator
	{
	public:
		Mesh* Generate(const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags);

	private:
		struct Rectangle
		{
			uint32_t start_h;   // Inclusive. Horizontal start.
			uint32_t end_h;     // Inclusive. Horizontal end.
			uint32_t start_v;   // Inclusive. Vertical start.
		};

		// Generate a single side of all the voxels. Will need to be called 6 times for full mesh generation.
		void GenerateSide(ParticleSidesFlagBits side, const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags);

		void TriangulateRectangle(ParticleSidesFlagBits side, uint32_t rect_idx, uint32_t vertical, uint32_t depth);

		// Clear indices to a rectangle in rectangles_ between rectangle's start and end.
		void ClearRectangleIndices(uint32_t rect_idx);

		// Set indices to a rectangle in rectangles_ between rectangle's start and end.
		void SetRectangleIndices(uint32_t rect_idx);

		// Get reference to chunk coordinate currently acting as the horizontal access.
		uint32_t& GetHorizontalReference(ParticleSidesFlagBits side);

		// Get reference to chunk coordinate currently acting as the vertical access.
		uint32_t& GetVerticalReference(ParticleSidesFlagBits side);

		// Get reference to chunk coordinate currently acting as the depth access.
		uint32_t& GetDepthReference(ParticleSidesFlagBits side);

		std::vector<uint32_t> rectangle_indices_{};  // rectangle_indices[j] contains the index into x_positive_partial_rectangles which contains this coordinate in its range. Otherwise contains null index.
		std::vector<Rectangle> rectangles_{};        // The WIP rectangles that have not been triangulated yet.
		Mesh* mesh_{};                               // The output mesh.

		// These coordinates will not be accessed directly, but instead will be reference by horizontal/vertical/depth variables.
		uint32_t x_{};
		uint32_t y_{};
		uint32_t z_{};
	};

	class ParticleContext
	{
	public:
		void Initialize(Context* context, VulkanRenderer* renderer);

		void PhysicsUpdate(float delta_time);

		void CleanUp();

		// Generated render object will replace ro_target.
		void InvokeParticleGenShader();

		void SetParticleGenShader(uint32_t shader_idx, const std::vector<std::byte>& custom_ubo_buffer);

		DescriptorSetLayoutResource& GetParticleGenLayoutResource();

		void EnablePhysicsUpdate();

		void DisablePhysicsUpdate();

		void ResetParticles();

		bool GetPhysicsUpdateEnabled() const;

		bool GetParticlesEmpty() const;

		void TransferStaticParticlesToMPM();

		void SetTargetRenderObject(RenderObjectHandle ro_target);

		void SetChunkWidth(float chunk_width);

		float GetChunkWidth() const;

#ifdef EDITOR_ENABLED
		void SetMPMDebugGeometryGenEnabled(bool enabled);

		const MPMDebugGeometry& GetMPMDebugGeometry() const;
#endif

	private:
		void InitializeParticleGenShaderResources();

		void InitializeParticleNeighborsShaderResources();


		// Generates triangles for each individual particle as a cube. Can be done on host or device.
		void GenerateDynamicParticleMesh(const std::vector<MaterialPoint>& particles);

#ifdef EDITOR_ENABLED
		void GenerateDynamicDebugMPMParticleMesh(const std::vector<MaterialPoint>& particles);
#endif

		// Genereates fewest triangles possible as a shell around particle mass. Good for particles not currently being simulated.
		void GenerateStaticParticleMesh(const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags);

		// Get the vertex data for a single particle, eg a cube.
		std::vector<Vertex> GetParticleVertices() const;

		std::vector<MPMDebugVertex> GetMPMParticleVertices() const;

		// Get the index data for a single particle, eg a cube.
		std::vector<uint32_t> GetParticleIndices() const;

		std::vector<MaterialPoint> StaticParticleToDynamic(const std::vector<StaticParticle>& static_particles, const std::vector<uint8_t>& side_flags) const;

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

#ifdef EDITOR_ENABLED
		bool generate_mpm_geometry_{};
		MPMDebugGeometry mpm_geometry_{};
#endif

		std::vector<StaticParticle> static_particles_{};
		bool has_played_{}; // True if the particle simulation has been played yet.
		bool update_physics_{};
		float chunk_width_{ 5.0f };
		RenderObjectHandle ro_target_{}; // Target render object for updating during particle simulation.
		MPMContext mpm_context_{};
		Context* context_{};
		VulkanRenderer* renderer_{};
	};
}
