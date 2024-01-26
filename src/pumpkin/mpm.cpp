#include "mpm.h"

#include <algorithm>
#include <execution>
#include <ranges>
#include "renderer_constants.h"
#include "logger.h"
#include "common_constants.h"
#include "tracy/Tracy.hpp"
#include "glm/gtx/norm.hpp"
#include "glm/gtc/matrix_inverse.hpp"

namespace pmk
{
	constexpr float KERNEL_RADIUS{ 1.5f };
	constexpr uint32_t SUB_BLOCK_ROW_COUNT{ GRID_NODE_ROW_COUNT * 2u };
	constexpr uint32_t SUB_BLOCK_COUNT{ SUB_BLOCK_ROW_COUNT * SUB_BLOCK_ROW_COUNT * SUB_BLOCK_ROW_COUNT };

	glm::uvec3 PositionToSubBlockCoordinate(glm::vec3 pos)
	{
		pos.x = std::clamp(pos.x, 0.0f, CHUNK_WIDTH);
		pos.y = std::clamp(pos.y, 0.0f, CHUNK_WIDTH);
		pos.z = std::clamp(pos.z, 0.0f, CHUNK_WIDTH);

		// Make 1 positional unit equal to 1 grid unit.
		pos /= GRID_SIZE;
		// Convert to sub block units.
		pos *= 2.0f;

		uint32_t x{ std::clamp((uint32_t)pos.x, 0u, SUB_BLOCK_ROW_COUNT - 1) };
		uint32_t y{ std::clamp((uint32_t)pos.y, 0u, SUB_BLOCK_ROW_COUNT - 1) };
		uint32_t z{ std::clamp((uint32_t)pos.z, 0u, SUB_BLOCK_ROW_COUNT - 1) };

		return { x, y, z };
	}

	// Returns coordinate into sub_block_indices_.
	uint32_t SubBlockCoordinateToIndex(const glm::uvec3& sub_coord)
	{
		constexpr uint32_t slice_area{ SUB_BLOCK_ROW_COUNT * SUB_BLOCK_ROW_COUNT };
		return sub_coord.x + sub_coord.y * SUB_BLOCK_ROW_COUNT + sub_coord.z * slice_area;
	}

	void MPMContext::Initialize(std::vector<MaterialPoint>&& particles, float chunk_width)
	{
		particles_ = std::move(particles);
		particle_cache_.clear();
		particle_cache_.resize(particles_.size());
		particle_indices_.clear();
		particle_indices_.resize(particles_.size());
		sub_block_indices_.clear();
		sub_block_indices_.resize(SUB_BLOCK_COUNT, NULL_INDEX);
		nodes_.resize(GRID_NODE_COUNT);

		for (ConstitutiveModel* constitutive_model : constitutive_models_) {
			constitutive_model->Initialize(this);
		}

		float particle_width = chunk_width / CHUNK_ROW_VOXEL_COUNT;
		particle_radius_ = 0.5f * particle_width;
		particle_initial_volume_ = particle_width * particle_width * particle_width;

		for (MaterialPoint& p : particles_)
		{
			ConstitutiveModel* model{ constitutive_models_[(uint32_t)p.constitutive_model_index] };
			model->InitializeParticle(&p, particle_initial_volume_);
		}

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
		ParticleToGrid();
		ComputeGridVelocities();
		UpdateLameParameters();
		ComputeExplicitGridForces();
		UpdateGridVelocity(delta_time);
		GridToParticle();
		UpdateParticleDeformationGradient(delta_time);
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

		{
			ZoneScopedN("Precompute particle values");
			float d_inverse{ GetDInverse() }; // For APIC transfer.
			std::ranges::iota_view particle_range{ std::views::iota(0u, (uint32_t)particles_.size()) };

			// Compute and cache computation needed for each particle.
			std::for_each(
				std::execution::par,
				particle_range.begin(),
				particle_range.end(),
				[&](uint32_t i)
				{
					MaterialPoint& p{ particles_[i] };
					particle_cache_[i].p2g_lhs = p.mass * (p.velocity - p.affine_matrix * d_inverse * p.position);
					particle_cache_[i].p2g_rhs = p.mass * p.affine_matrix * d_inverse;
				});
		}

		{
			ZoneScopedN("P2G");
			std::for_each(
				std::execution::par,
				nodes_.begin(),
				nodes_.end(),
				[&](auto& node)
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
							MaterialPoint& p{ particles_[particle_indices_[j].index] };
							ParticleCache& c{ particle_cache_[particle_indices_[j].index] };

							float weight{ GetWeight(node.position, p.position) };
							node.mass += weight * p.mass;                                      // Equation (172).
							node.momentum += weight * (c.p2g_lhs + c.p2g_rhs * node.position); // Equation (173).
						}
					}
				});
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
		const float d_inverse{ GetDInverse() };

		ZoneScoped;
		// Compute and cache computation needed for each particle.
		auto range{ std::views::iota(0u, (uint32_t)particles_.size()) };
		std::for_each(
			std::execution::par,
			range.begin(),
			range.end(),
			[&](uint32_t i)
			{
				MaterialPoint& p{ particles_[i] };
				particle_cache_[i].stress = constitutive_models_[(uint32_t)p.constitutive_model_index]->GetJCauchyStress(p);
			});

		std::for_each(
			std::execution::par,
			nodes_.begin(),
			nodes_.end(),
			[&](auto& node)
			{
				if (node.mass == 0.0f) {
					return;
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
						ParticleCache& c{ particle_cache_[particle_indices_[j].index] };
						sum += GetWeight(node.position, p.position) * c.stress * (node.position - p.position);
					}
				}

				//node.force = -particle_initial_volume_ * d_inverse * sum; // Equation (18) of MLS-MPM. Replaces equation (189) of UCLA paper.
				node.force = -d_inverse * sum; // Equation (18) of MLS-MPM. Replaces equation (189) of UCLA paper.
				node.force += node.mass * glm::vec3{ 0.0f, -9.8f, 0.0f }; // Gravity?

				if (std::isnan(node.force.x))
				{
					logger::Error("Node force is nan. Aborting.\n");
					__debugbreak();
				}
			});
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

			if ((node.coordinate.x == 0 && node.velocity.x < 0.0f) || (node.coordinate.x == GRID_NODE_ROW_COUNT - 1 && node.velocity.x > 0.0f)) {
				node.velocity.x = 0.0f;
			}
			if ((node.coordinate.y == 0 && node.velocity.y < 0.0f) || (node.coordinate.y == GRID_NODE_ROW_COUNT - 1 && node.velocity.y > 0.0f)) {
				node.velocity.y = 0.0f;
			}
			if ((node.coordinate.z == 0 && node.velocity.z < 0.0f) || (node.coordinate.z == GRID_NODE_ROW_COUNT - 1 && node.velocity.z > 0.0f)) {
				node.velocity.z = 0.0f;
			}
		}
	}

	void MPMContext::UpdateParticleDeformationGradient(float delta_time)
	{
		ZoneScoped;
		const float d_inverse{ GetDInverse() };

		std::for_each(
			std::execution::par,
			particles_.begin(),
			particles_.end(),
			[&](auto& p)
			{
				constitutive_models_[(uint32_t)p.constitutive_model_index]->UpdateDeformationGradient(&p, d_inverse, delta_time);
			});
	}

	void MPMContext::GridToParticle()
	{
		ZoneScoped;
		std::for_each(
			std::execution::par,
			particles_.begin(),
			particles_.end(),
			[&](MaterialPoint& p)
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
					p.velocity += w * node.velocity;                                                     // Equation (175).
					p.affine_matrix += glm::outerProduct(w * node.velocity, node.position - p.position); // Equation (176).
				}
			});
	}

	void MPMContext::AdvectParticles(float delta_time)
	{
		ZoneScoped;
		std::for_each(
			std::execution::par,
			particle_indices_.begin(),
			particle_indices_.end(),
			[&](auto& mi)
			{
				MaterialPoint& p{ particles_[mi.index] };
				constexpr float epsilon{ 0.01f };

				p.position += delta_time * p.velocity;
				p.position.x = std::clamp(p.position.x, 0.0f, CHUNK_WIDTH - epsilon);
				p.position.y = std::clamp(p.position.y, 0.0f, CHUNK_WIDTH - epsilon);
				p.position.z = std::clamp(p.position.z, 0.0f, CHUNK_WIDTH - epsilon);

				mi.key = SubBlockCoordinateToIndex(PositionToSubBlockCoordinate(p.position));
			});
	}

	void MPMContext::UpdateIndexBuffers()
	{
		ZoneScoped;
		{
			ZoneScopedN("Sort");
			std::sort(particle_indices_.begin(), particle_indices_.end());
		}

		{
			ZoneScopedN("Update keys");
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
			sum += GetWeight(node.position, particle_pos) * glm::outerProduct(node.position - particle_pos, node.position - particle_pos);
		}
		return sum;
	}

	void MPMContext::UpdateLameParameters()
	{
		ZoneScoped;

		std::for_each(
			std::execution::par,
			particles_.begin(),
			particles_.end(),
			[&](auto& p)
			{
				constitutive_models_[(uint32_t)p.constitutive_model_index]->UpdateLameParameters(&p);
			});
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
		pos = std::clamp(pos, 0.0f, (float)(GRID_NODE_ROW_COUNT));

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
					result[result_idx++] = GridNodeCoordinateToIndex({ x, y, z });
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


	// Constitutive models -------------------------------------------------------------------------------------------

	void ConstitutiveModel::Initialize(MPMContext* mpm_context)
	{
		mpm_context_ = mpm_context;
	}

	glm::mat3 HyperElasticModel::GetJCauchyStress(MaterialPoint& p) const
	{
		return GetPiolaKirchoffStress(p) * glm::transpose(p.deformation_gradient_elastic);
	}

	void HyperElasticModel::UpdateLameParameters(MaterialPoint* p) const
	{
		// Hyper elastic model does not change Lame parameters.
	}

	void HyperElasticModel::InitializeParticle(MaterialPoint* p, float initial_volume) const
	{
		p->lambda = LAMBDA;
		p->mu = MU;
		p->mass = initial_volume * DENSITY;
	}

	void HyperElasticModel::UpdateDeformationGradient(MaterialPoint* p, float d_inverse, float delta_time) const
	{
		p->deformation_gradient_elastic = (glm::mat3(1.0f) + delta_time * d_inverse * p->affine_matrix) * p->deformation_gradient_elastic; // Equation (17) from MLS-MPM. Replaces equation (181) from UCLA paper.
	}

	glm::mat3 HyperElasticModel::GetPiolaKirchoffStress(MaterialPoint& p) const
	{
		glm::mat3 f_inv_transpose{ glm::inverseTranspose(p.deformation_gradient_elastic) };
		float j{ glm::determinant(p.deformation_gradient_elastic) };
		auto tmp = p.mu * (p.deformation_gradient_elastic - f_inv_transpose) + p.lambda * std::logf(j) * f_inv_transpose; // Equation (48).
		if (std::isnan(tmp[0][0]))
		{
			logger::Error("Piola-Kirchoff stress is nan. Aborting.\n");
			__debugbreak();
		}
		return tmp;
	}

	glm::mat3 FluidModel::GetJCauchyStress(MaterialPoint& p) const
	{
		constexpr float GRID_CELL_VOLUME{ GRID_SIZE * GRID_SIZE * GRID_SIZE };
		float density{ 0.0f };

		uint32_t node_index_count{};
		auto node_indices{ mpm_context_->GetNodeIndicesWithinRadius(p.position, &node_index_count) };
		for (uint32_t i{ 0 }; i < node_index_count; ++i)
		{
			GridNode& node{ mpm_context_->nodes_[node_indices[i]] };
			if (node.mass == 0.0f) {
				continue;
			}
			float w{ mpm_context_->GetWeight(node.position, p.position) };

			float v{ GRID_CELL_VOLUME };
			uint32_t boundary_count{};
			boundary_count += (node.coordinate.x == 0 || node.coordinate.x == GRID_NODE_ROW_COUNT - 1) ? 1 : 0;
			boundary_count += (node.coordinate.y == 0 || node.coordinate.y == GRID_NODE_ROW_COUNT - 1) ? 1 : 0;
			boundary_count += (node.coordinate.z == 0 || node.coordinate.z == GRID_NODE_ROW_COUNT - 1) ? 1 : 0;
			switch (boundary_count)
			{
			case 0:
				break;
			case 1:
				v *= 0.5f;
				break;
			case 2:
				v *= 0.25f;
				break;
			case 3:
				v *= 0.125f;
				break;
			}
			density += (node.mass * w) / v;
		}

		// stress = -pressure * I + viscosity * (velocity_gradient + velocity_gradient_transposed)

		//float volume{ p.mass / density };
		float pressure = std::max(-0.1f, EOS_STIFFNESS * (std::powf((density) / 30.0f, EOS_POWER) - 1.0f));

		//constexpr float pressure_multiplier{ 2.0f };
		//float density_error{ density - 30.0f };
		//float pressure{ std::max(-0.1f, density_error * pressure_multiplier) };

		// For debug rendering.
		p.mu = pressure;
		p.lambda = density;

		// velocity gradient - MLS-MPM eq. 17, where derivative of quadratic polynomial is linear
		glm::mat3 velocity_gradient = p.affine_matrix * mpm_context_->GetDInverse();
		glm::mat3 stress{ -pressure * glm::mat3(1.0f) + DYNAMIC_VISCOSITY * (velocity_gradient + glm::transpose(velocity_gradient)) };

		// Testing this... Replaces the initiail volume that the caller multiplies with the return value of this function.
		float volume = p.mass / density;

		// J is assumed to be 1.0f since fluid is incompressible, so no need to multiply by it.
		return volume * stress;
	}

	void FluidModel::UpdateLameParameters(MaterialPoint* p) const
	{
		// No-op.
	}

	void FluidModel::InitializeParticle(MaterialPoint* p, float initial_volume) const
	{
		p->mass = initial_volume * REST_DENSITY;
	}

	void FluidModel::UpdateDeformationGradient(MaterialPoint* p, float d_inverse, float delta_time) const
	{
		// No-op. Fluid doesn't use the deformation gradient when calculating stress.
	}

	glm::mat3 SnowModel::GetJCauchyStress(MaterialPoint& p) const
	{
		glm::mat3 r{};
		glm::mat3 s{};
		PolarDecomposition(p.deformation_gradient_elastic, &r, &s);
		glm::mat3 lhs{ 2.0f * p.mu * p.deformation_gradient_plastic * (p.deformation_gradient_elastic - r) * glm::transpose(p.deformation_gradient_elastic) };

		float je{ glm::determinant(p.deformation_gradient_elastic) };
		glm::mat3 rhs{ p.lambda * p.deformation_gradient_plastic * (je - 1.0f) * je };
		return lhs + rhs;
	}

	void SnowModel::UpdateLameParameters(MaterialPoint* p) const
	{
		// Snow hardening.
		float j_p{ glm::determinant(p->deformation_gradient_plastic) };
		float hardening_exponential{ std::expf(HARDENING_PARAMETER * (1.0f - j_p)) };
		hardening_exponential = std::min(1.1f, hardening_exponential);
		p->mu = MU * hardening_exponential;
		p->lambda = LAMBDA * hardening_exponential;
	}

	void SnowModel::InitializeParticle(MaterialPoint* p, float initial_volume) const
	{
		p->lambda = LAMBDA;
		p->mu = MU;
		p->mass = initial_volume * DENSITY;
	}

	void SnowModel::UpdateDeformationGradient(MaterialPoint* p, float d_inverse, float delta_time) const
	{
		glm::mat3 tentative_elastic = (glm::mat3(1.0f) + delta_time * d_inverse * p->affine_matrix) * p->deformation_gradient_elastic; // Equation (17) from MLS-MPM. Replaces equation (181) from UCLA paper.
		glm::mat3 deformation_gradient{ tentative_elastic * p->deformation_gradient_plastic };

		glm::mat3 u{};
		glm::mat3 s{};
		glm::mat3 v{};
		SingularValueDecomposition(tentative_elastic, &u, &s, &v);

		s[0][0] = std::clamp(s[0][0], 1.0f - SIGMA_C, 1.0f + SIGMA_S);
		s[1][1] = std::clamp(s[1][1], 1.0f - SIGMA_C, 1.0f + SIGMA_S);
		s[2][2] = std::clamp(s[2][2], 1.0f - SIGMA_C, 1.0f + SIGMA_S);

		p->deformation_gradient_elastic = u * s * glm::transpose(v);
		p->deformation_gradient_plastic = glm::inverse(p->deformation_gradient_elastic) * deformation_gradient;
	}
}
