#pragma once

namespace config
{
	enum class OptimizationLevel
	{
		NONE,
		AGGRESSIVE, // Disable log output and validation layers.
	};

	constexpr OptimizationLevel optimization_level{ OptimizationLevel::NONE };
}
