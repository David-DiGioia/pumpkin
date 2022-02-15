#include "project_config.h"

#include "vulkan_renderer.h"
#include "logger.h"

int main()
{
	logger::Print("Pumpkin Engine Version %d.%d\n", PUMPKIN_VERSION_MAJOR, PUMPKIN_VERSION_MINOR);

	VulkanRenderer renderer;
	
	renderer.TestFunction();

	return 0;
}
