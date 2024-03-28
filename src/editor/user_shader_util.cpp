#include "user_shader_util.h"

#include <Windows.h>
#include <fstream>
#include <sstream>
#include <vector>
#include "glm/glm.hpp"
#include "imgui.h"

#include "string_util.h"
#include "logger.h"

void ShaderParser::Parse(const std::filesystem::path& shader_path)
{
	std::ifstream file{ shader_path };
	std::string line{};
	state_ = ParserState::FIND_UBO;
	bool move_to_next_line{ true };

	// Concatenation of all the characters between the open and closing braces of the UBO.
	std::string ubo_body{};

	// We do the getline() outside the while condition so we can reprocess a line if needed using move_to_next_line.
	while (true)
	{
		if (move_to_next_line && !std::getline(file, line)) {
			break;
		}

		switch (state_)
		{
		case ParserState::FIND_UBO:
		{
			move_to_next_line = true;
			std::string line_copy{ line };
			std::erase(line_copy, ' ');
			if (pmkutil::Contains(line_copy, "binding=1"))
			{
				state_ = ParserState::FIND_OPEN_BRACE;
				move_to_next_line = false;
			}
			break;
		}
		case ParserState::FIND_OPEN_BRACE:
		{
			move_to_next_line = true;
			size_t open_brace_idx{ line.find_first_of('{') };
			if (open_brace_idx != std::string::npos)
			{
				// Found open brace, so either concatenate rest of line, or until closing brace if there is one.
				size_t close_brace_idx{ line.find_first_of('}', open_brace_idx) };

				if (close_brace_idx != std::string::npos)
				{
					// If open and close brace are on the same line we have the whole body string.
					size_t start_index{ open_brace_idx + 1 };
					ubo_body = line.substr(start_index, close_brace_idx - start_index);
					state_ = ParserState::PARSE_MEMBERS;
					move_to_next_line = false;
				}
				else
				{
					// Otherwise add the rest of this line to the body string then switch state to concatenate the rest.
					ubo_body += line.substr(open_brace_idx + 1);
					state_ = ParserState::CONCATENATE_BETWEEN_BRACES;
				}
			}
			break;
		}
		case ParserState::CONCATENATE_BETWEEN_BRACES:
		{
			move_to_next_line = true;
			size_t close_brace_idx{ line.find_first_of('}') };
			if (close_brace_idx != std::string::npos)
			{
				// If we find closing brace, we have copied the whole body so change states.
				ubo_body += line.substr(0, close_brace_idx);
				state_ = ParserState::PARSE_MEMBERS;
				move_to_next_line = false;
			}
			else
			{
				// Otherwise just copy the whole line and move on.
				ubo_body += line;
			}
			break;
		}
		case ParserState::PARSE_MEMBERS:
		{
			ParseUniformBufferBody(ubo_body);
			file.close();
			return;
		}
		}
	}
	file.close();
}

const UniformBuffer& ShaderParser::GetUniformBuffer() const
{
	return uniform_buffer_;
}

void ShaderParser::ParseUniformBufferBody(const std::string& ubo_body)
{
	std::vector<std::string> statements{ pmkutil::Split(ubo_body, ';') };
	std::vector<MemberVariable> member_variables{};
	uint32_t offset{ 0 };

	for (const std::string& statement : statements)
	{
		std::stringstream ss{ statement };
		std::string type{};
		std::string name{};

		ss >> type;
		ss >> name;

		MemberVariable member_variable{};
		member_variable.name = name;
		member_variable.offset = offset;
		member_variable.array_count = 1;

		if (type == "bool")
		{
			member_variable.type = MemberType::BOOL;
			member_variable.size = sizeof(bool);
			offset += sizeof(bool);
		}
		else if (type == "int")
		{
			member_variable.type = MemberType::INT;
			member_variable.size = sizeof(int);
			offset += sizeof(int);
		}
		else if (type == "uint")
		{
			member_variable.type = MemberType::UINT;
			member_variable.size = sizeof(uint32_t);
			offset += sizeof(uint32_t);
		}
		else if (type == "float")
		{
			member_variable.type = MemberType::FLOAT;
			member_variable.size = sizeof(float);
			offset += sizeof(float);
		}
		else if (type == "double")
		{
			member_variable.type = MemberType::DOUBLE;
			member_variable.size = sizeof(double);
			offset += sizeof(double);
		}
		else if (type == "vec2")
		{
			member_variable.type = MemberType::VEC2;
			member_variable.size = sizeof(glm::vec2);
			offset += sizeof(glm::vec2);
		}
		else if (type == "vec3")
		{
			member_variable.type = MemberType::VEC3;
			member_variable.size = sizeof(glm::vec3);
			offset += sizeof(glm::vec3);
		}
		else if (type == "vec4")
		{
			member_variable.type = MemberType::VEC4;
			member_variable.size = sizeof(glm::vec4);
			offset += sizeof(glm::vec4);
		}
		else if (type == "mat2")
		{
			member_variable.type = MemberType::MAT2;
			member_variable.size = sizeof(glm::mat2);
			offset += sizeof(glm::mat2);
		}
		else if (type == "mat3")
		{
			member_variable.type = MemberType::MAT3;
			member_variable.size = sizeof(glm::mat3);
			offset += sizeof(glm::mat3);
		}
		else if (type == "mat4")
		{
			member_variable.type = MemberType::MAT4;
			member_variable.size = sizeof(glm::mat4);
			offset += sizeof(glm::mat4);
		}

		member_variables.push_back(member_variable);
	}

	uniform_buffer_.Initialize(member_variables);
}

bool CompileShader(const std::filesystem::path& shader_path, std::filesystem::path* out_spirv_path)
{
	const std::filesystem::path vulkan_sdk_path{ getenv("VULKAN_SDK") };
	const std::filesystem::path glsl_validator{ vulkan_sdk_path / "Bin/glslangValidator.exe" };
	const std::filesystem::path spirv_path{ shader_path.parent_path() / (shader_path.filename().string() + ".spv") };
	*out_spirv_path = spirv_path;

	const std::string command_line{ glsl_validator.string() + " -V " + shader_path.string() + " -o " + spirv_path.string() + " --target-env spirv1.6" };
	bool compilation_succeeded{ true };

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
			CREATE_NO_WINDOW,
			NULL,
			NULL,
			&si,
			&pi);

		// Wait for the process to finish.
		WaitForSingleObject(pi.hProcess, INFINITE);

		DWORD exit_code{ 0 };
		if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
			logger::Error("Couldn't get exit code\n");
		}
		else if (exit_code != 0) {
			compilation_succeeded = false;
		}

		// Close process and thread handles. 
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}

	return compilation_succeeded;
}

void UniformBuffer::Initialize(const std::vector<MemberVariable>& members)
{
	uint32_t buffer_size{ 0 };
	for (const MemberVariable& member : members) {
		buffer_size += member.size;
	}

	members_ = members;
	uniform_buffer_.resize(buffer_size, (std::byte)0u);
}

std::vector<std::byte>& UniformBuffer::GetBuffer()
{
	return uniform_buffer_;
}

const std::vector<std::byte>& UniformBuffer::GetBuffer() const
{
	return uniform_buffer_;
}

bool UniformBuffer::DrawGui(float alignment)
{
	bool value_changed{ false };
	ImGui::PushID(members_.data());

	for (const MemberVariable& member : members_)
	{
		void* ptr{ &uniform_buffer_[member.offset] };

		ImGui::Text(member.name.c_str());
		ImGui::SameLine(alignment);

		switch (member.type)
		{
		case MemberType::BOOL:
			value_changed |= ImGui::Checkbox("##bool", (bool*)ptr);
			break;
		case MemberType::INT:
			value_changed |= ImGui::DragInt("##int", (int*)ptr);
			break;
		case MemberType::UINT:
			value_changed |= ImGui::DragInt("##uint", (int*)ptr);
			break;
		case MemberType::FLOAT:
			value_changed |= ImGui::DragFloat("##float", (float*)ptr);
			break;
		case MemberType::DOUBLE:
			value_changed |= ImGui::DragFloat("##double", (float*)ptr);
			break;
		case MemberType::VEC2:
			value_changed |= ImGui::DragFloat2("##vec2", (float*)ptr);
			break;
		case MemberType::VEC3:
			value_changed |= ImGui::DragFloat3("##vec3", (float*)ptr);
			break;
		case MemberType::VEC4:
			value_changed |= ImGui::DragFloat4("##vec4", (float*)ptr);
			break;
		case MemberType::MAT2:
			break;
		case MemberType::MAT3:
			break;
		case MemberType::MAT4:
			break;
		}
	}

	ImGui::PopID();
	return value_changed;
}

nlohmann::json UniformBuffer::ToJson() const
{
	nlohmann::json j{};

	for (const MemberVariable& member : members_)
	{
		const void* ptr{ &uniform_buffer_[member.offset] };

		nlohmann::json member_json{};

		member_json[jsonkey::MEMBER_NAME] = member.name;

		switch (member.type)
		{
		case MemberType::BOOL:
		{
			bool b{};
			std::memcpy(&b, ptr, sizeof(bool));
			member_json[jsonkey::MEMBER_VALUE] = b;
			member_json[jsonkey::MEMBER_TYPE] = "bool";
			break;
		}
		case MemberType::INT:
		{
			int i{};
			std::memcpy(&i, ptr, sizeof(int));
			member_json[jsonkey::MEMBER_VALUE] = i;
			member_json[jsonkey::MEMBER_TYPE] = "int";
			break;
		}
		case MemberType::UINT:
		{
			uint32_t u{};
			std::memcpy(&u, ptr, sizeof(uint32_t));
			member_json[jsonkey::MEMBER_VALUE] = u;
			member_json[jsonkey::MEMBER_TYPE] = "uint";
			break;
		}
		case MemberType::FLOAT:
		{
			float f{};
			std::memcpy(&f, ptr, sizeof(float));
			member_json[jsonkey::MEMBER_VALUE] = f;
			member_json[jsonkey::MEMBER_TYPE] = "float";
			break;
		}
		case MemberType::DOUBLE:
		{
			double d{};
			std::memcpy(&d, ptr, sizeof(double));
			member_json[jsonkey::MEMBER_VALUE] = d;
			member_json[jsonkey::MEMBER_TYPE] = "double";
			break;
		}
		case MemberType::VEC2:
		{
			glm::vec2 v{};
			std::memcpy(&v, ptr, sizeof(glm::vec2));
			member_json[jsonkey::MEMBER_VALUE] = { v.x, v.y };
			member_json[jsonkey::MEMBER_TYPE] = "vec2";
			break;
		}
		case MemberType::VEC3:
		{
			glm::vec3 v{};
			std::memcpy(&v, ptr, sizeof(glm::vec3));
			member_json[jsonkey::MEMBER_VALUE] = { v.x, v.y, v.z };
			member_json[jsonkey::MEMBER_TYPE] = "vec3";
			break;
		}
		case MemberType::VEC4:
			break;
		case MemberType::MAT2:
			break;
		case MemberType::MAT3:
			break;
		case MemberType::MAT4:
			break;
		}

		j += member_json;
	}

	return j;
}
