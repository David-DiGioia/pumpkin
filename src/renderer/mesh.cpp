#include "mesh.h"

#include <type_traits>
#include <vector>

#include "tiny_gltf.h"
#include "logger.h"
#include "memory_allocator.h"
#include "vulkan_util.h"

namespace renderer
{
	std::vector<VkVertexInputAttributeDescription> Vertex::GetVertexAttributes(VertexAttributes attributes)
	{
		switch (attributes)
		{
		case VertexAttributes::POSITION_NORMAL_COORD:
			return {
				VERTEX_ATTRIBUTE(0, position),
				VERTEX_ATTRIBUTE(1, normal),
				VERTEX_ATTRIBUTE(2, tex_coord),
			};
		case VertexAttributes::POSITION_NORMAL:
			return {
				VERTEX_ATTRIBUTE(0, position),
				VERTEX_ATTRIBUTE(1, normal),
			};
		case VertexAttributes::POSITION:
			return {
				VERTEX_ATTRIBUTE(0, position),
			};
		case VertexAttributes::NONE:
			return {};
		default:
			logger::Error("Unrecognized vertex attributes.");
			return {};
		}
	}

#ifdef EDITOR_ENABLED
	std::vector<VkVertexInputAttributeDescription> MPMDebugParticleInstance::GetVertexAttributes()
	{
		return {
			// Vertex attributes.
			VERTEX_ATTRIBUTE(0, position),

			// Instance attributes.
			MPM_PARTICLE_ATTRIBUTE(1, mass),
			MPM_PARTICLE_ATTRIBUTE(2, mu),
			MPM_PARTICLE_ATTRIBUTE(3, lambda),
			MPM_PARTICLE_ATTRIBUTE(4, position),
			MPM_PARTICLE_ATTRIBUTE(5, velocity),
			MPM_PARTICLE_ATTRIBUTE(6, gradient),
			MPM_PARTICLE_ATTRIBUTE(7, elastic_col_0),
			MPM_PARTICLE_ATTRIBUTE(8, elastic_col_1),
			MPM_PARTICLE_ATTRIBUTE(9, elastic_col_2),
			MPM_PARTICLE_ATTRIBUTE(10, plastic_col_0),
			MPM_PARTICLE_ATTRIBUTE(11, plastic_col_1),
			MPM_PARTICLE_ATTRIBUTE(12, plastic_col_2),
		};
	}

	std::vector<VkVertexInputAttributeDescription> MPMDebugNodeInstance::GetVertexAttributes()
	{
		return {
			// Vertex attributes.
			VERTEX_ATTRIBUTE(0, position),

			// Instance attributes.
			MPM_NODE_ATTRIBUTE(1, mass),
			MPM_NODE_ATTRIBUTE(2, position),
			MPM_NODE_ATTRIBUTE(3, velocity),
			MPM_NODE_ATTRIBUTE(4, momentum),
			MPM_NODE_ATTRIBUTE(5, force),
		};
	}

	std::vector<VkVertexInputAttributeDescription> RigidBodyDebugVoxelInstance::GetVertexAttributes()
	{
		return {
			// Vertex attributes.
			VERTEX_ATTRIBUTE(0, position),

			// Instance attributes.
			RIGID_BODY_VOXEL_ATTRIBUTE(1, position),
			RIGID_BODY_VOXEL_ATTRIBUTE(2, normal),
		};
	}
#endif

	uint64_t HashVertex(const Vertex& v, uint32_t i)
	{
		uint64_t pos_sum{ (uint64_t)((i + 13) * (v.position.x + v.position.y + v.position.z)) };
		uint64_t norm_sum{ (uint64_t)((3 * i + 23) * (v.normal.x + v.normal.y + v.normal.z)) };
		return pos_sum ^ norm_sum;
	}

	uint64_t LoadVerticesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, Mesh* out_mesh)
	{
		uint64_t hash{ 4589709 };
		uint32_t geo_idx{ 0 };

		for (tinygltf::Primitive& primitive : tinygltf_mesh.primitives)
		{
			tinygltf::Accessor& pos_accesor{ model.accessors[primitive.attributes["POSITION"]] };
			tinygltf::Accessor& norm_accesor{ model.accessors[primitive.attributes["NORMAL"]] };
			auto tex_coord_itr{ primitive.attributes.find("TEXCOORD_0") };

			out_mesh->geometries[geo_idx].vertices.resize(pos_accesor.count);
			for (uint32_t i = 0; i < out_mesh->geometries[geo_idx].vertices.size(); i++)
			{
				// Positions.
				if (pos_accesor.type == TINYGLTF_TYPE_VEC3)
				{
					if (pos_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
					{
						int buffer_view_idx{ pos_accesor.bufferView };
						auto& buffer_view{ model.bufferViews[buffer_view_idx] };
						int buffer_idx{ buffer_view.buffer };
						auto& buffer{ model.buffers[buffer_idx] };

						float* data = (float*)(buffer.data.data() + buffer_view.byteOffset);

						out_mesh->geometries[geo_idx].vertices[i].position[0] = *(data + (i * 3) + 0);
						out_mesh->geometries[geo_idx].vertices[i].position[1] = *(data + (i * 3) + 1);
						out_mesh->geometries[geo_idx].vertices[i].position[2] = *(data + (i * 3) + 2);
					}
					else
					{
						logger::Error("glTF position component type mismatch.\n");
					}
				}
				else
				{
					logger::Error("glTF position accessor type mismatch.\n");
				}

				// Normals.
				if (norm_accesor.type == TINYGLTF_TYPE_VEC3)
				{
					if (norm_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
					{
						int buffer_view_idx{ norm_accesor.bufferView };
						auto& buffer_view{ model.bufferViews[buffer_view_idx] };
						int buffer_idx{ buffer_view.buffer };
						auto& buffer{ model.buffers[buffer_idx] };

						float* data = (float*)(buffer.data.data() + buffer_view.byteOffset);

						out_mesh->geometries[geo_idx].vertices[i].normal[0] = *(data + (i * 3) + 0);
						out_mesh->geometries[geo_idx].vertices[i].normal[1] = *(data + (i * 3) + 1);
						out_mesh->geometries[geo_idx].vertices[i].normal[2] = *(data + (i * 3) + 2);
					}
					else
					{
						logger::Error("glTF normal component type mismatch.\n");
					}
				}
				else
				{
					logger::Error("glTF normal accessor type mismatch.\n");
				}

				// TEX COORD.
				if (tex_coord_itr != primitive.attributes.end())
				{
					tinygltf::Accessor& coord_accesor = model.accessors[tex_coord_itr->second];

					if (coord_accesor.type == TINYGLTF_TYPE_VEC2)
					{
						if (coord_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
						{
							int buffer_view_idx{ coord_accesor.bufferView };
							auto& buffer_view{ model.bufferViews[buffer_view_idx] };
							int buffer_idx{ buffer_view.buffer };
							auto& buffer{ model.buffers[buffer_idx] };

							float* data = (float*)(buffer.data.data() + buffer_view.byteOffset);

							out_mesh->geometries[geo_idx].vertices[i].tex_coord[0] = *(data + (i * 2) + 0);
							out_mesh->geometries[geo_idx].vertices[i].tex_coord[1] = *(data + (i * 2) + 1);
						}
						else
						{
							logger::Error("glTF tex coord component type mismatch.\n");
						}
					}
					else
					{
						logger::Error("glTF tex coord accessor type mismatch.\n");
					}
				}

				hash ^= HashVertex(out_mesh->geometries[geo_idx].vertices[i], i);
			}

			++geo_idx;
		}

		return hash;
	}

	template<typename T>
	void LoadPrimitiveIndicesGLTF(tinygltf::Model& model, tinygltf::Primitive& primitive, uint32_t geo_idx, uint64_t* out_hash, Mesh* out_mesh)
	{
		int accessor_idx = primitive.indices;

		int buffer_view_idx = model.accessors[accessor_idx].bufferView;
		auto& buffer_view = model.bufferViews[buffer_view_idx];
		int buffer_idx = buffer_view.buffer;
		auto& buffer = model.buffers[buffer_idx];

		uint32_t idx_count{ (uint32_t)model.accessors[accessor_idx].count };
		out_mesh->geometries[geo_idx].indices.reserve(idx_count);

		T* data = (T*)(buffer.data.data() + buffer_view.byteOffset);

		for (uint32_t i = 0; i < idx_count; i++)
		{
			T idx{ *(data + i) };
			out_mesh->geometries[geo_idx].indices.push_back(idx);
			*out_hash ^= idx * (i + 47);
		}
	}

	uint64_t LoadIndicesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, Mesh* out_mesh)
	{
		uint64_t hash{ 8501276 };
		uint32_t geo_idx{ 0 };

		for (tinygltf::Primitive& primitive : tinygltf_mesh.primitives)
		{
			int accessor_idx = primitive.indices;
			int component_type = model.accessors[accessor_idx].componentType;

			if (component_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
				LoadPrimitiveIndicesGLTF<uint16_t>(model, primitive, geo_idx, &hash, out_mesh);
			}
			else if (component_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
				LoadPrimitiveIndicesGLTF<uint32_t>(model, primitive, geo_idx, &hash, out_mesh);
			}
			else {
				logger::Error("glTF index component type mismatch.");
			}

			++geo_idx;
		}

		return hash;
	}

	void CalculateTangents(Mesh* out_mesh)
	{
		for (Geometry& geometry : out_mesh->geometries)
		{
			for (uint32_t i{ 0 }; i < geometry.indices.size() - 2; i += 3)
			{
				Vertex& v1{ geometry.vertices[geometry.indices[(uint64_t)i + 0]] };
				Vertex& v2{ geometry.vertices[geometry.indices[(uint64_t)i + 1]] };
				Vertex& v3{ geometry.vertices[geometry.indices[(uint64_t)i + 2]] };

				glm::vec3 edge1{ glm::vec3{v2.position} - glm::vec3{v1.position} };
				glm::vec3 edge2{ glm::vec3{v3.position} - glm::vec3{v1.position} };
				glm::vec2 delta_uv1{ v2.tex_coord - v1.tex_coord };
				glm::vec2 delta_uv2{ v3.tex_coord - v1.tex_coord };

				float f{ 1.0f / (delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y) };

				glm::vec3 tangent{
					f * (delta_uv2.y * edge1.x - delta_uv1.y * edge2.x),
					f * (delta_uv2.y * edge1.y - delta_uv1.y * edge2.y),
					f * (delta_uv2.y * edge1.z - delta_uv1.y * edge2.z),
				};

				tangent = glm::normalize(tangent);

				v1.tangent = glm::vec4{ tangent, 0.0f };
				v2.tangent = glm::vec4{ tangent, 0.0f };
				v3.tangent = glm::vec4{ tangent, 0.0f };
			}
		}
	}

	std::string NameMesh(const std::vector<Geometry>& geometries)
	{
		uint32_t triangle_count{ 0 };
		for (const Geometry& geometry : geometries) {
			triangle_count += (uint32_t)(geometry.indices.size() / 3);
		}

		std::string name{ "Mesh_" };
		name += std::to_string(geometries.size()) + "_Geometries_" + std::to_string(triangle_count) + "_Triangles";
		return name;
	}
}
