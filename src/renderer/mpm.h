#pragma once

#include <vector>
#include "glm/glm.hpp"

namespace renderer
{
	struct MaterialPoint
	{
		float mass;
		float volume;
		float mu;
		float lambda;
		glm::vec3 position;
		glm::vec3 velocity;
		glm::mat3 affine_matrix;
	};

	struct GridNode
	{
		float mass;
		float volume;
		glm::vec3 position;
		glm::vec3 velocity;
		glm::vec3 momentum;
	};

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

		void UpdateGridVelocity();

		void UpdateParticleDeformationGradient();

		void GridToParticle();

		void AdvectParticles();

		// Kernel for interpolation function.
		float CubicKernel(float x) const;

		float Interpolate(const glm::vec3& node_pos, const glm::vec3& particle_pos) const;

		float GetDInverse() const;

		glm::uvec3 GridNodeIndexToCoordinate(uint32_t index) const;

		std::vector<MaterialPoint> particles_{};
		std::vector<GridNode> nodes_{};
		float grid_spacing_{};
		float particle_radius_{};
	};
}
