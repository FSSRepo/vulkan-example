#ifndef MESH_DATA_H
#define MESH_DATA_H

#include <vector>
#include <cstdint>

struct MeshData {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
};

MeshData createBoxMesh(float w, float h, float d);

#endif
