#include "mpm.h"

#include "renderer_constants.h"
#include "logger.h"
#include "common_constants.h"
#include "tracy/Tracy.hpp"
#include "glm/gtx/norm.hpp"

namespace renderer
{
	constexpr float KERNEL_RADIUS{ 1.5f };
	constexpr float KERNEL_RADIUS_SQUARED{ KERNEL_RADIUS * KERNEL_RADIUS };
	const uint32_t SUB_BLOCK_ROW_COUNT{ GRID_NODE_ROW_COUNT * 2u };
	const uint32_t SUB_BLOCK_COUNT{ SUB_BLOCK_ROW_COUNT * SUB_BLOCK_ROW_COUNT * SUB_BLOCK_ROW_COUNT };

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

	glm::uvec3 PositionToSubBlockCoordinate(glm::vec3 pos)
	{
		pos.x = std::clamp(pos.x, 0.0f, CHUNK_WIDTH);
		pos.y = std::clamp(pos.y, 0.0f, CHUNK_WIDTH);
		pos.z = std::clamp(pos.z, 0.0f, CHUNK_WIDTH);

		// Make 1 positional unit equal to 1 grid unit.
		pos /= GRID_SIZE;
		// Convert to sub block units.
		pos *= 2.0f;
		
		return { (uint32_t)pos.x, (uint32_t)pos.y, (uint32_t)pos.z };
	}

	// Returns coordinate into sub_block_indices_.
	uint32_t SubBlockCoordinateToIndex(const glm::uvec3& sub_coord)
	{
		uint32_t slice_area{ SUB_BLOCK_ROW_COUNT * SUB_BLOCK_ROW_COUNT };
		return sub_coord.x + sub_coord.y * SUB_BLOCK_ROW_COUNT + sub_coord.z * slice_area;
	}

	void MPMContext::Initialize(std::vector<MaterialPoint>&& particles, float chunk_width)
	{
		particles_ = std::move(particles);
		particle_indices_.clear();
		particle_indices_.resize(particles_.size());
		sub_block_indices_.clear();
		sub_block_indices_.resize(SUB_BLOCK_COUNT, NULL_INDEX);
		nodes_.resize(GRID_NODE_COUNT);

		particle_radius_ = 0.5f * (chunk_width / CHUNK_ROW_VOXEL_COUNT);
		particle_initial_volume_ = (4.0f / 3.0f) * PI * particle_radius_ * particle_radius_ * particle_radius_; // Treat particle as sphere for now.

		// Initialize grid positions.
		for (uint32_t i{ 0 }; i < (uint32_t)nodes_.size(); ++i)
		{
			glm::uvec3 coord{ GridNodeIndexToCoordinate(i) };
			nodes_[i].position = GRID_SIZE * glm::vec3{ coord.x, coord.y, coord.z };
			nodes_[i].coordinate = coord;
		}

		// Initialize particle index buffer.
		for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i)
		{
			glm::uvec3 sub_block_coord{ PositionToSubBlockCoordinate(particles_[i].position) };

			particle_indices_[i] = MaterialPointIndex{
				.key = SubBlockCoordinateToIndex(sub_block_coord),
				.index = i,
			};
		}

		UpdateIndexBuffers();
	}

	void MPMContext::SimulateStep(float delta_time)
	{
		//PrintParticleWeights();

		ParticleToGrid();
		ComputeGridVelocities();
		ComputeExplicitGridForces();
		UpdateGridVelocity(delta_time);
		UpdateParticleDeformationGradient(delta_time);
		GridToParticle();
		AdvectParticles(delta_time);

		UpdateIndexBuffers();
	}

	const std::vector<MaterialPoint>& MPMContext::GetParticles() const
	{
		return particles_;
	}

	const std::vector<GridNode>& MPMContext::GetNodes() const
	{
		return nodes_;
	}

	void MPMContext::ParticleToGrid()
	{
		ZoneScoped;
		// For APIC transfer.
		float d_inverse{ GetDInverse() };

		for (GridNode& node : nodes_)
		{
			node.mass = 0.0f;
			node.momentum = glm::vec3{ 0.0f, 0.0f, 0.0f };

			uint32_t particle_range_count{};
			auto start_of_ranges{ GetParticleRangesWithinRadius(node.coordinate, &particle_range_count) };
			for (uint32_t i{ 0 }; i < particle_range_count; ++i)
			{
				uint32_t range_start{ start_of_ranges[i] };
				uint32_t current_key{ particle_indices_[range_start].key };
				for (uint32_t j{ range_start }; j < (uint32_t)particle_indices_.size() && particle_indices_[j].key == current_key; ++j)
				{
					// Doing radius check here slightly hurts performance unlike when computing the force. Probably because work here it more light weight.
					MaterialPoint& p{ particles_[particle_indices_[j].index] };

					float weight{ GetWeight(node.position, p.position) };
					node.mass += weight * p.mass;                                                                                 // Equation (172).
					node.momentum += weight * p.mass * (p.velocity + p.affine_matrix * d_inverse * (node.position - p.position)); // Equation (173).
				}
			}
		}
	}

	void MPMContext::ComputeGridVelocities()
	{
		ZoneScoped;
		// TODO: This can later be put into ParticleToGrid(). Probably will me more efficient.
		for (GridNode& node : nodes_)
		{
			node.velocity = (node.mass == 0.0f) ? glm::vec3{ 0.0f, 0.0f, 0.0f } : node.momentum / node.mass;

			if (std::isnan(node.velocity.x))
			{
				logger::Error("Node velocity is nan. Aborting.\n");
				__debugbreak();
			}
		}
	}

	void MPMContext::ComputeExplicitGridForces()
	{
		ZoneScoped;
		for (GridNode& node : nodes_)
		{
			if (node.mass == 0.0f) {
				continue;
			}

			glm::vec3 sum{};

			uint32_t particle_range_count{};
			auto start_of_ranges{ GetParticleRangesWithinRadius(node.coordinate, &particle_range_count) };
			for (uint32_t i{ 0 }; i < particle_range_count; ++i)
			{
				uint32_t range_start{ start_of_ranges[i] };
				uint32_t current_key{ particle_indices_[range_start].key };
				for (uint32_t j{ range_start }; j < (uint32_t)particle_indices_.size() && particle_indices_[j].key == current_key; ++j)
				{
					MaterialPoint& p{ particles_[particle_indices_[j].index] };
					if (glm::length2((p.position / GRID_SIZE) - glm::vec3{ node.coordinate }) >= KERNEL_RADIUS_SQUARED) {
						continue;
					}

					sum += GetPiolaKirchoffStress(p) * glm::transpose(p.deformation_gradient) * GetWeightGradient(node.position, p.position);
				}
			}

			node.force = -particle_initial_volume_ * sum; // Equation (189).

			node.force += node.mass * glm::vec3{ 0.0f, -2.0f, 0.0f }; // Gravity?

			if (std::isnan(node.force.x))
			{
				logger::Error("Node force is nan. Aborting.\n");
				__debugbreak();
			}
		}
	}

	void MPMContext::UpdateGridVelocity(float delta_time)
	{
		ZoneScoped;
		for (GridNode& node : nodes_)
		{
			if (node.mass == 0.0f) {
				continue;
			}
			node.velocity += delta_time * (node.force / node.mass);

			if (node.coordinate.y == 0 && node.velocity.y < 0.0f) {
				node.velocity.y = 0.0f;
			}
		}
	}

	void MPMContext::UpdateParticleDeformationGradient(float delta_time)
	{
		ZoneScoped;
		for (MaterialPoint& p : particles_)
		{
			glm::mat3 sum{};
			uint32_t node_index_count{};
			auto node_indices{ GetNodeIndicesWithinRadius(p.position, &node_index_count) };
			for (uint32_t i{ 0 }; i < node_index_count; ++i)
			{
				GridNode& node{ nodes_[node_indices[i]] };
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
		ZoneScoped;
		for (MaterialPoint& p : particles_)
		{
			p.velocity = glm::vec3{ 0.0f, 0.0f, 0.0f };
			p.affine_matrix = glm::mat3{ 0.0f };

			uint32_t node_index_count{};
			auto node_indices{ GetNodeIndicesWithinRadius(p.position, &node_index_count) };
			for (uint32_t i{ 0 }; i < node_index_count; ++i)
			{
				GridNode& node{ nodes_[node_indices[i]] };
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
		ZoneScoped;
		for (MaterialPointIndex& mi : particle_indices_)
		{
			MaterialPoint& p{ particles_[mi.index] };

			p.position += delta_time * p.velocity;
			mi.key = SubBlockCoordinateToIndex(PositionToSubBlockCoordinate(p.position));
		}
	}

	void MPMContext::UpdateIndexBuffers()
	{
		ZoneScoped;
		std::sort(particle_indices_.begin(), particle_indices_.end());

		uint32_t current_key{ NULL_INDEX };
		for (uint32_t i{ 0 }; i < (uint32_t)particle_indices_.size(); ++i)
		{
			if ((particle_indices_[i].key != current_key) && (particle_indices_[i].key != NULL_INDEX))
			{
				MaterialPoint& p{ particles_[particle_indices_[i].index] };
				glm::uvec3 sub_block_coord{ PositionToSubBlockCoordinate(p.position) };
				sub_block_indices_[SubBlockCoordinateToIndex(sub_block_coord)] = i;

				current_key = particle_indices_[i].key;
			}
		}
	}

	// Equation (123).
	float MPMContext::QuadraticKernel(float x) const
	{
		x = std::fabsf(x);

		if (x < 0.5f) {
			return (3.0f / 4.0f) - x * x;
		}
		else if (x < (3.0f / 2.0f))
		{
			float a{ (3.0f / 2.0f) - x };
			return 0.5f * a * a;
		}
		else {
			return 0.0f;
		}
	}

	// Not from an equation. Computed the derivative myself.
	float MPMContext::QuadraticKernelDerivative(float x) const
	{
		if (std::fabsf(x) < 0.5) {
			return -2.0f * x;
		}
		else if (std::fabsf(x) < (3.0f / 2.0f))
		{
			return x - (3.0f * x) / (2.0f * std::fabsf(x));
		}
		else {
			return 0.0f;
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
			x_result = QuadraticKernel((particle_pos.x - node_pos.x) / GRID_SIZE);
			y_result = QuadraticKernel((particle_pos.y - node_pos.y) / GRID_SIZE);
			z_result = QuadraticKernel((particle_pos.z - node_pos.z) / GRID_SIZE);
			break;
		case MPMInterpolationKernel::CUBIC:
			x_result = CubicKernel((particle_pos.x - node_pos.x) / GRID_SIZE);
			y_result = CubicKernel((particle_pos.y - node_pos.y) / GRID_SIZE);
			z_result = CubicKernel((particle_pos.z - node_pos.z) / GRID_SIZE);
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
		{
			logger::Error("Linear MPM interpolation kernel not yet supported.\n");
			break;
		}
		case MPMInterpolationKernel::QUADRATIC:
		{
			float x_result = QuadraticKernel((particle_pos.x - node_pos.x) / GRID_SIZE);
			float y_result = QuadraticKernel((particle_pos.y - node_pos.y) / GRID_SIZE);
			float z_result = QuadraticKernel((particle_pos.z - node_pos.z) / GRID_SIZE);
			float ddx_x_result = QuadraticKernelDerivative((particle_pos.x - node_pos.x) / GRID_SIZE);
			float ddy_y_result = QuadraticKernelDerivative((particle_pos.y - node_pos.y) / GRID_SIZE);
			float ddz_z_result = QuadraticKernelDerivative((particle_pos.z - node_pos.z) / GRID_SIZE);
			// Equation after (124).
			gradient.x = ddx_x_result * y_result * z_result / GRID_SIZE;
			gradient.y = x_result * ddy_y_result * z_result / GRID_SIZE;
			gradient.z = x_result * y_result * ddz_z_result / GRID_SIZE;
			break;
		}
		case MPMInterpolationKernel::CUBIC:
		{
			float x_result = CubicKernel((particle_pos.x - node_pos.x) / GRID_SIZE);
			float y_result = CubicKernel((particle_pos.y - node_pos.y) / GRID_SIZE);
			float z_result = CubicKernel((particle_pos.z - node_pos.z) / GRID_SIZE);
			float ddx_x_result = CubicKernelDerivative((particle_pos.x - node_pos.x) / GRID_SIZE);
			float ddy_y_result = CubicKernelDerivative((particle_pos.y - node_pos.y) / GRID_SIZE);
			float ddz_z_result = CubicKernelDerivative((particle_pos.z - node_pos.z) / GRID_SIZE);
			gradient.x = ddx_x_result * y_result * z_result / GRID_SIZE;
			gradient.y = x_result * ddy_y_result * z_result / GRID_SIZE;
			gradient.z = x_result * y_result * ddz_z_result / GRID_SIZE;
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
		if (std::isnan(tmp[0][0]))
		{
			logger::Error("Piola-Kirchoff stress is nan. Aborting.\n");
			__debugbreak();
		}
		return tmp;
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
			d = (1.0f / 4.0f) * GRID_SIZE * GRID_SIZE; // From paragraph after equation (176).
			break;
		case MPMInterpolationKernel::CUBIC:
			d = (1.0f / 3.0f) * GRID_SIZE * GRID_SIZE; // From paragraph after equation (176).
			break;
		default:
			logger::Error("Unrecognized MPM interpolation kernel.\n");
		}

		return 1.0f / d;
	}

	glm::mat3 MPMContext::GetDInverseGeneralized(const glm::vec3& particle_pos) const
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

	uint32_t MPMContext::GridNodeCoordinateToIndex(const glm::uvec3& coord) const
	{
		uint32_t slice_area{ GRID_NODE_ROW_COUNT * GRID_NODE_ROW_COUNT };
		return coord.x + coord.y * GRID_NODE_ROW_COUNT + coord.z * slice_area;
	}

	// Get the 1D range of node coordinates given a position.
	void GetNodeCoordinateRange(float pos, uint32_t* out_min, uint32_t* out_max)
	{
		pos = std::clamp(pos, 0.0f, (float)GRID_NODE_ROW_COUNT);

		float pos_floor{ std::floorf(pos) };
		float pos_frac{ pos - pos_floor };
		uint32_t n{ (uint32_t)pos_floor };
		const uint32_t last_index{ GRID_NODE_ROW_COUNT - 1 };

		if (pos_frac <= 0.5f)
		{
			*out_min = (n == 0) ? 0 : n - 1;
			*out_max = (n == last_index) ? last_index : n + 1;
		}
		else
		{
			*out_min = n;
			*out_max = (n >= last_index - 1) ? last_index : n + 2;
		}
	}

	// Assumes radius of 1.5.
	std::array<uint32_t, MAXIMUM_NODES_IN_RANGE> MPMContext::GetNodeIndicesWithinRadius(glm::vec3 position, uint32_t* out_count) const
	{
		std::array<uint32_t, MAXIMUM_NODES_IN_RANGE> result{};
		position /= GRID_SIZE;

		uint32_t x_min{};
		uint32_t x_max{};
		uint32_t y_min{};
		uint32_t y_max{};
		uint32_t z_min{};
		uint32_t z_max{};
		GetNodeCoordinateRange(position.x, &x_min, &x_max);
		GetNodeCoordinateRange(position.y, &y_min, &y_max);
		GetNodeCoordinateRange(position.z, &z_min, &z_max);

		uint32_t result_idx{ 0 };
		for (uint32_t x{ x_min }; x <= x_max; ++x)
		{
			for (uint32_t y{ y_min }; y <= y_max; ++y)
			{
				for (uint32_t z{ z_min }; z <= z_max; ++z)
				{
					if (glm::length2(glm::vec3{ x, y, z } - position) < KERNEL_RADIUS_SQUARED) {
						result[result_idx++] = GridNodeCoordinateToIndex({ x, y, z });
					}
				}
			}
		}

		*out_count = result_idx;
		return result;
	}

	std::array<uint32_t, MAXIMUM_SUB_BLOCKS_IN_RANGE> MPMContext::GetParticleRangesWithinRadius(const glm::uvec3& grid_coord, uint32_t* out_count) const
	{
		std::array<uint32_t, MAXIMUM_SUB_BLOCKS_IN_RANGE> result{};
		glm::uvec3 node_sub_coord{ 2u * grid_coord }; // Coordinate into grid subdividing each of grid coordinates.

		uint32_t x_min{ (node_sub_coord.x < 3) ? 0 : node_sub_coord.x - 3 };
		uint32_t x_max{ (node_sub_coord.x + 2 >= SUB_BLOCK_ROW_COUNT) ? SUB_BLOCK_ROW_COUNT - 1 : node_sub_coord.x + 2 };
		uint32_t y_min{ (node_sub_coord.y < 3) ? 0 : node_sub_coord.y - 3 };
		uint32_t y_max{ (node_sub_coord.y + 2 >= SUB_BLOCK_ROW_COUNT) ? SUB_BLOCK_ROW_COUNT - 1 : node_sub_coord.y + 2 };
		uint32_t z_min{ (node_sub_coord.z < 3) ? 0 : node_sub_coord.z - 3 };
		uint32_t z_max{ (node_sub_coord.z + 2 >= SUB_BLOCK_ROW_COUNT) ? SUB_BLOCK_ROW_COUNT - 1 : node_sub_coord.z + 2 };

		uint32_t result_idx{ 0 };
		for (uint32_t i{ x_min }; i <= x_max; ++i)
		{
			for (uint32_t j{ y_min }; j <= y_max; ++j)
			{
				for (uint32_t k{ z_min }; k <= z_max; ++k)
				{
					glm::uvec3 sub_coord{ i, j, k };
					uint32_t particle_idx_idx{ sub_block_indices_[SubBlockCoordinateToIndex(sub_coord)] };
					if (particle_idx_idx != NULL_INDEX) {
						result[result_idx++] = particle_idx_idx;
					}
				}
			}
		}

		*out_count = result_idx;
		return result;
	}

	void MPMContext::PrintParticleWeights() const
	{
		logger::Print("h = %.2f\n", GRID_SIZE);

		for (uint32_t i{ 7 }; i <= 9; ++i)
		{
			for (uint32_t j{ 10 }; j <= 12; ++j)
			{
				for (uint32_t k{ 10 }; k <= 12; ++k)
				{
					const glm::vec3& node_position{ nodes_[GridNodeCoordinateToIndex(glm::uvec3{ i, j, k })].position };
					float weight{ GetWeight(node_position, particles_[0].position) };
					glm::vec3 grad_weight{ GetWeightGradient(node_position, particles_[0].position) };

					logger::Print("ijk: %u, %u, %u\n", i, j, k);
					logger::Print("weight: %.9f\n", weight);
					logger::Print("grad_weight: %.6f, %.6f, %.6f\n\n", grad_weight.x, grad_weight.y, grad_weight.z);
				}
			}
		}

	}

	bool MaterialPointIndex::operator<(const MaterialPointIndex& other)
	{
		return key < other.key;
	}
}
