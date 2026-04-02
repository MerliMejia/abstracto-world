#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>

struct HeightfieldSample {
  bool insideBounds = false;
  float height = 0.0f;
  glm::vec3 normal{0.0f, 1.0f, 0.0f};
  glm::vec3 position{0.0f, 0.0f, 0.0f};
};

struct HeightfieldGrid {
  uint32_t cellsX = 0;
  uint32_t cellsZ = 0;
  float cellSize = 1.0f;
  glm::vec2 originXZ{0.0f, 0.0f};
  std::vector<float> heights;

  static HeightfieldGrid flat(uint32_t gridCellsX, uint32_t gridCellsZ,
                              float gridCellSize,
                              glm::vec2 gridOriginXZ = {0.0f, 0.0f},
                              float baseHeight = 0.0f) {
    HeightfieldGrid grid;
    grid.cellsX = gridCellsX;
    grid.cellsZ = gridCellsZ;
    grid.cellSize = gridCellSize;
    grid.originXZ = gridOriginXZ;
    grid.heights.assign(grid.pointCount(), baseHeight);
    return grid;
  }

  uint32_t pointsX() const { return cellsX + 1; }
  uint32_t pointsZ() const { return cellsZ + 1; }
  size_t pointCount() const {
    return static_cast<size_t>(pointsX()) * static_cast<size_t>(pointsZ());
  }

  bool hasCells() const { return cellsX > 0 && cellsZ > 0; }

  bool isValid() const { return cellSize > 0.0f && heights.size() == pointCount(); }

  size_t pointIndex(uint32_t pointX, uint32_t pointZ) const {
    if (pointX >= pointsX() || pointZ >= pointsZ()) {
      throw std::runtime_error("heightfield point index is out of bounds");
    }
    return static_cast<size_t>(pointZ) * static_cast<size_t>(pointsX()) +
           static_cast<size_t>(pointX);
  }

  float heightAt(uint32_t pointX, uint32_t pointZ) const {
    if (!isValid()) {
      throw std::runtime_error("heightfield grid is invalid");
    }
    return heights[pointIndex(pointX, pointZ)];
  }

  void setHeightAt(uint32_t pointX, uint32_t pointZ, float heightValue) {
    if (!isValid()) {
      throw std::runtime_error("heightfield grid is invalid");
    }
    heights[pointIndex(pointX, pointZ)] = heightValue;
  }

  glm::vec3 pointPosition(uint32_t pointX, uint32_t pointZ) const {
    return {originXZ.x + static_cast<float>(pointX) * cellSize,
            heightAt(pointX, pointZ),
            originXZ.y + static_cast<float>(pointZ) * cellSize};
  }

  float width() const { return static_cast<float>(cellsX) * cellSize; }
  float depth() const { return static_cast<float>(cellsZ) * cellSize; }

  bool containsXZ(glm::vec2 worldXZ) const {
    if (!isValid() || !hasCells()) {
      return false;
    }

    const float maxX = originXZ.x + width();
    const float maxZ = originXZ.y + depth();
    return worldXZ.x >= originXZ.x && worldXZ.x <= maxX &&
           worldXZ.y >= originXZ.y && worldXZ.y <= maxZ;
  }

  HeightfieldSample sample(glm::vec2 worldXZ) const {
    HeightfieldSample result;
    if (!containsXZ(worldXZ)) {
      result.position = {worldXZ.x, 0.0f, worldXZ.y};
      return result;
    }

    const float localX = (worldXZ.x - originXZ.x) / cellSize;
    const float localZ = (worldXZ.y - originXZ.y) / cellSize;
    const uint32_t cellX = std::min(static_cast<uint32_t>(std::floor(localX)),
                                    cellsX - 1);
    const uint32_t cellZ = std::min(static_cast<uint32_t>(std::floor(localZ)),
                                    cellsZ - 1);
    const float tx = glm::clamp(localX - static_cast<float>(cellX), 0.0f, 1.0f);
    const float tz = glm::clamp(localZ - static_cast<float>(cellZ), 0.0f, 1.0f);

    const float h00 = heightAt(cellX, cellZ);
    const float h10 = heightAt(cellX + 1, cellZ);
    const float h01 = heightAt(cellX, cellZ + 1);
    const float h11 = heightAt(cellX + 1, cellZ + 1);

    const float height0 = glm::mix(h00, h10, tx);
    const float height1 = glm::mix(h01, h11, tx);
    const float sampledHeight = glm::mix(height0, height1, tz);

    const float dhdx =
        glm::mix(h10 - h00, h11 - h01, tz) / std::max(cellSize, 1e-6f);
    const float dhdz =
        glm::mix(h01 - h00, h11 - h10, tx) / std::max(cellSize, 1e-6f);

    result.insideBounds = true;
    result.height = sampledHeight;
    result.normal = glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz));
    result.position = {worldXZ.x, sampledHeight, worldXZ.y};
    return result;
  }
};
