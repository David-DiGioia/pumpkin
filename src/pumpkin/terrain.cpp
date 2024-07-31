#include "terrain.h"

#include <cstdint>
#include "scene.h"

namespace pmk
{
	constexpr uint32_t CHUNK_X_COUNT{ 4 };
	constexpr uint32_t CHUNK_Y_COUNT{ 4 };
	constexpr uint32_t CHUNK_COUNT{ CHUNK_X_COUNT * CHUNK_Y_COUNT };

	void Chunk::Initialize(Node* node, renderer::VulkanRenderer* renderer, XPBDParticleContext* xpbd_context)
	{
		node_ = node;
		renderer_ = renderer;
		xpbd_context_ = xpbd_context;
	}

	void Chunk::CleanUp()
	{
	}

	void Chunk::GenerateVoxels()
	{
		renderer_->InvokeParticleGenShader(node_->render_object, &rb_.voxel_chunk.GetVoxels(), &rb_.voxel_chunk.GetSideFlags());
		GenerateStaticParticleMesh(node_->render_object);
		renderer_->UpdateMaterials();

		generated_voxel_count_ = 0;
		for (uint32_t i{ 0 }; i < rb_.voxel_chunk.VoxelCount(); ++i)
		{
			if (!rb_.voxel_chunk.IsEmpty(i)) {
				++generated_voxel_count_;
			}
		}
	}

	void Chunk::GenerateStaticParticleMesh(renderer::RenderObjectHandle ro_target)
	{
		renderer_->GenerateStaticParticleMesh(ro_target, rb_.voxel_chunk);
	}

	void Chunk::TransferVoxelsToXPBD()
	{
		uint32_t particle_count{ 0 };
		for (uint32_t i{ 0 }; i < rb_.voxel_chunk.VoxelCount(); ++i)
		{
			if (rb_.voxel_chunk.IsEmpty(i)) {
				continue;
			}

			++particle_count;
		}

		std::vector<XPBDParticle> xpbd_particles{};
		xpbd_particles.reserve(particle_count);

		uint32_t position_idx{ 0 };
		for (uint32_t i{ 0 }; i < rb_.voxel_chunk.VoxelCount(); ++i)
		{
			if (rb_.voxel_chunk.IsEmpty(i)) {
				continue;
			}

			glm::uvec3 coord{ rb_.voxel_chunk.IndexToCoordinate(i) };
			glm::vec3 pos{ PARTICLE_WIDTH * glm::vec3{ coord } };;

			XPBDParticle xpbd_particle{
				.key = {}, // Set later.
				.velocity = glm::vec3{0.0f, 0.0f, 0.0f},
				.physics_material_index = rb_.voxel_chunk.Index(i).physics_material_index,
				.s = {
					.position = pos,
					.predicted_position = pos,
					.inverse_mass = {}, // Set later.
				},
				.debug_color = {}, // Set later.
			};

			xpbd_particles.push_back(xpbd_particle);
		}

		xpbd_context_->AddParticles(std::move(xpbd_particles));
	}

	void Chunk::DestroyRenderObject()
	{
		if (node_)
		{
			renderer_->QueueDestroyRenderObject(node_->render_object);
			node_->render_object = renderer::NULL_HANDLE;
		}
	}

	void Terrain::Initialize(Scene* scene, renderer::VulkanRenderer* renderer, XPBDParticleContext* xpbd_context)
	{
		scene_ = scene;
		renderer_ = renderer;
		xpbd_context_ = xpbd_context;

		CreateChunks();
	}

	void Terrain::CleanUp()
	{
		for (Chunk& chunk : chunks_) {
			chunk.CleanUp();
		}
	}

	void Terrain::GenerateVoxels()
	{
		for (Chunk& chunk : chunks_) {
			chunk.GenerateVoxels();
		}
	}

	void Terrain::CreateChunks()
	{
		chunks_.resize(CHUNK_COUNT);

		for (uint32_t x{ 0 }; x < CHUNK_X_COUNT; ++x)
		{
			for (uint32_t y{ 0 }; y < CHUNK_Y_COUNT; ++y)
			{
				Node* node{ scene_->CreateNode() };
				node->position = glm::vec3{ x * CHUNK_WIDTH, y * CHUNK_WIDTH, 0.0f };

				Chunk chunk{};
				chunk.Initialize(node, renderer_, xpbd_context_);
				chunks_.push_back(chunk);
			}
		}
	}
}
