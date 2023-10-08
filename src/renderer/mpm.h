#pragma once

#include <vector>
#include "glm/glm.hpp"

namespace renderer
{
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

	struct GridNode
	{
		float mass;
		float volume;
		glm::vec3 position;
		glm::vec3 velocity;
		glm::vec3 momentum;
		glm::vec3 force;
	};

	float CalculateMu(float youngs_modulus, float poissons_ratio);

	float CalculateLambda(float youngs_modulus, float poissons_ratio);

	class MPMContext
	{
	public:
		void Initialize(std::vector<MaterialPoint>&& particles, float chunk_width);

		void SimulateStep(float delta_time);

		const std::vector<MaterialPoint>& GetParticles() const;

	private:
		void ParticleToGrid();

		void ComputeGridVelocities();

		void ComputeExplicitGridForces();

		void UpdateGridVelocity(float delta_time);

		void UpdateParticleDeformationGradient(float delta_time);

		void GridToParticle();

		void AdvectParticles(float delta_time);

		// Kernel for interpolation function.
		float CubicKernel(float x) const;

		float CubicKernelDerivative(float x) const;

		float GetWeight(const glm::vec3& node_pos, const glm::vec3& particle_pos) const;

		glm::vec3 GetWeightGradient(const glm::vec3& node_pos, const glm::vec3& particle_pos) const;

		glm::mat3 GetPiolaKirchoffStress(const MaterialPoint& p) const;

		float GetDInverse(float delta_x) const;

		glm::mat3 GetDInverse2(const glm::vec3& particle_pos) const;

		glm::uvec3 GridNodeIndexToCoordinate(uint32_t index) const;

		std::vector<MaterialPoint> particles_{};
		std::vector<GridNode> nodes_{};
		float grid_spacing_{};
		float particle_radius_{};
		float particle_initial_volume_{};
	};
}
