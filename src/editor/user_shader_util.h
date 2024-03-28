#pragma once

#include <filesystem>
#include <cstddef>
#include <vector>
#include <string>
#include "nlohmann/json.hpp"

namespace jsonkey
{
	const std::string MEMBER_NAME{ "name" };
	const std::string MEMBER_VALUE{ "value" };
	const std::string MEMBER_TYPE{ "type" };
}

enum class MemberType
{
	BOOL,
	INT,
	UINT,
	FLOAT,
	DOUBLE,
	VEC2,
	VEC3,
	VEC4,
	MAT2,
	MAT3,
	MAT4,
};

// Describes a member variable of a uniform buffer.
struct MemberVariable
{
	std::string name;     // Name of variable.
	MemberType type;      // GLSL type, such as float, int, etc.
	uint32_t size;        // Byte size of this variable.
	uint32_t offset;      // Byte offset into uniform buffer.
	uint32_t array_count; // Number of array elements.
};

// Readable result of parsing a uniform buffer, allowing its values to be modified.
class UniformBuffer
{
public:
	void Initialize(const std::vector<MemberVariable>& members);

	std::vector<std::byte>& GetBuffer();

	const std::vector<std::byte>& GetBuffer() const;

	// Draw the ImGui elements for each of the UBO members, that allow each member to be modified.
	// Return true if any of the gui values are changed this frame.
	bool DrawGui(float alignment);

	nlohmann::json ToJson() const;

private:
	std::vector<MemberVariable> members_{};   // Type info about each of the members of the UBO struct.
	std::vector<std::byte> uniform_buffer_{}; // A byte buffer the same size as the shader's uniform buffer.
};

class ShaderParser
{
public:
	void Parse(const std::filesystem::path& shader_path);

	const UniformBuffer& GetUniformBuffer() const;

private:
	void ParseUniformBufferBody(const std::string& ubo_body);

	enum class ParserState
	{
		FIND_UBO,
		FIND_OPEN_BRACE,
		CONCATENATE_BETWEEN_BRACES,
		PARSE_MEMBERS,
	} state_{};

	UniformBuffer uniform_buffer_;
};

// Returns true if compilation succeeds, otherwise returns false.
bool CompileShader(const std::filesystem::path& shader_path, std::filesystem::path* out_spirv_path);
