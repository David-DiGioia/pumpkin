#pragma once

#include <vector>
#include "pumpkin.h"
#include "imgui.h"

enum class ScaleType
{
	MAJOR,
	MINOR,
};

std::vector<pmk::Note> GetNotesFromInput(pmk::Note scale, ScaleType scale_type);
