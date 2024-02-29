#include "rigid_body.h"

#include <algorithm>
#include <queue>

namespace pmk
{
	// Static particle coordinate to index.
	uint32_t ToIndex(const glm::uvec3& coord)
	{
		uint32_t slice_area{ CHUNK_ROW_VOXEL_COUNT * CHUNK_ROW_VOXEL_COUNT };
		return coord.x + coord.y * CHUNK_ROW_VOXEL_COUNT + coord.z * slice_area;
	}

	std::vector<std::pair<float*, std::string>> RigidBodyModel::GetParameters()
	{
		return {};
	}

	void RigidBodyModel::OnParametersMutated()
	{
		// No-op.
	}

	void RigidBodyContext::Initialize(Scene* scene, const std::vector<PhysicsMaterial*>* physics_materials)
	{
		scene_ = scene;
		physics_materials_ = physics_materials;
	}

	void RigidBodyContext::CleanUp()
	{
	}

	// Returns true when the given coordinate is inside the region that flood fill is filling.
	bool FloodFillInside(const glm::uvec3& coord, std::vector<renderer::StaticParticle>& particles, const std::vector<uint8_t>& material_mask)
	{
		// We only check upper condition since negative uints will overflow anyway.
		bool x_out_bounds{ coord.x >= CHUNK_ROW_VOXEL_COUNT };
		bool y_out_bounds{ coord.y >= CHUNK_ROW_VOXEL_COUNT };
		bool z_out_bounds{ coord.z >= CHUNK_ROW_VOXEL_COUNT };

		if (x_out_bounds || y_out_bounds || z_out_bounds) {
			return false;
		}

		uint32_t idx{ particles[ToIndex(coord)].physics_material_index };
		return idx == renderer::PHYSICS_MATERIAL_EMPTY_INDEX ? false : material_mask[idx];
	}

	RigidBodyVoxel FloodFillSet(const glm::uvec3& coordinate, std::vector<renderer::StaticParticle>& particles)
	{
		uint32_t idx{ ToIndex(coordinate) };
		RigidBodyVoxel voxel{
			.coordinate = coordinate,
			.physics_material_index = particles[idx].physics_material_index
		};
		particles[idx].physics_material_index = renderer::PHYSICS_MATERIAL_EMPTY_INDEX;
		return voxel;
	}

	void FloodFillScan(
		uint32_t lx,
		uint32_t rx,
		uint32_t y,
		uint32_t z,
		std::queue<glm::uvec3>& queue,
		std::vector<renderer::StaticParticle>& particles,
		const std::vector<uint8_t>& material_mask)
	{
		bool span_added{ false };

		for (uint32_t x{ lx }; x < rx; ++x)
		{
			if (!FloodFillInside({ x, y, z }, particles, material_mask)) {
				span_added = false;
			}
			else if (!span_added)
			{
				queue.push({ x, y, z });
				span_added = true;
			}
		}
	}

	// Return list of all voxels of specified materials connected to the voxel at coord. Replaces found voxels with empty voxel in input particles.
	std::vector<RigidBodyVoxel> RigidBodyFloodFill(const glm::uvec3& coordinate, std::vector<renderer::StaticParticle>& particles, const std::vector<uint8_t>& material_mask)
	{
		std::vector<RigidBodyVoxel> out_voxels{};
		std::queue<glm::uvec3> queue{};
		queue.push(coordinate);

		while (!queue.empty())
		{
			glm::uvec3 coord{ queue.front() };
			queue.pop();
			uint32_t lx{ coord.x };

			while (FloodFillInside({ lx - 1, coord.y, coord.z }, particles, material_mask))
			{
				out_voxels.push_back(FloodFillSet({ lx - 1, coord.y, coord.z }, particles));
				--lx;
			}

			while (FloodFillInside(coord, particles, material_mask))
			{
				out_voxels.push_back(FloodFillSet(coord, particles));
				++coord.x;
			}

			FloodFillScan(lx, coord.x, coord.y + 1, coord.z, queue, particles, material_mask);
			FloodFillScan(lx, coord.x, coord.y - 1, coord.z, queue, particles, material_mask);
			FloodFillScan(lx, coord.x, coord.y, coord.z + 1, queue, particles, material_mask);
			FloodFillScan(lx, coord.x, coord.y, coord.z - 1, queue, particles, material_mask);
		}

		return out_voxels;
	}

	void RigidBodyContext::CreateRigidBodiesByConnectedness(std::vector<renderer::StaticParticle>& particles)
	{
		rigid_bodies_.clear();

		// Create a mask to quickly test if a static particle has a rigid body material.
		std::vector<uint8_t> rigid_body_mask(physics_materials_->size());
		std::transform(physics_materials_->begin(), physics_materials_->end(), rigid_body_mask.begin(),
			[](const pmk::PhysicsMaterial* m) { return dynamic_cast<const RigidBodyModel*>(m->constitutive_model) != nullptr; });

		for (uint32_t i{ 0 }; i < CHUNK_ROW_VOXEL_COUNT; ++i)
		{
			for (uint32_t j{ 0 }; j < CHUNK_ROW_VOXEL_COUNT; ++j)
			{
				for (uint32_t k{ 0 }; k < CHUNK_ROW_VOXEL_COUNT; ++k)
				{
					renderer::StaticParticle& p{ particles[ToIndex({i, j, k})] };

					if (p.physics_material_index == renderer::PHYSICS_MATERIAL_EMPTY_INDEX) {
						continue;
					}

					if (rigid_body_mask[p.physics_material_index])
					{
						RigidBody* rigid_body{ new RigidBody{} };
						rigid_body->voxels = RigidBodyFloodFill({ i, j, k }, particles, rigid_body_mask);
						rigid_bodies_.push_back(rigid_body);
					}
				}
			}
		}
	}
}
