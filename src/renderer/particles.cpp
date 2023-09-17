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

	void GenerateStaticParticleMesh(const std::vector<StaticParticle>& particles, const std::vector<uint8_t>& side_flags, float particle_width)
	{
		// TODO: Experiment and measure speed with reinterpretting particles as uint64_t.
		constexpr uint64_t empty_particles{ 0 };
		constexpr uint64_t all_sides{ 0x3F3f3f3f3F3f3f3f };

	}
}
