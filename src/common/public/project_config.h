#pragma once

namespace config
{
	enum class OptimizationLevel
	{
		NONE,
		AGGRESSIVE, // Disable log output and validation layers.
	};

	// Set optimization level here.
	constexpr OptimizationLevel optimization_level{ OptimizationLevel::NONE };

	// Set effects of optimization level here.
	constexpr bool suppress_logger{ optimization_level >= OptimizationLevel::AGGRESSIVE };
	constexpr bool disable_validation{ optimization_level >= OptimizationLevel::AGGRESSIVE };
}
