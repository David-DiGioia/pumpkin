#pragma once

#include <vector>
#include "glm/glm.hpp"

namespace renderer
{
	constexpr uint32_t MAXIMUM_NODES_IN_RANGE{ 27 };       // Radius for quadratic interpolation is 1.5*h so maximum nodes is 3^3.
	constexpr uint32_t MAXIMUM_SUB_BLOCKS_IN_RANGE{ 216 }; // Quadratic interpolation radius of 1.5 is 3 sub blocks, so a diameter of 6, and 6^3 = 216.

	struct MaterialPoint
	{
		float mass;
		float mu;
		float lambda;
		glm::vec3 position;
		glm::vec3 velocity;
		glm::mat3 affine_matrix;
		glm::mat3 deformation_gradient;
	};

	struct MaterialPointIndex
	{
		uint32_t key;   // Unique value for every half box of grid. Used for sorting indices.
		uint32_t index; // Index into particles_.

		bool operator<(const MaterialPointIndex& other);
	};

	struct GridNode
	{
		float mass;
		glm::vec3 position;
		glm::vec3 velocity;
		glm::vec3 momentum;
		glm::vec3 force;
		glm::uvec3 coordinate;
	};

	float CalculateMu(float youngs_modulus, float poissons_ratio);

	float CalculateLambda(float youngs_modulus, float poissons_ratio);

	class MPMContext
	{
	public:
		void Initialize(std::vector<MaterialPoint>&& particles, float chunk_width);

		void SimulateStep(float delta_time);

		const std::vector<MaterialPoint>& GetParticles() const;

		const std::vector<GridNode>& GetNodes() const;

	private:
		void ParticleToGrid();

		void ComputeGridVelocities();

		void ComputeExplicitGridForces();

		void UpdateGridVelocity(float delta_time);

		void UpdateParticleDeformationGradient(float delta_time);

		void GridToParticle();

		void AdvectParticles(float delta_time);

		void UpdateIndexBuffers();

		float QuadraticKernel(float x) const;

		// Kernel for interpolation function.
		float CubicKernel(float x) const;

		float CubicKernelDerivative(float x) const;

		float QuadraticKernelDerivative(float x) const;

		float GetWeight(const glm::vec3& node_pos, const glm::vec3& particle_pos) const;

		glm::vec3 GetWeightGradient(const glm::vec3& node_pos, const glm::vec3& particle_pos) const;

		glm::mat3 GetPiolaKirchoffStress(const MaterialPoint& p) const;

		float GetDInverse(float delta_x) const;

		glm::mat3 GetDInverse2(const glm::vec3& particle_pos) const;

		glm::uvec3 GridNodeIndexToCoordinate(uint32_t index) const;

		uint32_t GridNodeCoordinateToIndex(const glm::uvec3& coord) const;

		// Returns vector of indices into nodes_.
		std::array<uint32_t, MAXIMUM_NODES_IN_RANGE> GetNodeIndicesWithinRadius(glm::vec3 position, uint32_t* out_count) const;

		// Returns array of indices into particle_indices_ of first element of contiguous block of particles in same sub block.
		std::array<uint32_t, MAXIMUM_SUB_BLOCKS_IN_RANGE> GetParticleRangesWithinRadius(const glm::uvec3& grid_coord, uint32_t* out_count) const;

		void PrintParticleWeights() const;

		std::vector<MaterialPoint> particles_{};
		std::vector<MaterialPointIndex> particle_indices_{}; // Contains indices into particles_, along with a key encoding the sub block coordinate.
		std::vector<uint32_t> sub_block_indices_{};          // Indices into particle_indices_, showing start of contiguous region containing particles in this sub block.
		std::vector<GridNode> nodes_{};
		float particle_radius_{};
		float particle_initial_volume_{};
	};
}
