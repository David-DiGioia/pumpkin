#include "user_shader_util.h"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <vector>
#include "string_util.h"

void ShaderParser::Parse(const std::filesystem::path& shader_path)
{
	std::ifstream file{ shader_path };
	std::string line{};
	state_ = ParserState::BEGIN;

	while (std::getline(file, line))
	{
		std::stringstream ss{ line };
		std::string token;

		std::erase(line, ' ');
		if (pmkutil::Contains(line, "binding=1"))
		{
			state_ = ParserState::UBO_FOUND;
		}

		while (std::getline(ss, token, ' '))
		{

		}
	}


	file.close();
}

std::filesystem::path CompileShader(const std::filesystem::path& shader_path)
{
	const std::filesystem::path vulkan_sdk_path{ getenv("VULKAN_SDK") };
	const std::filesystem::path glsl_validator{ vulkan_sdk_path / "Bin/glslangValidator.exe" };
	const std::filesystem::path spirv_path{ shader_path.parent_path() / (shader_path.filename().string() + ".spv") };

	const std::string command_line{ glsl_validator.string() + " -V " + shader_path.string() + " -o " + spirv_path.string() + " --target-env spirv1.6" };

	// Attempt to compile shader.
	{
		STARTUPINFOA si{ .cb = sizeof(si) };
		PROCESS_INFORMATION pi{};

		CreateProcessA(
			NULL,                        // Path to executable is null since it's first argument of command line.
			(LPSTR)command_line.c_str(), // Command line.
			NULL,
			NULL,
			FALSE,
			CREATE_NEW_CONSOLE,
			NULL,
			NULL,
			&si,
			&pi);

		// Wait for the process to finish.
		WaitForSingleObject(pi.hProcess, INFINITE);

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	return spirv_path;
}
