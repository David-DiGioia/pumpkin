#pragma once

#include <vector>
#include <type_traits>
#include "glm/glm.hpp"
#include "glm/gtx/quaternion.hpp"

#include "constraint.h"
#include "logger.h"
#include "common_constants.h"

namespace pmk
{
	constexpr uint32_t MAXIMUM_BLOCKS_IN_KERNEL{ 27 }; // Since 3^3 = 27. Assuming kernel radius is less or equal to grid spacing.

	struct XPBDParticle
	{
		glm::vec3 position;           // Meters.
		glm::vec3 predicted_position; // Meters.
		glm::vec3 velocity;           // Meters per second.
		float inverse_mass;           // Reciprocal kilograms.
		uint8_t physics_material_index;

#ifdef EDITOR_ENABLED
		glm::vec3 debug_color; // Color to show arbitrary debug information when debugging.
#endif
	};

	struct XPBDParticleIndex
	{
		uint32_t key;   // Unique value for every half box of grid. Used for sorting indices.
		uint32_t index; // Index into particles_.

		bool operator<(const XPBDParticleIndex& other);
	};

	struct RigidBodyParticleCollisionInfo
	{
		uint32_t rb_index;
		glm::vec3 rb_delta_position;
		glm::quat rb_delta_rotation;
	};

	class XPBDRigidBodyContext;
	struct PhysicsMaterial;

	class XPBDParticleContext
	{
	public:
		/*
		* An iterator to reproduce the following behavior to iterate through nearby particles.
		*
		*	uint32_t particle_range_count{};
		*	auto start_of_ranges{ context->GetParticleRangesWithinKernel(p->position, &particle_range_count) };
		*	for (uint32_t i{ 0 }; i < particle_range_count; ++i)
		*	{
		*		uint32_t range_start{ start_of_ranges[i] };
		*		uint32_t current_key{ particle_indices_[range_start].key };
		*		for (uint32_t j{ range_start }; j < (uint32_t)particle_indices_.size() && particle_indices_[j].key == current_key; ++j)
		*		{
		*			const XPBDParticle& p{ particles_[particle_indices_[j].index] };
		*			// Code using p here.
		*		}
		*	}
		*/
		template<typename DataType, typename DereferenceType>
		class ParticleProximityIterator
		{
		public:
			using difference_type = std::ptrdiff_t;
			using value_type = DataType;

			// Construct begin iterator.
			ParticleProximityIterator(const XPBDParticleContext* context, const glm::vec3& position)
				: context_{ context }
				, particle_range_count_{}
				, start_of_ranges_{ context->GetParticleRangesWithinKernel(position, &particle_range_count_) }
				, i_{}
				, current_key_{}
				, j_{}
				, cached_particle_indices_count_{ (uint32_t)context_->particle_indices_.size() }
			{
				if (i_ < particle_range_count_)
				{
					uint32_t range_start = start_of_ranges_[i_];
					current_key_ = context_->particle_indices_[range_start].key;

					j_ = range_start;
					if (!ParticleInSameBlock()) {
						j_ = NULL_INDEX;
					}
				}
				else {
					j_ = NULL_INDEX;
				}
			}

			// Construct end iterator.
			ParticleProximityIterator()
				: context_{}
				, particle_range_count_{}
				, start_of_ranges_{}
				, i_{}
				, current_key_{}
				, j_{ NULL_INDEX }
				, cached_particle_indices_count_{}
			{
			}

			template <typename T = DereferenceType>
			typename std::enable_if<std::is_same<T, uint32_t>::value, T>::type operator*() const
			{
				return context_->particle_indices_[j_].index;
			}

			template <typename T = DereferenceType>
			typename std::enable_if<!std::is_same<T, uint32_t>::value, T&>::type operator*() const
			{
				return context_->particles_[context_->particle_indices_[j_].index];
			}

			ParticleProximityIterator& operator++()
			{
				++j_;
				if (!ParticleInSameBlock())
				{
					++i_;
					if (i_ >= particle_range_count_) {
						j_ = NULL_INDEX;
					}
					else
					{
						uint32_t range_start = start_of_ranges_[i_];
						current_key_ = context_->particle_indices_[range_start].key;
						j_ = range_start;
					}
				}
				return *this;
			}

			ParticleProximityIterator operator++(int)
			{
				auto tmp = *this;
				++*this;
				return tmp;
			}

			bool operator==(const ParticleProximityIterator& other) const
			{
				return j_ == other.j_;
			}

			inline bool ParticleInSameBlock()
			{
				return j_ < cached_particle_indices_count_ && context_->particle_indices_[j_].key == current_key_;
			}

		protected:
			const XPBDParticleContext* context_{};

			uint32_t particle_range_count_{};
			std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> start_of_ranges_{};
			uint32_t i_{};
			uint32_t current_key_{};
			uint32_t j_{};

			uint32_t cached_particle_indices_count_{};
		};

		template <typename Context, typename DereferenceType>
		class ParticleProximityContainer
		{
		public:
			typedef ParticleProximityIterator<XPBDParticle, DereferenceType> iterator;
			typedef ParticleProximityIterator<const XPBDParticle, DereferenceType> const_iterator;

			ParticleProximityContainer(Context* context, const glm::vec3& position)
				: context_{ context }
				, position_{ position }
			{
			}

			iterator begin() const
			{
				return iterator(context_, position_);
			}

			iterator end() const
			{
				return iterator();
			}

			const_iterator cbegin() const
			{
				return const_iterator(context_, position_);
			}

			const_iterator cend() const
			{
				return const_iterator();
			}

		public:
			Context* context_{};
			glm::vec3 position_{};
		};

		typedef ParticleProximityContainer<XPBDParticleContext, XPBDParticle> ProximityContainer;
		typedef ParticleProximityContainer<const XPBDParticleContext, const XPBDParticle> ConstProximityContainer;
		typedef ParticleProximityContainer<XPBDParticleContext, uint32_t> IndexProximityContainer;
		typedef ParticleProximityContainer<const XPBDParticleContext, uint32_t> ConstIndexProximityContainer;

		void Initialize(
			std::vector<XPBDParticle>&& particles,
			float chunk_width,
			const std::vector<XPBDConstraint*>* jacobi_constraints,
			const std::vector<PhysicsMaterial*>* physics_materials);

		void SimulateStep(float delta_time, const XPBDRigidBodyContext* rb_context);

		const std::vector<XPBDParticle>& GetParticles() const;

		std::vector<XPBDParticle>& GetParticles();

		RigidBodyParticleCollisionInfo& GetRigidBodyCollision(uint32_t particle_idx);

		// Compute density using SPH kernel, taking into account both particles and rigid body voxels.
		float ComputeDensity(const glm::vec3& pos, const ConstIndexProximityContainer& proximity_particles) const;

		ProximityContainer GetParticlesByProximity(const glm::vec3& position);

		ConstProximityContainer GetParticlesByProximity(const glm::vec3& position) const;

		IndexProximityContainer GetParticleIndicesByProximity(const glm::vec3& position);

		ConstIndexProximityContainer GetParticleIndicesByProximity(const glm::vec3& position) const;

	private:
		friend ParticleProximityIterator<XPBDParticle, XPBDParticle>;
		friend ParticleProximityIterator<XPBDParticle, uint32_t>;
		friend ParticleProximityIterator<const XPBDParticle, const XPBDParticle>;
		friend ParticleProximityIterator<const XPBDParticle, uint32_t>;

		void ApplyForces(float delta_time);

		void SolveConstraints(float delta_time, const XPBDRigidBodyContext* rb_context);

		void UpdateVelocityAndInternalForces(float delta_time);

		std::array<uint32_t, MAXIMUM_BLOCKS_IN_KERNEL> GetParticleRangesWithinKernel(const glm::vec3& position, uint32_t* out_block_count) const;

		void UpdateIndexBuffers();

		const PhysicsMaterial* GetPhysicsMaterial(const XPBDParticle& p) const;

		PhysicsMaterial* GetPhysicsMaterial(const XPBDParticle& p);

		std::vector<XPBDParticle> particles_{};
		std::vector<XPBDParticleIndex> particle_indices_{};           // Contains indices into particles_, along with a key encoding the block coordinate.
		std::vector<uint32_t> hash_table_{};                          // Indices into particle_indices_, showing start of contiguous region containing particles with this hash value.
		std::vector<RigidBodyParticleCollisionInfo> rb_collisions_{}; // The ith index cooresponds to particles_[i] collision with a rigid body.

		std::vector<glm::vec3> jacobi_positions_{}; // For Jacobi solver, the temporary positions corresponding to each particle in particles_.
		const std::vector<XPBDConstraint*>* jacobi_constraints_{};
		const std::vector<PhysicsMaterial*>* physics_materials_{};

		float particle_radius_{};
		float particle_initial_volume_{};
	};

	class FluidDensityConstraint : public XPBDConstraint
	{
	public:
		virtual void Preprocess(const XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, float delta_time) override;

		virtual glm::vec3 Solve(XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, uint32_t particle_idx, float delta_time) const override;

		virtual std::vector<std::pair<float*, std::string>> GetParameters() override;

		virtual void OnParametersMutated() override;

	protected:
		friend class PhysicsContext;

		std::vector<float> lambda_cache_{};
		float rest_density_{ 1000.0f }; // kilogram per meter cubed.
	};

	class CollisionConstraint : public XPBDConstraint
	{
	public:
		virtual void Preprocess(const XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, float delta_time) override;

		virtual glm::vec3 Solve(XPBDParticleContext* p_context, const XPBDRigidBodyContext* rb_context, uint32_t particle_idx, float delta_time) const override;

		virtual std::vector<std::pair<float*, std::string>> GetParameters() override;

		virtual void OnParametersMutated() override;

	protected:
		friend class PhysicsContext;

		float compliance_{ 0.0f };
	};
}
