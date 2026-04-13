#pragma once

#include "Terrain.h"

class TerrainEditing {
public:
  static bool applyHeightBrush(TerrainConfig &config,
                               const glm::vec2 &center, float radius,
                               float heightDelta) {
    return TerrainGenerator::applyBrush(config, center, radius, heightDelta);
  }

  static bool applyFlattenBrush(TerrainConfig &config,
                                const glm::vec2 &center, float radius,
                                float targetHeight, float maxHeightDelta) {
    return TerrainGenerator::applyFlattenBrush(config, center, radius,
                                               targetHeight, maxHeightDelta);
  }

  static bool applyColorBrush(TerrainConfig &config, const glm::vec2 &center,
                              float radius, const glm::vec4 &targetColor,
                              float blendAmount) {
    return TerrainGenerator::applyColorBrush(config, center, radius,
                                             targetColor, blendAmount);
  }

  static float maxHeightOffsetMagnitude(const TerrainConfig &config) {
    return TerrainGenerator::maxHeightOffsetMagnitude(config);
  }
};
