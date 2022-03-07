#pragma once

class GraphicsPipeline
{
public:
	GraphicsPipeline();

	~GraphicsPipeline();

private:
	VkPipeline pipeline_{};
};

