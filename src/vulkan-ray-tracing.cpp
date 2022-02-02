#include "vulkan-ray-tracing.h"
#include "project-config.h"

#include <iostream>

int main(int argc, char** argv)
{
	if (argc < 2) {
		// report version
		std::cout << "Vulkan Ray Tracing Version " << VRT_VERSION_MAJOR << "." << VRT_VERSION_MINOR << '\n';
		return 1;
	}

	return 0;
}
