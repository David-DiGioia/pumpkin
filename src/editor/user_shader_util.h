#pragma once

#include <filesystem>
#include <cstddef>
#include <vector>
#include <string>

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

	const std::vector<std::byte>& GetBuffer() const;

	// Draw the ImGui elements for each of the UBO members, that allow each member to be modified.
	void DrawGui(uint32_t alignment);

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

// Returns path to spirv file.
std::filesystem::path CompileShader(const std::filesystem::path& shader_path);
