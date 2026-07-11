#define TINYOBJLOADER_IMPLEMENTATION
#include "obj_loader.h"
#include <tiny_obj_loader.h>

#include <cstring>
#include <unordered_map>

struct VertexKey {
  int vi, ti, ni;
  bool operator==(const VertexKey &o) const {
    return vi == o.vi && ti == o.ti && ni == o.ni;
  }
};

struct VertexKeyHash {
  size_t operator()(const VertexKey &k) const {
    size_t h = std::hash<int>{}(k.vi);
    h ^= std::hash<int>{}(k.ti) << 1;
    h ^= std::hash<int>{}(k.ni) << 2;
    return h;
  }
};

ObjData loadObj(const std::string &path) {
  tinyobj::attrib_t attrib;
  std::vector<tinyobj::shape_t> shapes;
  std::vector<tinyobj::material_t> materials;
  std::string warn, err;

  std::string baseDir = path.substr(0, path.find_last_of("/\\") + 1);

  if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(),
                        baseDir.c_str())) {
    throw std::runtime_error("failed to load obj: " + warn + err);
  }

  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::unordered_map<VertexKey, uint32_t, VertexKeyHash> uniqueVertices;

  for (const auto &shape : shapes) {
    size_t indexOffset = 0;
    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
      size_t fv = shape.mesh.num_face_vertices[f];

      // Get face material
      int material_id = shape.mesh.material_ids[f];

      // Get 3 corner positions/UVs/normals for this triangle
      glm::vec3 pos[3];
      glm::vec2 uv[3];
      glm::vec3 nor[3];

      for (size_t v = 0; v < fv; v++) {
        tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

        pos[v] = {attrib.vertices[3 * idx.vertex_index + 0],
                  attrib.vertices[3 * idx.vertex_index + 1],
                  attrib.vertices[3 * idx.vertex_index + 2]};

        if (idx.texcoord_index >= 0) {
          uv[v] = {attrib.texcoords[2 * idx.texcoord_index + 0],
                   1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]};
        } else {
          uv[v] = {0.0f, 0.0f};
        }

        if (idx.normal_index >= 0) {
          nor[v] = {attrib.normals[3 * idx.normal_index + 0],
                    attrib.normals[3 * idx.normal_index + 1],
                    attrib.normals[3 * idx.normal_index + 2]};
        } else {
          nor[v] = {0.0f, 1.0f, 0.0f};
        }
      }

      // Compute tangent for this triangle
      glm::vec3 edge1 = pos[1] - pos[0];
      glm::vec3 edge2 = pos[2] - pos[0];
      glm::vec2 duv1 = uv[1] - uv[0];
      glm::vec2 duv2 = uv[2] - uv[0];

      float det = duv1.x * duv2.y - duv2.x * duv1.y;
      glm::vec3 tangent;
      if (std::abs(det) < 1e-6f) {
        tangent = glm::normalize(edge1);
      } else {
        float invDet = 1.0f / det;
        tangent = invDet * (duv2.y * edge1 - duv1.y * edge2);
      }
      tangent = glm::normalize(tangent);

      // Add vertices
      for (size_t v = 0; v < fv; v++) {
        tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];
        VertexKey key{idx.vertex_index, idx.texcoord_index, idx.normal_index};

        auto it = uniqueVertices.find(key);
        if (it != uniqueVertices.end()) {
          indices.push_back(it->second);
          // Accumulate tangent for averaging
          vertices[it->second].tangent += tangent;
        } else {
          uint32_t newIndex = static_cast<uint32_t>(vertices.size());
          uniqueVertices[key] = newIndex;
          indices.push_back(newIndex);

          Vertex vert{};
          vert.pos = pos[v];
          vert.color = {1.0f, 1.0f, 1.0f};
          vert.uv = uv[v];
          vert.tangent = tangent;
          vert.normal = nor[v];
          vertices.push_back(vert);
        }
      }

      indexOffset += fv;
    }
  }

  // Normalize accumulated tangents
  for (auto &v : vertices) {
    v.tangent = glm::normalize(v.tangent);
  }

  return {vertices, indices};
}
