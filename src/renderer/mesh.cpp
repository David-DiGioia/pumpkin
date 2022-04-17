#include "mesh.h"

#include <type_traits>
#include <vector>

#include "tiny_gltf.h"
#include "logger.h"
#include "memory_allocator.h"
#include "vulkan_util.h"

namespace renderer
{
	std::vector<VkVertexInputAttributeDescription> Vertex::GetVertexAttributes()
	{
		return {
			VERTEX_ATTRIBUTE(0, position),
		};
	}

	void LoadVerticesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, std::vector<Vertex>* out_vertices)
	{
		auto& primitive{ tinygltf_mesh.primitives[0] };

		tinygltf::Accessor& pos_accesor = model.accessors[primitive.attributes["POSITION"]];
		tinygltf::Accessor& norm_accesor = model.accessors[primitive.attributes["NORMAL"]];
		tinygltf::Accessor& coord_accesor = model.accessors[primitive.attributes["TEXCOORD_0"]];

		out_vertices->resize(pos_accesor.count);
		for (size_t i = 0; i < out_vertices->size(); i++)
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

					(*out_vertices)[i].position[0] = *(data + (i * 3) + 0);
					(*out_vertices)[i].position[1] = *(data + (i * 3) + 1);
					(*out_vertices)[i].position[2] = *(data + (i * 3) + 2);
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

					(*out_vertices)[i].normal[0] = *(data + (i * 3) + 0);
					(*out_vertices)[i].normal[1] = *(data + (i * 3) + 1);
					(*out_vertices)[i].normal[2] = *(data + (i * 3) + 2);
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
			if (coord_accesor.type == TINYGLTF_TYPE_VEC2)
			{
				if (coord_accesor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
				{
					int buffer_view_idx{ coord_accesor.bufferView };
					auto& buffer_view{ model.bufferViews[buffer_view_idx] };
					int buffer_idx{ buffer_view.buffer };
					auto& buffer{ model.buffers[buffer_idx] };

					float* data = (float*)(buffer.data.data() + buffer_view.byteOffset);

					(*out_vertices)[i].tex_coord[0] = *(data + (i * 2) + 0);
					(*out_vertices)[i].tex_coord[1] = *(data + (i * 2) + 1);
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
	}

	void LoadIndicesGLTF(tinygltf::Model& model, tinygltf::Mesh& tinygltf_mesh, std::vector<uint16_t>* out_indices)
	{
		int accessor_idx = tinygltf_mesh.primitives[0].indices;

		int buffer_view_idx = model.accessors[accessor_idx].bufferView;
		auto& buffer_view = model.bufferViews[buffer_view_idx];
		int buffer_idx = buffer_view.buffer;
		auto& buffer = model.buffers[buffer_idx];

		int component_type = model.accessors[accessor_idx].componentType;
		if (component_type != TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
			logger::Error("glTF index component type mismatch.");
		}

		uint32_t idx_count{ (uint32_t)model.accessors[accessor_idx].count };
		out_indices->reserve(idx_count);

		uint16_t* data = (uint16_t*)(buffer.data.data() + buffer_view.byteOffset);

		for (uint32_t i = 0; i < idx_count; i++) {
			out_indices->push_back(*(data + i));
		}
	}
}
