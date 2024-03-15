#pragma once

#include "glm/glm.hpp"

#include "descriptor_set.h"
#include "memory_allocator.h"
#include "mesh.h"
#include "pipeline.h"

namespace renderer
{
	class VulkanRenderer;

	// Encodes whether each of the 6 voxel neighbors are occupied or not.
	enum class VoxelSidesFlagBits : uint8_t
	{
		X_POSITIVE = 0x01,
		X_NEGATIVE = 0x02,
		Y_POSITIVE = 0x04,
		Y_NEGATIVE = 0x08,
		Z_POSITIVE = 0x10,
		Z_NEGATIVE = 0x20,
		ALL_SIDES = 0x3F,
	};

	// Each voxel in a rigid body can be labeled based on its neighbors which is useful for collision detection. 
	enum class VoxelGeometricFeatureType : uint8_t
	{
		INTERIOR,
		CORNER,
		EDGE,
		FACE,
	};

	// Defined in renderer instead of Pumpkin since it's used in particle_gen.cpp.
	constexpr uint8_t PHYSICS_MATERIAL_EMPTY_INDEX{ 0xFF };

	// Particles that are not being simulated.
	struct Voxel
	{
		uint8_t physics_material_index;
	};

	class VoxelChunk
	{
	public:
		VoxelChunk(uint32_t width, uint32_t height, uint32_t depth);

		VoxelChunk(uint32_t width, uint32_t height, uint32_t depth, std::vector<std::pair<Voxel, glm::uvec3>>&& voxel_pairs);

		Voxel& Coordinate(uint32_t i, uint32_t j, uint32_t k);

		Voxel& Coordinate(const glm::uvec3& coord);

		const Voxel& Coordinate(uint32_t i, uint32_t j, uint32_t k) const;

		const Voxel& Coordinate(const glm::uvec3& coord) const;

		Voxel& Index(uint32_t idx);

		const Voxel& Index(uint32_t idx) const;

		uint32_t VoxelCount() const;

		bool IsOccluded(uint32_t voxel_idx) const;

		bool IsEmpty(uint32_t voxel_idx) const;

		bool IsEmpty(const glm::uvec3& voxel_coord) const;

		bool InBounds(const glm::uvec3& voxel_coord) const;

		glm::uvec3 IndexToCoordinate(uint32_t index) const;

		uint32_t CoordinateToIndex(const glm::uvec3& coord) const;

		std::vector<Voxel>& GetVoxels();

		std::vector<uint8_t>& GetSideFlags();

		const std::vector<glm::uvec3>& GetOuterVoxelIndices() const;

		// Given a voxel-sized particle in coordinate space (one unit is one voxel width) return all possible voxels in the chunk that could collide with it.
		// This means the voxels that are a close enough distance and not empty.
		std::array<glm::uvec3, 8> GetPotentialCollisions(const glm::vec3& coord_space, uint32_t* potential_collision_count) const;

	private:
		// Helper function for calculating side flags.
		bool NeighborOccupied(glm::uvec3 coord, glm::ivec3 offset) const;

		uint32_t width_{};
		uint32_t height_{};
		uint32_t depth_{};
		uint32_t width_height_slice_{};
		std::vector<Voxel> voxels_{};
		std::vector<uint8_t> side_flags_{};
		std::vector<glm::uvec3> outer_voxel_coords_{}; // A list of the non-occluded voxels.
	};

	// Can convert static particles to this as a simplified stand-in for material point.
	struct MaterialPosition
	{
		uint8_t physics_material_index;
		glm::vec3 position;
	};

	// Each MaterialOffset specifies a material type and range in the particle buffer.
	struct MaterialRange
	{
		uint8_t physics_material_index;
		uint32_t offset; // Index offset, not byte offset.
		uint32_t count;  // Count of particles with this physics material.
	};

	class StaticParticleMeshGenerator
	{
	public:
		Mesh* Generate(const VoxelChunk& voxel_chunk);

	private:
		struct Rectangle
		{
			uint32_t start_h;   // Inclusive. Horizontal start.
			uint32_t end_h;     // Inclusive. Horizontal end.
			uint32_t start_v;   // Inclusive. Vertical start.
		};

		// Generate a single side of all the voxels. Will need to be called 6 times for full mesh generation.
		void GenerateSide(VoxelSidesFlagBits side, const std::vector<Voxel>& particles, const std::vector<uint8_t>& side_flags);

		void TriangulateRectangle(VoxelSidesFlagBits side, uint32_t rect_idx, uint32_t vertical, uint32_t depth);

		// Clear indices to a rectangle in rectangles_ between rectangle's start and end.
		void ClearRectangleIndices(uint32_t rect_idx);

		// Set indices to a rectangle in rectangles_ between rectangle's start and end.
		void SetRectangleIndices(uint32_t rect_idx);

		// Get reference to chunk coordinate currently acting as the horizontal access.
		uint32_t& GetHorizontalReference(VoxelSidesFlagBits side);

		// Get reference to chunk coordinate currently acting as the vertical access.
		uint32_t& GetVerticalReference(VoxelSidesFlagBits side);

		// Get reference to chunk coordinate currently acting as the depth access.
		uint32_t& GetDepthReference(VoxelSidesFlagBits side);

		std::vector<uint32_t> rectangle_indices_{};  // rectangle_indices[j] contains the index into x_positive_partial_rectangles which contains this coordinate in its range. Otherwise contains null index.
		std::vector<Rectangle> rectangles_{};        // The WIP rectangles that have not been triangulated yet.
		Mesh* mesh_{};                               // The output mesh.

		// These coordinates will not be accessed directly, but instead will be reference by horizontal/vertical/depth variables.
		uint32_t x_{};
		uint32_t y_{};
		uint32_t z_{};
	};

	class ParticleGenContext
	{
	private:
		struct FrameResources;

	public:

		// Input material_points must have each material type contiguous with one another (eg sorted, but order of material groups doesn't matter).
		template <typename T>
		std::vector<MaterialRange> CreateMaterialRanges(const std::vector<T>& material_points)
		{
			if (material_points.empty()) {
				return {};
			}

			std::vector<MaterialRange> mat_ranges{};
			uint8_t current_physics_mat_idx{ material_points.front().physics_material_index };
			uint32_t offset{ 0 };

			for (uint32_t i{ 0 }; i < (uint32_t)material_points.size(); ++i)
			{
				const T& p{ material_points[i] };
				if (current_physics_mat_idx != p.physics_material_index)
				{
					mat_ranges.push_back(MaterialRange{
						.physics_material_index = current_physics_mat_idx,
						.offset = offset,
						.count = i - offset,
						});

					offset = i;
					current_physics_mat_idx = p.physics_material_index;
				}
			}

			mat_ranges.push_back(MaterialRange{
				.physics_material_index = current_physics_mat_idx,
				.offset = offset,
				.count = (uint32_t)material_points.size() - offset,
				});

			return mat_ranges;
		}

		std::vector<MaterialRange> GetMaterialRanges();

		void Initialize(Context* context, VulkanRenderer* renderer);

		void CleanUp();

		void InvokeParticleGenShader(RenderObjectHandle ro_target, std::vector<Voxel>* out_voxels, std::vector<uint8_t>* out_side_flags);

		void SetParticleGenShader(uint32_t shader_idx, uint32_t custom_ubo_size);

		void UpdateParticleGenShaderCustomUBO(const std::vector<std::byte>& custom_ubo);

		DescriptorSetLayoutResource& GetParticleGenLayoutResource();

		// Get the vertex data for a single particle, eg a cube.
		std::vector<Vertex> GetParticleVertices() const;

		// Get the index data for a single particle, eg a cube.
		std::vector<uint32_t> GetParticleIndices() const;

		// Generates triangles for each individual particle as a cube.
		// Positions should be an array of glm::vec3 with arbitrary stride between each. Stride is in bytes.
		void GenerateDynamicParticleMesh(
			RenderObjectHandle ro_target,
			const std::byte* positions,
			uint32_t offset,
			uint32_t stride,
			const std::vector<MaterialRange>& mat_ranges);
		
		void SetPhysicsToRenderMaterialMap(std::vector<int>&& physics_to_render_mat_idx);

		void UpdatePhysicsRenderMaterials(RenderObjectHandle ro_target);

		void CmdBegin();

		// Offset and stride must be multiples of 4.
		// Particle positions must be grouped, and correspond to mat_ranges.
		void CmdGenerateDynamicParticleMesh(
			RenderObjectHandle ro_target,
			const std::byte* positions,
			uint32_t position_count,
			uint32_t offset,
			uint32_t stride,
			const std::vector<MaterialRange>& mat_ranges);

		void CmdSubmit();

		bool CommandsRecordedThisFrame();

		// Genereates fewest triangles possible as a shell around particle mass. Good for particles not currently being simulated.
		void GenerateStaticParticleMesh(RenderObjectHandle ro_target, const VoxelChunk& voxel_chunk, const glm::vec3& object_origin);

		void NextFrame();

	private:
		void InitializeParticleGenShaderResources();

		void InitializeParticleNeighborsShaderResources();

		void InitializeParticleMeshShaderResources();

		void InitializeCommandBuffers();

		void InitializeFences();

		FrameResources& GetCurrentFrame();

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

		struct ParticleMeshGlobalShaderResources
		{
			DescriptorSetLayoutResource layout_resource;   // Layout resource for user-defined particle shaders.
			ComputePipeline pipeline;
		}particle_mesh_{};

		struct ParticleMeshFrameShaderResources
		{
			struct UBO
			{
				Vertex cube_vertices[24];
				uint32_t cube_indices[36];
				uint32_t position_stride_dword; // Byte stride divided by 4.
				uint32_t position_offset_dword; // Byte offset divided by 4.
				uint32_t position_count;
			};

			DescriptorSetResource descriptor_set_resource; // Descriptor set resource for user-defined particle gen shader.
			BufferResource positions_in;                   // Non-packed position data of dynamic particles.
			BufferResource vertices_out;                   // Out vertex data for particle mesh.
			BufferResource indices_out;                    // Out index data for particle mesh.
			BufferResource ubo_buffer;                     // UBO containing cube vertex data and position data.
		};

		struct FrameResources
		{
			ParticleMeshFrameShaderResources particle_mesh; // Frame resource since particle position data needs to be double buffered.
			VkCommandBuffer command_buffer;                 // Command buffer that particle mesh creation is recorded into.
			VkFence fence;
		};

		Context* context_{};
		VulkanRenderer* renderer_{};

		uint32_t current_frame_{};
		std::array<FrameResources, FRAMES_IN_FLIGHT> frame_resources_{};

		bool commands_recorded_{};

		std::vector<int> physics_to_render_mat_idx_{}; // Convert a physics material index into a render material index. ith index is render material of ith physics material.
		std::vector<MaterialRange> mat_ranges_{};      // Particle ranges of each physics material.
	};
}
