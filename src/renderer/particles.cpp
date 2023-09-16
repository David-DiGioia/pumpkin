#include "particles.h"

namespace renderer
{
	glm::uvec3 ParticleIndexToCoordinate(uint32_t index)
	{
		uint32_t slice_area{ PARTICLE_CHUNK_SIZE * PARTICLE_CHUNK_SIZE };
		uint32_t z{ index / slice_area };
		uint32_t y{ (index % slice_area) / PARTICLE_CHUNK_SIZE };
		uint32_t x{ index % PARTICLE_CHUNK_SIZE };

		return glm::uvec3{ x, y, z };
	}

	void GenerateStaticParticleMesh(const std::vector<StaticParticle>& particles, float particle_width)
	{

	}
}
