#include "mpm.h"

#include "renderer_constants.h"
#include "logger.h"
#include "common_constants.h"

namespace renderer
{
	constexpr uint32_t GRID_NODE_COUNT{ GRID_NODE_ROW_COUNT * GRID_NODE_ROW_COUNT * GRID_NODE_ROW_COUNT };

	glm::mat3 OuterProduct(const glm::vec3& v1, const glm::vec3& v2)
	{
		return glm::mat3{ v1 * v2.x, v1 * v2.y, v1 * v2.z };
	}

	float CalculateMu(float youngs_modulus, float poissons_ratio)
	{
		return youngs_modulus / (2.0f * (1.0f + poissons_ratio));
	}

	float CalculateLambda(float youngs_modulus, float poissons_ratio)
	{
		return (youngs_modulus * poissons_ratio) / ((1.0f + poissons_ratio) * (1.0f - 2.0f * poissons_ratio));
	}

	void MPMContext::Initialize(std::vector<MaterialPoint>&& particles, float chunk_width)
	{
		particles_ = std::move(particles);
		nodes_.resize(GRID_NODE_COUNT);

		grid_spacing_ = chunk_width / (float)(GRID_NODE_ROW_COUNT - 1);
		particle_radius_ = 0.5f * (chunk_width / CHUNK_ROW_VOXEL_COUNT);
		particle_initial_volume_ = (4.0f / 3.0f) * PI * particle_radius_ * particle_radius_ * particle_radius_; // Treat particle as sphere for now.

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
		UpdateGridVelocity(delta_time);
		UpdateParticleDeformationGradient(delta_time);
		GridToParticle();
		AdvectParticles(delta_time);
	}

	const std::vector<MaterialPoint>& MPMContext::GetParticles() const
	{
		return particles_;
	}

	void MPMContext::ParticleToGrid()
	{
		//float d_inverse{ GetDInverse() };

		//std::vector<glm::mat3> d_inverses{};
		//d_inverses.reserve(particles_.size());
		//for (const MaterialPoint& p : particles_) {
		//	d_inverses.push_back(GetDInverse2(p.position));
		//}

		// TODO: Later, should only iterate over particles within radius. Or approximately so. Will requiring binning the particles.
		for (GridNode& node : nodes_)
		{
			node.mass = 0.0f;
			node.momentum = glm::vec3{ 0.0f, 0.0f, 0.0f };

			uint32_t p_idx{0};
			for (const MaterialPoint& p : particles_)
			{
				float weight{ GetWeight(node.position, p.position) };

				node.mass += weight * p.mass;                                                                                 // Equation (172).
				//node.momentum += weight * p.mass * (p.velocity + p.affine_matrix * d_inverses[p_idx++] * (node.position - p.position)); // Equation (173).
				node.momentum += weight * p.mass * p.velocity;
			}
		}
	}

	void MPMContext::ComputeGridVelocities()
	{
		// TODO: This can later be put into ParticleToGrid(). Probably will me more efficient.
		for (GridNode& node : nodes_)
		{
			node.velocity = (node.mass == 0.0f) ? glm::vec3{ 0.0f, 0.0f, 0.0f } : node.momentum / node.mass;

			if (std::isnan(node.velocity.x)) {
				__debugbreak();
			}
		}
	}

	void MPMContext::ComputeExplicitGridForces()
	{
		for (GridNode& node : nodes_)
		{
			if (node.mass == 0.0f) {
				continue;
			}

			glm::vec3 sum{};
			for (const MaterialPoint& p : particles_) {
				sum += GetPiolaKirchoffStress(p) * glm::transpose(p.deformation_gradient) * GetWeightGradient(node.position, p.position);
			}

			node.force = -particle_initial_volume_ * sum;

			if (std::isnan(node.force.x)) {
				__debugbreak();
			}
		}
	}

	void MPMContext::UpdateGridVelocity(float delta_time)
	{
		for (GridNode& node : nodes_)
		{
			if (node.mass == 0.0f) {
				continue;
			}
			node.velocity += delta_time * (node.force / node.mass);

			if ((node.position.y == 0.0f) && (node.velocity.y < 0.0f)) {
				node.velocity.y = 0.0f;
			}
		}
	}

	void MPMContext::UpdateParticleDeformationGradient(float delta_time)
	{
		for (MaterialPoint& p : particles_)
		{
			glm::mat3 sum{};
			for (const GridNode& node : nodes_)
			{
				if (node.mass == 0.0f) {
					continue;
				}
				sum += OuterProduct(node.velocity, GetWeightGradient(node.position, p.position));
			}

			p.deformation_gradient = (glm::mat3(1.0f) + delta_time * sum) * p.deformation_gradient; // Equation (181).
		}
	}

	void MPMContext::GridToParticle()
	{
		for (MaterialPoint& p : particles_)
		{
			p.velocity = glm::vec3{ 0.0f, 0.0f, 0.0f };
			p.affine_matrix = glm::mat3{ 0.0f };
			for (const GridNode& node : nodes_)
			{
				if (node.mass == 0.0f) {
					continue;
				}
				float w{ GetWeight(node.position, p.position) };
				p.velocity += w * node.velocity;                                                // Equation (175).
				p.affine_matrix += w * OuterProduct(node.velocity, node.position - p.position); // Equation (176).
			}
		}
	}

	void MPMContext::AdvectParticles(float delta_time)
	{
		for (MaterialPoint& p : particles_) {
			p.position += delta_time * p.velocity;
		}
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

	// Not from an equation. Computed the derivative myself.
	float MPMContext::CubicKernelDerivative(float x) const
	{
		if (std::fabsf(x) < 1.0f) {
			return 0.5f * x * (3.0f * std::fabsf(x) - 4.0f);
		}
		else if (std::fabsf(x) < 2.0f)
		{
			float a{ 2.0f - std::fabsf(x) };
			return (x * a * a) / (2.0f * std::fabsf(x));
		}
		else {
			return 0.0f;
		}
	}

	// Equation (121).
	float MPMContext::GetWeight(const glm::vec3& node_pos, const glm::vec3& particle_pos) const
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

	glm::vec3 MPMContext::GetWeightGradient(const glm::vec3& node_pos, const glm::vec3& particle_pos) const
	{
		glm::vec3 gradient{};

		switch (MPM_INTERPOLATION_KERNEL)
		{
		case MPMInterpolationKernel::LINEAR:
			logger::Error("Linear MPM interpolation kernel not yet supported.\n");
			break;
		case MPMInterpolationKernel::QUADRATIC:
			logger::Error("Quadratic MPM interpolation kernel not yet supported.\n");
			break;
		case MPMInterpolationKernel::CUBIC:
		{
			float x_result = CubicKernel((particle_pos.x - node_pos.x) / grid_spacing_);
			float y_result = CubicKernel((particle_pos.y - node_pos.y) / grid_spacing_);
			float z_result = CubicKernel((particle_pos.z - node_pos.z) / grid_spacing_);
			float ddx_x_result = CubicKernelDerivative((particle_pos.x - node_pos.x) / grid_spacing_);
			float ddy_y_result = CubicKernelDerivative((particle_pos.y - node_pos.y) / grid_spacing_);
			float ddz_z_result = CubicKernelDerivative((particle_pos.z - node_pos.z) / grid_spacing_);
			gradient.x = ddx_x_result * y_result * z_result;
			gradient.y = x_result * ddy_y_result * z_result;
			gradient.z = x_result * y_result * ddz_z_result;
			break;
		}
		default:
			logger::Error("Unrecognized MPM interpolation kernel.\n");
		}

		return gradient;
	}

	glm::mat3 MPMContext::GetPiolaKirchoffStress(const MaterialPoint& p) const
	{
		glm::mat3 f_inv_transpose{ glm::transpose(glm::inverse(p.deformation_gradient)) };
		float j{ glm::determinant(p.deformation_gradient) };
		auto tmp = p.mu * (p.deformation_gradient - f_inv_transpose) + p.lambda * std::logf(j) * f_inv_transpose; // Equation (48).
		if (std::isnan(tmp[0][0])) {
			__debugbreak();
		}
		return tmp;
	}

	float MPMContext::GetDInverse(float delta_x) const
	{
		float d{};

		switch (MPM_INTERPOLATION_KERNEL)
		{
		case MPMInterpolationKernel::LINEAR:
			logger::Error("Linear D^-1 does not exist. Do not call this function when using a linear interpolation kernel.\n");
			break;
		case MPMInterpolationKernel::QUADRATIC:
			d = (1.0f / 4.0f) * delta_x * delta_x; // From paragraph after equation (176).
			break;
		case MPMInterpolationKernel::CUBIC:
			d = (1.0f / 3.0f) * delta_x * delta_x; // From paragraph after equation (176).
			break;
		default:
			logger::Error("Unrecognized MPM interpolation kernel.\n");
		}

		return 1.0f / d;
	}

	glm::mat3 MPMContext::GetDInverse2(const glm::vec3& particle_pos) const
	{
		glm::mat3 sum{};
		for (const GridNode& node : nodes_) {
			sum += GetWeight(node.position, particle_pos) * OuterProduct(node.position - particle_pos, node.position - particle_pos);
		}
		return sum;
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
