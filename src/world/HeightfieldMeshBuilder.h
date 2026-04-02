#pragma once

#include "HeightfieldGrid.h"
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>

struct HeightfieldMeshVertex {
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 normal{0.0f, 1.0f, 0.0f};
  glm::vec2 uv{0.0f, 0.0f};
};

struct HeightfieldMeshData {
  std::vector<HeightfieldMeshVertex> vertices;
  std::vector<uint32_t> indices;
};

class HeightfieldMeshBuilder {
public:
  static HeightfieldMeshData build(const HeightfieldGrid &grid) {
    if (!grid.isValid()) {
      throw std::runtime_error("cannot build a mesh from an invalid heightfield");
    }
    if (!grid.hasCells()) {
      throw std::runtime_error("cannot build a mesh from a heightfield with no cells");
    }

    HeightfieldMeshData mesh;
    mesh.vertices.reserve(grid.pointCount());
    mesh.indices.reserve(static_cast<size_t>(grid.cellsX) *
                         static_cast<size_t>(grid.cellsZ) * 6);

    for (uint32_t pointZ = 0; pointZ < grid.pointsZ(); ++pointZ) {
      for (uint32_t pointX = 0; pointX < grid.pointsX(); ++pointX) {
        mesh.vertices.push_back(HeightfieldMeshVertex{
            .position = grid.pointPosition(pointX, pointZ),
            .normal = pointNormal(grid, pointX, pointZ),
            .uv = {
                grid.cellsX == 0
                    ? 0.0f
                    : static_cast<float>(pointX) / static_cast<float>(grid.cellsX),
                grid.cellsZ == 0
                    ? 0.0f
                    : static_cast<float>(pointZ) / static_cast<float>(grid.cellsZ),
            },
        });
      }
    }

    for (uint32_t cellZ = 0; cellZ < grid.cellsZ; ++cellZ) {
      for (uint32_t cellX = 0; cellX < grid.cellsX; ++cellX) {
        const uint32_t topLeft = vertexIndex(grid, cellX, cellZ);
        const uint32_t topRight = vertexIndex(grid, cellX + 1, cellZ);
        const uint32_t bottomLeft = vertexIndex(grid, cellX, cellZ + 1);
        const uint32_t bottomRight = vertexIndex(grid, cellX + 1, cellZ + 1);

        mesh.indices.push_back(topLeft);
        mesh.indices.push_back(bottomLeft);
        mesh.indices.push_back(bottomRight);

        mesh.indices.push_back(bottomRight);
        mesh.indices.push_back(topRight);
        mesh.indices.push_back(topLeft);
      }
    }

    return mesh;
  }

private:
  static uint32_t vertexIndex(const HeightfieldGrid &grid, uint32_t pointX,
                              uint32_t pointZ) {
    return static_cast<uint32_t>(grid.pointIndex(pointX, pointZ));
  }

  static glm::vec3 pointNormal(const HeightfieldGrid &grid, uint32_t pointX,
                               uint32_t pointZ) {
    const uint32_t leftX = pointX == 0 ? pointX : pointX - 1;
    const uint32_t rightX = std::min(pointX + 1, grid.cellsX);
    const uint32_t topZ = pointZ == 0 ? pointZ : pointZ - 1;
    const uint32_t bottomZ = std::min(pointZ + 1, grid.cellsZ);

    const float leftHeight = grid.heightAt(leftX, pointZ);
    const float rightHeight = grid.heightAt(rightX, pointZ);
    const float topHeight = grid.heightAt(pointX, topZ);
    const float bottomHeight = grid.heightAt(pointX, bottomZ);

    const float stepX =
        static_cast<float>(rightX - leftX) * std::max(grid.cellSize, 1e-6f);
    const float stepZ =
        static_cast<float>(bottomZ - topZ) * std::max(grid.cellSize, 1e-6f);
    const float dhdx = stepX > 0.0f ? (rightHeight - leftHeight) / stepX : 0.0f;
    const float dhdz = stepZ > 0.0f ? (bottomHeight - topHeight) / stepZ : 0.0f;

    return glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
  }
};
