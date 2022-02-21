#pragma once

namespace config
{
	enum class OptimizationLevel
	{
		NONE,
		STRONG,     // Disable validation layers.
		AGGRESSIVE, // Suppress log output.
	};

	// Set optimization level here.
	constexpr OptimizationLevel optimization_level{ OptimizationLevel::NONE };

	// Set effects of optimization level here.
	constexpr bool suppress_logger{ optimization_level >= OptimizationLevel::AGGRESSIVE };
	constexpr bool disable_validation{ optimization_level >= OptimizationLevel::STRONG };
}
