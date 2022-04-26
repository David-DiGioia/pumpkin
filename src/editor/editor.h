#pragma once

#include <functional>

#include "pumpkin.h"

class Editor
{
public:
	void Initialize();

	std::function<void(void)> GetRenderCallback();

private:

};
