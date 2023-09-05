#pragma once

#include <filesystem>
#include <cstddef>
#include <vector>

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
	uint32_t offset;      // Byte offset into uniform buffer.
	uint32_t array_count; // Number of array elements.
};

// Readable result of parsing a uniform buffer, allowing its values to be modified.
class UniformBuffer
{
public:
	UniformBuffer(const std::vector<MemberVariable>& members);

	const std::vector<std::byte>& GetBuffer() const;

	// Draw the ImGui elements for each of the UBO members, that allow each member to be modified.
	void DrawGui();

private:
	std::vector<std::byte> uniform_buffer_{}; // A byte buffer the same size as the shader's uniform buffer.
	std::vector<MemberVariable> members_{};   // Type info about each of the members of the UBO struct.
};

class ShaderParser
{
public:
	void Parse(const std::filesystem::path& shader_path);

	const UniformBuffer& GetUniformBuffer() const;

private:
	UniformBuffer uniform_buffer_;
};