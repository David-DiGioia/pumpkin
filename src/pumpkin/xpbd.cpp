#include "xpbd.h"

#include <cmath>
#include <bit>
#include <execution>
#include <ranges>
#include <thread>
#include <immintrin.h>  // header file for AVX2 intrinsics.
#include "tracy/Tracy.hpp"
#include "glm/gtx/norm.hpp"

#include "common_constants.h"
#include "physics.h"
#include "rigid_body.h"
#include "scene.h"

namespace pmk
{
	constexpr uint32_t HASH_TABLE_SIZE{ 262144 }; // Power of two, 2^18, makes it fast to take modulo using bitwise and.
	constexpr float GRID_SPACING{ PARTICLE_WIDTH };
	constexpr float SPH_KERNEL_RADIUS{ GRID_SPACING };
	constexpr float SPH_KERNEL_RADIUS_SQUARED{ SPH_KERNEL_RADIUS * SPH_KERNEL_RADIUS };

#ifdef EDITOR_ENABLED
	static glm::vec3 Heatmap(float val, float lower, float upper)
	{
		val = glm::clamp(val, lower, upper);
		// Map val from [lower, upper] to [0, 1].
		val = (val - lower) / upper - lower;
		// Map val from [0, 1] to [0, pi / 2].
		val *= PI / 2.0f;

		return glm::vec3(glm::sin(val), glm::sin(val * 2.0f), glm::cos(val));
	}

	static glm::vec3 UniqueColor(uint64_t seed)
	{
		seed += (seed << 10u);
		seed ^= (seed >> 6u);
		seed += (seed << 3u);
		seed ^= (seed >> 11u);
		seed += (seed << 15u);
		return Heatmap((seed % 10000) / 10000.0f, 0.0f, 1.0f);
	}
#endif

	static __m256i InterleaveBitsSIMD(__m256i& x, __m256i& y, __m256i& z)
	{
		x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 16)), _mm256_set1_epi32(0xFF0000FF));
		x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 8)), _mm256_set1_epi32(0x0F00F00F));
		x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 4)), _mm256_set1_epi32(0xC30C30C3));
		x = _mm256_and_si256(_mm256_or_si256(x, _mm256_slli_epi32(x, 2)), _mm256_set1_epi32(0x49249249));

		y = _mm256_and_si256(_mm256_or_si256(y, _mm256_slli_epi32(y, 16)), _mm256_set1_epi32(0xFF0000FF));
		y = _mm256_and_si256(_mm256_or_si256(y, _mm256_slli_epi32(y, 8)), _mm256_set1_epi32(0x0F00F00F));
		y = _mm256_and_si256(_mm256_or_si256(y, _mm256_slli_epi32(y, 4)), _mm256_set1_epi32(0xC30C30C3));
		y = _mm256_and_si256(_mm256_or_si256(y, _mm256_slli_epi32(y, 2)), _mm256_set1_epi32(0x49249249));

		z = _mm256_and_si256(_mm256_or_si256(z, _mm256_slli_epi32(z, 16)), _mm256_set1_epi32(0xFF0000FF));
		z = _mm256_and_si256(_mm256_or_si256(z, _mm256_slli_epi32(z, 8)), _mm256_set1_epi32(0x0F00F00F));
		z = _mm256_and_si256(_mm256_or_si256(z, _mm256_slli_epi32(z, 4)), _mm256_set1_epi32(0xC30C30C3));
		z = _mm256_and_si256(_mm256_or_si256(z, _mm256_slli_epi32(z, 2)), _mm256_set1_epi32(0x49249249));

		// Combine the results.
		return _mm256_or_si256(_mm256_or_si256(x, _mm256_slli_epi32(y, 1)), _mm256_slli_epi32(z, 2));
	}

	// Only the first 21 bits of each input are used.
	static uint64_t InterleaveBits(const glm::uvec3& c)
	{
		uint32_t x{ c.x };
		uint32_t y{ c.y };
		uint32_t z{ c.z };
		x = (x | (x << 16)) & 0xFF0000FF;
		x = (x | (x << 8)) & 0x0F00F00F;
		x = (x | (x << 4)) & 0xC30C30C3;
		x = (x | (x << 2)) & 0x49249249;

		y = (y | (y << 16)) & 0xFF0000FF;
		y = (y | (y << 8)) & 0x0F00F00F;
		y = (y | (y << 4)) & 0xC30C30C3;
		y = (y | (y << 2)) & 0x49249249;

		z = (z | (z << 16)) & 0xFF0000FF;
		z = (z | (z << 8)) & 0x0F00F00F;
		z = (z | (z << 4)) & 0xC30C30C3;
		z = (z | (z << 2)) & 0x49249249;

		return x | ((uint64_t)y << 1) | ((uint64_t)z << 2);
	}

	static glm::uvec3 PositionToCoordinate(const glm::vec3& position)
	{
		// Positive only coordinates, so we offset coordinates so that they are valid in every direction of origin.
		constexpr int32_t coord_offset{ 1000000 };

		return glm::uvec3{
			(uint32_t)((int32_t)std::floorf(position.x / GRID_SPACING) + 1000000),
			(uint32_t)((int32_t)std::floorf(position.y / GRID_SPACING) + 1000000),
			(uint32_t)((int32_t)std::floorf(position.z / GRID_SPACING) + 1000000),
		};
	}

	static __m256i HashCoordsSIMD(__m256i& x, __m256i& y, __m256i& z)
	{
		__m256i interleaved{ InterleaveBitsSIMD(x, y, z) };
		return _mm256_and_si256(interleaved, _mm256_set1_epi32(HASH_TABLE_SIZE - 1)); // Modulo implemented with bitwise and.
	}

	static uint32_t HashCoords(const glm::uvec3& coord)
	{
		return InterleaveBits(coord) % HASH_TABLE_SIZE;
	}

	static uint32_t HashPosition(const glm::vec3& pos)
	{
		return HashCoords(PositionToCoordinate(pos));
	}

	// TODO: Maybe replace kernel and gradient with lookup table.
	// From https://pysph.readthedocs.io/en/latest/reference/kernels.html.
	static float SPHKernel(float q)
	{
		constexpr float h{ SPH_KERNEL_RADIUS / 2.0f };
		constexpr float sigma_3{ 1.0f / (PI * h * h * h) };
		q /= h;

		if (q <= 1.0f) {
			return sigma_3 * (1.0f - 1.5f * q * q * (1.0f - 0.5f * q));
		}
		else if (q <= 2.0f)
		{
			float a{ (2.0f - q) };
			return 0.25f * sigma_3 * a * a * a;
		}

		return 0.0f;
	}

	// Calculated using Wolfram Alpha.
	static glm::vec3 SPHKernelGradient(glm::vec3 q)
	{
		constexpr float h{ SPH_KERNEL_RADIUS / 2.0f };
		constexpr float sigma_3{ 1.0f / (PI * h * h * h) };
		q /= h;

		float q_length{ glm::length(q) };

		if (q_length != 0.0f)
		{
			glm::vec3 n{ q / q_length };

			if (q_length <= 1.0f) {
				return n * ((sigma_3 * q_length * (2.25f * q_length - 3.0f)) / h);
			}
			else if (q_length <= 2.0f)
			{
				float a{ q_length - 2.0f };
				return n * ((-0.75f * sigma_3 * a * a) / h);
			}
		}

		return glm::vec3{ 0.0f, 0.0f, 0.0f };
	}

	void XPBDParticleContext::Initialize(
		std::vector<XPBDParticle>&& particles,
		float chunk_width,
		const std::vector<XPBDConstraint*>* jacobi_constraints,
		const std::vector<PhysicsMaterial*>* physics_materials)
	{
		particles_ = std::move(particles);
		jacobi_constraints_ = jacobi_constraints;
		physics_materials_ = physics_materials;
		rb_collisions_.clear();
		rb_collisions_.resize(particles_.size());
		particle_keys_.clear();
		particle_keys_.resize(particles_.size());
		particle_ranges_.clear();
		particle_ranges_.resize(particles_.size());
		hash_table_.clear();
		hash_table_.resize(HASH_TABLE_SIZE, NULL_INDEX);

		// Create cache optimal particles.
		particles_stripped_ = static_cast<XPBDParticleStripped*>(::operator new[](particles_.size() * sizeof(XPBDParticleStripped), std::align_val_t{ CL_SIZE }));
#if GAUSS_SEIDEL_WITHIN_CHUNK
		particles_scratch_ = static_cast<XPBDParticleStripped*>(::operator new[](particles_.size() * sizeof(XPBDParticleStripped), std::align_val_t{ CL_SIZE }));
#endif

		float particle_width = chunk_width / CHUNK_ROW_VOXEL_COUNT;
		particle_radius_ = 0.5f * particle_width;
		particle_initial_volume_ = particle_width * particle_width * particle_width;

		// Initialize particle mass and index buffer.
		for (XPBDParticle& p : particles_)
		{
			p.s.inverse_mass = 1.0f / (GetPhysicsMaterial(p)->density * particle_initial_volume_);
			p.key = HashPosition(p.position);
		}

		UpdateIndexBuffers();
	}

	void XPBDParticleContext::CleanUp()
	{
		::operator delete[](particles_stripped_, std::align_val_t{ CL_SIZE });
	}

	void XPBDParticleContext::SimulateStep(float delta_time, const XPBDRigidBodyContext* rb_context)
	{
		ZoneScoped;

		constexpr uint32_t iterations{ 3 };

		// Zero out rigid body-particle collisions.
		std::memset(rb_collisions_.data(), 0, rb_collisions_.size() * sizeof(RigidBodyParticleCollisionInfo));

		ApplyForces(delta_time);
		{
			PrecomputeParticleRanges();

			ZoneScopedN("Solve all constraints");
			for (uint32_t i{ 0 }; i < iterations; ++i)
			{
				// Not needed first iteration since it's copied in UpdateIndexBuffers().
				if (i != 0) {
					CopyPositions();
				}

				SolveConstraints(delta_time, rb_context);
			}
		}
		UpdateVelocityAndInternalForces(delta_time);

		UpdateIndexBuffers();
	}

	const std::vector<XPBDParticle>& XPBDParticleContext::GetParticles() const
	{
		return particles_;
	}

	std::vector<XPBDParticle>& XPBDParticleContext::GetParticles()
	{
		return particles_;
	}

	const XPBDParticleStripped* XPBDParticleContext::GetParticlesStripped() const
	{
		return particles_stripped_;
	}

	uint32_t XPBDParticleContext::GetParticleCount() const
	{
		return (uint32_t)particles_.size();
	}

#if GAUSS_SEIDEL_WITHIN_CHUNK
	const XPBDParticleStripped* XPBDParticleContext::GetParticlesScratch() const
	{
		return particles_scratch_;
	}
#endif

	const std::vector<uint32_t>& XPBDParticleContext::GetParticleKeys() const
	{
		return particle_keys_;
	}

	RigidBodyParticleCollisionInfo& XPBDParticleContext::GetRigidBodyCollision(uint32_t particle_idx)
	{
		return rb_collisions_[particle_idx];
	}

	void XPBDParticleContext::ApplyForces(float delta_time)
	{
		ZoneScoped;
		for (XPBDParticle& p : particles_)
		{
			// Apply forces.
			p.velocity += delta_time * glm::vec3{ 0.0f, -9.8f, 0.0f }; // Gravity.

			// Predict position.
			p.s.predicted_position = p.position + delta_time * p.velocity;
		}
	}

	void XPBDParticleContext::PrecomputeParticleRanges()
	{
		auto indices{ std::views::iota(0u, (uint32_t)particles_.size()) };
		std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
			[&](uint32_t i) {
				particle_ranges_[i] = GetParticleRangesWithinKernelSIMD(particles_stripped_[i].predicted_position);
			});
	}

	void XPBDParticleContext::SolveConstraints(float delta_time, const XPBDRigidBodyContext* rb_context)
	{
		ZoneScoped;

		{
			ZoneScopedN("Parallel solve collisions");
			// Jacobi iterations.
			constexpr uint32_t chunk_size{ 512 };
			const uint32_t chunk_count{ ((uint32_t)particles_.size() + chunk_size - 1) / chunk_size }; // Round up.
			auto chunk_indices{ std::views::iota(0u, chunk_count) };
			std::for_each(std::execution::par_unseq, chunk_indices.begin(), chunk_indices.end(),
				[&](uint32_t chunk_idx) {
					uint32_t begin{ chunk_idx * chunk_size };
					uint32_t end{ std::min(begin + chunk_size, (uint32_t)particles_.size()) };
					for (uint32_t i{ begin }; i < end; ++i)
					{
						PhysicsMaterial* mat{ GetPhysicsMaterial(particles_[i]) };
						glm::vec3 p1_pos{ particles_scratch_[i].predicted_position };

						for (uint32_t j{ 0 }; j < (uint32_t)jacobi_constraints_->size(); ++j)
						{
							if (mat->jacobi_constraints_mask & (1 << j)) {
								glm::vec3 delta_x{ (*jacobi_constraints_)[j]->Solve(this, rb_context, i, delta_time, begin, end) };
								particles_[i].s.predicted_position += delta_x;
#if GAUSS_SEIDEL_WITHIN_CHUNK
								particles_scratch_[i].predicted_position += delta_x;;
#endif
							}
						}
					}
				});
		}
	}

	void XPBDParticleContext::UpdateVelocityAndInternalForces(float delta_time)
	{
		ZoneScoped;

		auto indices{ std::views::iota(0u, (uint32_t)particles_.size()) };
		std::for_each(std::execution::par_unseq, indices.begin(), indices.end(),
			[&](uint32_t i) {
				XPBDParticle& p{ particles_[i] };

				// Update velocity.
				p.velocity = (p.s.predicted_position - p.position) / delta_time;
				p.key = HashPosition(p.s.predicted_position);
				p.position = p.s.predicted_position;

				//p.debug_color = Heatmap(p.key, 0.0f, HASH_TABLE_SIZE);

				// In the future, internal forces like drag and vorticity will be applied here.
			});
	}

	XPBDParticleContext::ProximityContainer XPBDParticleContext::GetParticlesByProximity(const glm::vec3& position)
	{
		return ProximityContainer(this, position);
	}

	XPBDParticleContext::ConstProximityContainer XPBDParticleContext::GetParticlesByProximity(const glm::vec3& position) const
	{
		return ConstProximityContainer(this, position);
	}

	XPBDParticleContext::IndexProximityContainer XPBDParticleContext::GetParticleIndicesByProximity(const glm::vec3& position)
	{
		return IndexProximityContainer(this, position);
	}

	XPBDParticleContext::ConstIndexProximityContainer XPBDParticleContext::GetParticleIndicesByProximity(const glm::vec3& position) const
	{
		return ConstIndexProximityContainer(this, position);
	}

	std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> XPBDParticleContext::GetParticleRangesWithinKernelSIMD(const glm::vec3& position) const
	{
		std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> result{};
		glm::uvec3 coord{ PositionToCoordinate(position) };

		// Define all possible x, y, and z coordinates.
		uint32_t x_pos0{ coord.x - 1 };
		uint32_t& x_pos1{ coord.x };
		uint32_t x_pos2{ coord.x + 1 };

		uint32_t y_pos0{ coord.y - 1 };
		uint32_t& y_pos1{ coord.y };
		uint32_t y_pos2{ coord.y + 1 };

		uint32_t z_pos0{ coord.z - 1 };
		uint32_t& z_pos1{ coord.z };
		uint32_t z_pos2{ coord.z + 1 };

		// Set what all the indices of a triple for loop would be for i, j, k. There are 27 total since 3^3 = 27.
		__m256i x0 = _mm256_setr_epi32(x_pos0, x_pos0, x_pos0, x_pos0, x_pos0, x_pos0, x_pos0, x_pos0);
		__m256i x1 = _mm256_setr_epi32(x_pos0, x_pos1, x_pos1, x_pos1, x_pos1, x_pos1, x_pos1, x_pos1);
		__m256i x2 = _mm256_setr_epi32(x_pos1, x_pos1, x_pos2, x_pos2, x_pos2, x_pos2, x_pos2, x_pos2);
		__m256i x3 = _mm256_setr_epi32(x_pos2, x_pos2, x_pos2, 0, 0, 0, 0, 0); // Only first 3 used.

		__m256i y0 = _mm256_setr_epi32(y_pos0, y_pos0, y_pos0, y_pos1, y_pos1, y_pos1, y_pos2, y_pos2);
		__m256i y1 = _mm256_setr_epi32(y_pos2, y_pos0, y_pos0, y_pos0, y_pos1, y_pos1, y_pos1, y_pos2);
		__m256i y2 = _mm256_setr_epi32(y_pos2, y_pos2, y_pos0, y_pos0, y_pos0, y_pos1, y_pos1, y_pos1);
		__m256i y3 = _mm256_setr_epi32(y_pos2, y_pos2, y_pos2, 0, 0, 0, 0, 0); // Only first 3 used.

		__m256i z0 = _mm256_setr_epi32(z_pos0, z_pos1, z_pos2, z_pos0, z_pos1, z_pos2, z_pos0, z_pos1);
		__m256i z1 = _mm256_setr_epi32(z_pos2, z_pos0, z_pos1, z_pos2, z_pos0, z_pos1, z_pos2, z_pos0);
		__m256i z2 = _mm256_setr_epi32(z_pos1, z_pos2, z_pos0, z_pos1, z_pos2, z_pos0, z_pos1, z_pos2);
		__m256i z3 = _mm256_setr_epi32(z_pos0, z_pos1, z_pos2, 0, 0, 0, 0, 0); // Only first 3 used.

		// Compute the hash for all pairs in the triple for loop.
		__m256i hash0{ HashCoordsSIMD(x0, y0, z0) };
		__m256i hash1{ HashCoordsSIMD(x1, y1, z1) };
		__m256i hash2{ HashCoordsSIMD(x2, y2, z2) };
		__m256i hash3{ HashCoordsSIMD(x3, y3, z3) };

		// Look up the result from the hash table.
		result[0] = hash_table_[_mm256_extract_epi32(hash0, 0)];
		result[1] = hash_table_[_mm256_extract_epi32(hash0, 1)];
		result[2] = hash_table_[_mm256_extract_epi32(hash0, 2)];
		result[3] = hash_table_[_mm256_extract_epi32(hash0, 3)];
		result[4] = hash_table_[_mm256_extract_epi32(hash0, 4)];
		result[5] = hash_table_[_mm256_extract_epi32(hash0, 5)];
		result[6] = hash_table_[_mm256_extract_epi32(hash0, 6)];
		result[7] = hash_table_[_mm256_extract_epi32(hash0, 7)];

		result[8] = hash_table_[_mm256_extract_epi32(hash1, 0)];
		result[9] = hash_table_[_mm256_extract_epi32(hash1, 1)];
		result[10] = hash_table_[_mm256_extract_epi32(hash1, 2)];
		result[11] = hash_table_[_mm256_extract_epi32(hash1, 3)];
		result[12] = hash_table_[_mm256_extract_epi32(hash1, 4)];
		result[13] = hash_table_[_mm256_extract_epi32(hash1, 5)];
		result[14] = hash_table_[_mm256_extract_epi32(hash1, 6)];
		result[15] = hash_table_[_mm256_extract_epi32(hash1, 7)];

		result[16] = hash_table_[_mm256_extract_epi32(hash2, 0)];
		result[17] = hash_table_[_mm256_extract_epi32(hash2, 1)];
		result[18] = hash_table_[_mm256_extract_epi32(hash2, 2)];
		result[19] = hash_table_[_mm256_extract_epi32(hash2, 3)];
		result[20] = hash_table_[_mm256_extract_epi32(hash2, 4)];
		result[21] = hash_table_[_mm256_extract_epi32(hash2, 5)];
		result[22] = hash_table_[_mm256_extract_epi32(hash2, 6)];
		result[23] = hash_table_[_mm256_extract_epi32(hash2, 7)];

		result[24] = hash_table_[_mm256_extract_epi32(hash3, 0)];
		result[25] = hash_table_[_mm256_extract_epi32(hash3, 1)];
		result[26] = hash_table_[_mm256_extract_epi32(hash3, 2)];

		return result;
	}

	std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> XPBDParticleContext::GetParticleRangesWithinKernel(const glm::vec3& position, uint32_t* out_block_count) const
	{
		std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> result{};
		glm::uvec3 coord{ PositionToCoordinate(position) };

		uint32_t result_idx{ 0 };
		for (uint32_t i{ coord.x - 1 }; i <= coord.x + 1; ++i)
		{
			for (uint32_t j{ coord.y - 1 }; j <= coord.y + 1; ++j)
			{
				for (uint32_t k{ coord.z - 1 }; k <= coord.z + 1; ++k)
				{
					glm::uvec3 neighbor_coord{ i, j, k };
					uint32_t particle_idx{ hash_table_[HashCoords(neighbor_coord)] };
					if (particle_idx != NULL_INDEX) {
						result[result_idx++] = particle_idx;
					}
				}
			}
		}

		*out_block_count = result_idx;
		return result;
	}

	const std::vector<std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL>>& XPBDParticleContext::GetCachedParticleRanges() const
	{
		return particle_ranges_;
	}

	void XPBDParticleContext::UpdateIndexBuffers()
	{
		ZoneScoped;
		{
			ZoneScopedN("Sort particles");
			std::sort(std::execution::par_unseq, particles_.begin(), particles_.end(),
				[](const XPBDParticle& p0, const XPBDParticle& p1) { return p0.key < p1.key; });
		}

		std::memset(hash_table_.data(), NULL_INDEX, HASH_TABLE_SIZE * sizeof(uint32_t));

		{
			ZoneScopedN("Update hash table");
			uint32_t current_key{ NULL_INDEX };
			for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i)
			{
				XPBDParticle& p{ particles_[i] };
				if ((p.key != current_key) && (p.key != NULL_INDEX))
				{
					hash_table_[p.key] = i;
					current_key = p.key;
				}
			}
		}

		{
			ZoneScopedN("Copy to stripped particles");
			for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i)
			{
				std::memcpy(&particles_stripped_[i], &particles_[i].s, sizeof(XPBDParticleStripped));
#if GAUSS_SEIDEL_WITHIN_CHUNK
				std::memcpy(&particles_scratch_[i], &particles_[i].s, sizeof(XPBDParticleStripped));
#endif
				particle_keys_[i] = particles_[i].key;
			}
		}
	}

	void XPBDParticleContext::CopyPositions()
	{
		ZoneScoped;
		for (uint32_t i{ 0 }; i < (uint32_t)particles_.size(); ++i)
		{
			particles_stripped_[i].predicted_position = particles_[i].s.predicted_position;
#if GAUSS_SEIDEL_WITHIN_CHUNK
			particles_scratch_[i].predicted_position = particles_[i].s.predicted_position;
#endif
		}
	}

	const PhysicsMaterial* XPBDParticleContext::GetPhysicsMaterial(const XPBDParticle& p) const
	{
		return (*physics_materials_)[p.physics_material_index];
	}

	PhysicsMaterial* XPBDParticleContext::GetPhysicsMaterial(const XPBDParticle& p)
	{
		return (*physics_materials_)[p.physics_material_index];
	}

	glm::vec3 FluidCollisionConstraint::Solve(
		XPBDParticleContext* p_context,
		const XPBDRigidBodyContext* rb_context,
		uint32_t particle_idx,
		float delta_time,
		uint32_t chunk_begin,
		uint32_t chunk_end) const
	{
		const std::vector<uint32_t>& particle_keys{ p_context->GetParticleKeys() };
		const XPBDParticleStripped* particles_stripped{ p_context->GetParticlesStripped() };
#if GAUSS_SEIDEL_WITHIN_CHUNK
		const XPBDParticleStripped* particles_scratch{ p_context->GetParticlesScratch() };
#endif

		const float compliance_term{ compliance_ / (delta_time * delta_time) };

#if GAUSS_SEIDEL_WITHIN_CHUNK
		const XPBDParticleStripped& p1{ particles_scratch[particle_idx] };
#else
		const XPBDParticleStripped& p1{ particles_stripped[particle_idx] };
#endif
		glm::vec3 particle_delta_x{};

		const auto& start_of_ranges{ p_context->GetCachedParticleRanges()[particle_idx] };
		for (uint32_t range_start : start_of_ranges)
		{
			if (range_start == NULL_INDEX) {
				continue;
			}

			uint64_t current_key{ particle_keys[range_start] };
			for (uint32_t p2_idx{ range_start }; p2_idx < (uint32_t)particle_keys.size() && particle_keys[p2_idx] == current_key; ++p2_idx)
			{
				if (p2_idx == particle_idx) {
					continue;
				}

#if GAUSS_SEIDEL_WITHIN_CHUNK
				bool p2_in_chunk{ (p2_idx >= chunk_begin && p2_idx < chunk_end) };
				const XPBDParticleStripped& p2{ p2_in_chunk ? particles_scratch[p2_idx] : particles_stripped[p2_idx] }; // Guass-seidel if in chunk, Jacobi if not.
#else
				const XPBDParticleStripped& p2{ particles_stripped[p2_idx] };
#endif
				glm::vec3 diff{ p1.predicted_position - p2.predicted_position };
				float distance2{ glm::length2(diff) };

				if (distance2 == 0.0f)
				{
					diff = glm::vec3{ 0.0f, 0.0001f, 0.0f };
					distance2 = glm::length2(diff);
				}

				constexpr float attractive_width{ PARTICLE_WIDTH * 1.8f };
				constexpr float attractive_width_squared{ attractive_width * attractive_width };

				constexpr float repulsive_width{ PARTICLE_WIDTH * 1.4f };
				constexpr float repulsive_width_squared{ repulsive_width * repulsive_width };

				if (distance2 >= attractive_width_squared) {
					continue;
				}

				float distance{ std::sqrtf(distance2) };
				float c{};
				float compliance_term2{};

				if (distance2 >= repulsive_width_squared)
				{
					c = -(distance - attractive_width);
					compliance_term2 = 0.01f / (delta_time * delta_time);
				}
				else if (distance2 >= PARTICLE_WIDTH_SQUARED)
				{
					c = distance - repulsive_width;
					compliance_term2 = 0.01f / (delta_time * delta_time);
				}
				else
				{
					c = distance - PARTICLE_WIDTH;
					compliance_term2 = compliance_term;
				}

				glm::vec3 delta_c1{ diff / distance };

				float lambda{ -c / (p1.inverse_mass + p2.inverse_mass + compliance_term2) }; // Magnitude of gradients are 1.0, so they're not written here.
				particle_delta_x += lambda * p1.inverse_mass * delta_c1;
			}
		}

		// TODO: Don't iterate over all rigid bodies.
		// Detect rigid body collisions.
		RigidBodyParticleCollisionInfo& rb_collision{ p_context->GetRigidBodyCollision(particle_idx) };
		rb_collision.rb_index = NULL_INDEX;
		uint32_t rb_idx{ 0 };
		for (const RigidBody* rb : rb_context->GetRigidBodies())
		{
			std::optional<glm::vec3> rb_voxel_pos{ rb_context->ComputeParticleCollision(rb, p1.predicted_position) };

			if (rb_voxel_pos.has_value())
			{
				glm::vec3 diff{ p1.predicted_position - rb_voxel_pos.value() };
				float distance{ glm::length(diff) };
				float c{ distance - PARTICLE_WIDTH };
				glm::vec3 grad_c{ diff / distance };


				// Rigid body update that will be applied during rigid body physics update.
				glm::vec3& n{ grad_c };
				glm::vec3 world_pos_b = rb_voxel_pos.value() - n * PARTICLE_RADIUS;
				glm::vec3 r{ rb_voxel_pos.value() - rb->node->position };
				float rb_inv_mass{ rb->immovable ? 0.0f : (1.0f / rb->mass) };
				glm::vec3 r_cross_n{ glm::cross(r, n) };
				glm::mat3 inertia_tensor_inv_b{ rb->immovable || rb->voxel_chunk.IsPointMass() ? glm::mat3{} : glm::inverse(rb->inertia_tensor) };
				float rb_weight{ rb_inv_mass + glm::dot(r_cross_n, inertia_tensor_inv_b * r_cross_n) };
				float lambda{ -c / (p1.inverse_mass + rb_weight + compliance_term) };
				glm::vec3 p{ lambda * n };

				// Record particle's change in position.
				particle_delta_x += p * p1.inverse_mass;

				// Record rigid body's change in position and rotation.
				// For now just overwrite previous rigid body collisions this particle had. So particle will currently only influence one rigid body per time step.
				if (!rb->immovable)
				{
					rb_collision.rb_index = rb_idx;
					rb_collision.rb_delta_position = -p * rb_inv_mass;
					if (rb->voxel_chunk.IsPointMass()) {
						rb_collision.rb_delta_rotation = {};
					}
					else
					{
						glm::vec3 tmp2{ inertia_tensor_inv_b * glm::cross(r, p) };
						rb_collision.rb_delta_rotation = -0.5f * glm::quat{ 0.0f, tmp2.x, tmp2.y, tmp2.z } *rb->node->rotation;
					}
				}
			}
			++rb_idx;
		}

		return particle_delta_x;
	}

	std::vector<std::pair<float*, std::string>> FluidCollisionConstraint::GetParameters()
	{
		return {
			{&compliance_, "Compliance"},
		};
	}

	void FluidCollisionConstraint::OnParametersMutated()
	{
		// No-op.
	}
}
