#include "mpm.h"

#include "renderer_constants.h"
#include "logger.h"

namespace renderer
{
	constexpr uint32_t GRID_SIZE{ 4 }; // Grid cell width in units of voxels.
	constexpr uint32_t GRID_NODE_ROW_COUNT{ (CHUNK_ROW_VOXEL_COUNT / GRID_SIZE) + 1 };
	constexpr uint32_t GRID_NODE_COUNT{ GRID_NODE_ROW_COUNT * GRID_NODE_ROW_COUNT * GRID_NODE_ROW_COUNT };

	void MPMContext::Initialize(std::vector<MaterialPoint>&& particles, float chunk_width)
	{
		particles_ = std::move(particles);
		nodes_.resize(GRID_NODE_COUNT);

		grid_spacing_ = chunk_width / (float)(GRID_NODE_ROW_COUNT - 1);
		particle_radius_ = 0.5f * (chunk_width / CHUNK_ROW_VOXEL_COUNT);

		// Initialize grid positions.
		for (uint32_t i{ 0 }; i < (uint32_t)nodes_.size(); ++i)
		{
			glm::uvec3 coord{ GridNodeIndexToCoordinate(i) };
			nodes_[i].position = grid_spacing_ * glm::vec3{ coord.x, coord.y, coord.z };
		}
	}

	void MPMContext::SimulateStep(float delta_time)
	{
		ParticleToGrid();
		ComputeGridVelocities();
		ComputeExplicitGridForces();
		UpdateGridVelocity();
		UpdateParticleDeformationGradient();
		GridToParticle();
		AdvectParticles();
	}

	const std::vector<MaterialPoint>& MPMContext::GetParticles() const
	{
		return particles_;
	}

	void MPMContext::ParticleToGrid()
	{
		float d_inverse{ GetDInverse() };

		// TODO: Later, should only iterate over particles within radius. Or approximately so. Will requiring binning the particles.
		for (GridNode& node : nodes_)
		{
			node.mass = 0.0f;
			node.momentum = glm::vec3{ 0.0f, 0.0f, 0.0f };

			for (const MaterialPoint& p : particles_)
			{
				float weight{ Interpolate(node.position, p.position) };

				node.mass += weight * p.mass;                                                                                 // Equation (172).
				node.momentum += weight * p.mass * (p.velocity + p.affine_matrix * d_inverse * (node.position - p.position)); // Equation (173).
			}
		}
	}

	void MPMContext::ComputeGridVelocities()
	{
		// TODO: This can later be put into ParticleToGrid(). Probably will me more efficient.
		for (GridNode& node : nodes_) {
			node.velocity = (node.mass == 0.0f) ? glm::vec3{0.0f, 0.0f, 0.0f} : node.momentum / node.mass;
		}
	}

	void MPMContext::ComputeExplicitGridForces()
	{
		// TODO: Pickup here.
	}

	void MPMContext::UpdateGridVelocity()
	{
	}

	void MPMContext::UpdateParticleDeformationGradient()
	{
	}

	void MPMContext::GridToParticle()
	{
	}

	void MPMContext::AdvectParticles()
	{
	}

	// Equation (122).
	float MPMContext::CubicKernel(float x) const
	{
		x = std::fabsf(x);

		if (x < 1.0f) {
			return (0.5f * x * x * x) - (x * x) + (2.0f / 3.0f);
		}
		else if (x < 2.0f)
		{
			float a{ 2.0f - x };
			return (1.0f / 6.0f) * a * a * a;
		}
		else {
			return 0.0f;
		}
	}

	// Equation (121).
	float MPMContext::Interpolate(const glm::vec3& node_pos, const glm::vec3& particle_pos) const
	{
		float x_result{};
		float y_result{};
		float z_result{};

		switch (MPM_INTERPOLATION_KERNEL)
		{
		case MPMInterpolationKernel::LINEAR:
			logger::Error("Linear MPM interpolation kernel not yet supported.\n");
			break;
		case MPMInterpolationKernel::QUADRATIC:
			logger::Error("Quadratic MPM interpolation kernel not yet supported.\n");
			break;
		case MPMInterpolationKernel::CUBIC:
			x_result = CubicKernel((particle_pos.x - node_pos.x) / grid_spacing_);
			y_result = CubicKernel((particle_pos.y - node_pos.y) / grid_spacing_);
			z_result = CubicKernel((particle_pos.z - node_pos.z) / grid_spacing_);
			break;
		default:
			logger::Error("Unrecognized MPM interpolation kernel.\n");
		}

		return x_result * y_result * z_result;
	}

	float MPMContext::GetDInverse() const
	{
		float d{};

		switch (MPM_INTERPOLATION_KERNEL)
		{
		case MPMInterpolationKernel::LINEAR:
			logger::Error("Linear D^-1 does not exist. Do not call this function when using a linear interpolation kernel.\n");
			break;
		case MPMInterpolationKernel::QUADRATIC:
			d = (1.0f / 4.0f) * particle_radius_ * particle_radius_; // From paragraph after equation (176).
			break;
		case MPMInterpolationKernel::CUBIC:
			d = (1.0f / 3.0f) * particle_radius_ * particle_radius_; // From paragraph after equation (176).
			break;
		default:
			logger::Error("Unrecognized MPM interpolation kernel.\n");
		}

		return 1.0f / d;
	}

	glm::uvec3 MPMContext::GridNodeIndexToCoordinate(uint32_t index) const
	{
		uint32_t slice_area{ GRID_NODE_ROW_COUNT * GRID_NODE_ROW_COUNT };
		uint32_t z{ index / slice_area };
		uint32_t y{ (index % slice_area) / GRID_NODE_ROW_COUNT };
		uint32_t x{ index % GRID_NODE_ROW_COUNT };

		return glm::uvec3{ x, y, z };
	}
}
