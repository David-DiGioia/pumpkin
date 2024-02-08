#pragma once

#include <vector>
#include <array>
#include "glm/glm.hpp"

namespace pmk
{
	constexpr uint32_t MAXIMUM_NODES_IN_RANGE{ 27 };       // Radius for quadratic interpolation is 1.5*h so maximum nodes is 3^3.
	constexpr uint32_t MAXIMUM_SUB_BLOCKS_IN_RANGE{ 216 }; // Quadratic interpolation radius of 1.5 is 3 sub blocks, so a diameter of 6, and 6^3 = 216.

	constexpr float CalculateMu(float youngs_modulus, float poissons_ratio)
	{
		return youngs_modulus / (2.0f * (1.0f + poissons_ratio));
	}

	constexpr float CalculateLambda(float youngs_modulus, float poissons_ratio)
	{
		return (youngs_modulus * poissons_ratio) / ((1.0f + poissons_ratio) * (1.0f - 2.0f * poissons_ratio));
	}

	enum class MPMInterpolationKernel
	{
		LINEAR,
		QUADRATIC,
		CUBIC,
	};

	constexpr MPMInterpolationKernel MPM_INTERPOLATION_KERNEL{ MPMInterpolationKernel::QUADRATIC };

	enum class ConstitutiveModelIndex : uint32_t
	{
		HYPER_ELASTIC,
		FLUID,
		SNOW,

		CONSTITUTIVE_MODEL_COUNT,
	};

	struct MaterialPoint
	{
		float mass;
		float mu;
		float lambda;
		glm::vec3 position;
		glm::vec3 position_before_advection;
		glm::vec3 velocity;
		glm::vec3 gradient;
		glm::mat3 affine_matrix;
		glm::mat3 deformation_gradient_elastic;
		glm::mat3 deformation_gradient_plastic;
		ConstitutiveModelIndex  constitutive_model_index;
	};

	class MPMContext;

	class ConstitutiveModel
	{
	public:
		void Initialize(MPMContext* mpm_context);

		virtual void InitializeParticle(MaterialPoint* p, float initial_volume) const = 0;

		// Gets J * Cauchy stress, since the J would cancel out in later calculations.
		// Or equivalently, Piola-Kirchoff stress times F^T.
		virtual glm::mat3 GetJCauchyStress(MaterialPoint& p) const = 0;

		virtual void UpdateLameParameters(MaterialPoint* p) const;

		virtual void UpdateDeformationGradient(MaterialPoint* p, float d_inverse, float delta_time) const;

		virtual void SolveConstraints(MaterialPoint* p, float delta_time) const;

	protected:
		MPMContext* mpm_context_{};
	};

	class HyperElasticModel : public ConstitutiveModel
	{
	public:
		virtual void InitializeParticle(MaterialPoint* p, float initial_volume) const override;

		virtual glm::mat3 GetJCauchyStress(MaterialPoint& p) const override;

		virtual void UpdateLameParameters(MaterialPoint* p) const override;

		virtual void UpdateDeformationGradient(MaterialPoint* p, float d_inverse, float delta_time) const override;

	private:
		glm::mat3 GetPiolaKirchoffStress(MaterialPoint& p) const;

		static constexpr float YOUNGS_MODULUS{ 50.0f };
		static constexpr float POISSONS_RATIO{ 0.4f };
		static constexpr float MU{ CalculateMu(YOUNGS_MODULUS, POISSONS_RATIO) };
		static constexpr float LAMBDA{ CalculateLambda(YOUNGS_MODULUS, POISSONS_RATIO) };
		static constexpr float DENSITY{ 20.0f }; // kilogram per meter cubed.
	};

	class FluidModel : public ConstitutiveModel
	{
	public:
		virtual void InitializeParticle(MaterialPoint* p, float initial_volume) const override;

		virtual glm::mat3 GetJCauchyStress(MaterialPoint& p) const override;

		virtual void SolveConstraints(MaterialPoint* p, float delta_time) const override;

		float IncompressibleConstraintError(float) const;

		glm::vec3 IncompressibleConstraintErrorGradient(MaterialPoint* p) const;

	private:
		static constexpr float DENSITY{ 50.0f };          // kilogram per meter cubed.
		static constexpr float REST_DENSITY{ 50.0f };     // kilogram per meter cubed.
		static constexpr float DYNAMIC_VISCOSITY{ 0.0f };
		static constexpr float EOS_STIFFNESS{ 10.0f };    // Tait equation of state.
		static constexpr float EOS_POWER{ 20.0f };        // Tait equation of state.
	};

	// Takes 12+ substeps, and SVD quality parameters can be adjusted for performance / stability.
	class SnowModel : public ConstitutiveModel
	{
	public:
		virtual void InitializeParticle(MaterialPoint* p, float initial_volume) const override;

		virtual glm::mat3 GetJCauchyStress(MaterialPoint& p) const override;

		virtual void UpdateLameParameters(MaterialPoint* p) const override;

		virtual void UpdateDeformationGradient(MaterialPoint* p, float d_inverse, float delta_time) const override;

	private:
		static constexpr float YOUNGS_MODULUS{ 140000.0f };
		static constexpr float POISSONS_RATIO{ 0.2f };
		static constexpr float MU{ CalculateMu(YOUNGS_MODULUS, POISSONS_RATIO) };
		static constexpr float LAMBDA{ CalculateLambda(YOUNGS_MODULUS, POISSONS_RATIO) };
		static constexpr float DENSITY{ 400.0f }; // kilogram per meter cubed.
		static constexpr float SIGMA_C{ 0.025f };
		static constexpr float SIGMA_S{ 0.0075f };
		static constexpr float HARDENING_PARAMETER{ 10.0f };
	};

	// Per-particle cache.
	struct ParticleCache
	{
		glm::mat3 stress;
		glm::vec3 p2g_lhs;
		glm::mat3 p2g_rhs;
	};

	struct MaterialPointIndex
	{
		uint32_t key;   // Unique value for every half box of grid. Used for sorting indices.
		uint32_t index; // Index into particles_.

		bool operator<(const MaterialPointIndex& other);
	};

	struct GridNode
	{
		float mass;
		glm::vec3 position;
		glm::vec3 velocity;
		glm::vec3 momentum;
		glm::vec3 force;
		glm::uvec3 coordinate;
	};

	class MPMContext
	{
	public:
		void Initialize(std::vector<MaterialPoint>&& particles, float chunk_width);

		void SimulateStep(float delta_time);

		const std::vector<MaterialPoint>& GetParticles() const;

		const std::vector<GridNode>& GetNodes() const;

		float GetDensity(const glm::vec3& position) const;

	private:
		void ParticleToGrid();

		void ComputeGridVelocities();

		void ComputeExplicitGridForces();

		void UpdateGridVelocity(float delta_time);

		void UpdateParticleDeformationGradient(float delta_time);

		void GridToParticle();

		void AdvectParticles(float delta_time);

		void SolveConstraints(float delta_time);

		void UpdateIndexBuffers();

		float QuadraticKernel(float x) const;

		// Kernel for interpolation function.
		float CubicKernel(float x) const;

		float CubicKernelDerivative(float x) const;

		float QuadraticKernelDerivative(float x) const;

		float GetWeight(const glm::vec3& node_pos, const glm::vec3& particle_pos) const;

		glm::vec3 GetWeightGradient(const glm::vec3& node_pos, const glm::vec3& particle_pos) const;

		// Optimized version of GetDInverseGeneralized() for quadratic or cubic kernels.
		float GetDInverse() const;

		// General form of getting D inverse.
		glm::mat3 GetDInverseGeneralized(const glm::vec3& particle_pos) const;

		void UpdateLameParameters();

		glm::uvec3 GridNodeIndexToCoordinate(uint32_t index) const;

		uint32_t GridNodeCoordinateToIndex(const glm::uvec3& coord) const;

		// Returns vector of indices into nodes_.
		std::array<uint32_t, MAXIMUM_NODES_IN_RANGE> GetNodeIndicesWithinRadius(glm::vec3 position, uint32_t* out_count) const;

		// Returns array of indices into particle_indices_ of first element of contiguous block of particles in same sub block.
		std::array<uint32_t, MAXIMUM_SUB_BLOCKS_IN_RANGE> GetParticleRangesWithinRadius(const glm::uvec3& grid_coord, uint32_t* out_count) const;

		ConstitutiveModel* GetConstitutiveModel(const MaterialPoint& p);

		void PrintParticleWeights() const;

		friend FluidModel;

		std::vector<MaterialPoint> particles_{};
		std::vector<ParticleCache> particle_cache_{};
		std::vector<MaterialPointIndex> particle_indices_{}; // Contains indices into particles_, along with a key encoding the sub block coordinate.
		std::vector<uint32_t> sub_block_indices_{};          // Indices into particle_indices_, showing start of contiguous region containing particles in this sub block.
		std::vector<GridNode> nodes_{};
		std::array<ConstitutiveModel*, (uint32_t)ConstitutiveModelIndex::CONSTITUTIVE_MODEL_COUNT> constitutive_models_{
			new HyperElasticModel{},
			new FluidModel{},
			new SnowModel{}
		};

		float particle_radius_{};
		float particle_initial_volume_{};
	};
}
