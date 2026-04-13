#pragma once

#include "TerrainEditing.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

struct TerrainLocalRaycastHit {
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f, 1.0f, 0.0f};
  float distance = 0.0f;
};

class TerrainQueries {
public:
  static glm::vec3 sampleLocalNormal(const TerrainConfig &config, float x,
                                     float z) {
    const float epsilon =
        std::max(std::min(config.sizeX, config.sizeZ) / 512.0f, 0.01f);
    const float heightLeft =
        TerrainGenerator::sampleHeight(config, x - epsilon, z);
    const float heightRight =
        TerrainGenerator::sampleHeight(config, x + epsilon, z);
    const float heightBack =
        TerrainGenerator::sampleHeight(config, x, z - epsilon);
    const float heightFront =
        TerrainGenerator::sampleHeight(config, x, z + epsilon);
    return glm::normalize(glm::vec3(heightLeft - heightRight, epsilon * 2.0f,
                                    heightBack - heightFront));
  }

  static float maxLocalBoundsY(const TerrainConfig &config) {
    return std::max(std::max(std::abs(config.heightScale),
                             TerrainEditing::maxHeightOffsetMagnitude(config)),
                    0.5f) +
           1.0f;
  }

  static std::optional<glm::vec2>
  intersectRayAabb(const glm::vec3 &origin, const glm::vec3 &direction,
                   const glm::vec3 &boundsMin, const glm::vec3 &boundsMax) {
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();

    for (int axis = 0; axis < 3; ++axis) {
      if (std::abs(direction[axis]) <= 1e-6f) {
        if (origin[axis] < boundsMin[axis] || origin[axis] > boundsMax[axis]) {
          return std::nullopt;
        }
        continue;
      }

      const float invDirection = 1.0f / direction[axis];
      float t0 = (boundsMin[axis] - origin[axis]) * invDirection;
      float t1 = (boundsMax[axis] - origin[axis]) * invDirection;
      if (t0 > t1) {
        std::swap(t0, t1);
      }

      tMin = std::max(tMin, t0);
      tMax = std::min(tMax, t1);
      if (tMin > tMax) {
        return std::nullopt;
      }
    }

    return glm::vec2(tMin, tMax);
  }

  static std::optional<TerrainLocalRaycastHit>
  raycastLocalSurface(const TerrainConfig &config, const glm::vec3 &origin,
                      const glm::vec3 &direction) {
    const float halfSizeX = config.sizeX * 0.5f;
    const float halfSizeZ = config.sizeZ * 0.5f;
    const auto hitRange = intersectRayAabb(
        origin, direction, {-halfSizeX, -maxLocalBoundsY(config), -halfSizeZ},
        {halfSizeX, maxLocalBoundsY(config), halfSizeZ});
    if (!hitRange.has_value()) {
      return std::nullopt;
    }

    const float startT = std::max(hitRange->x, 0.0f);
    const float endT = hitRange->y;
    const uint32_t stepCount =
        std::max(config.xSegments + config.zSegments, 64u);
    float previousT = startT;
    glm::vec3 previousPoint = origin + direction * previousT;
    float previousDelta =
        previousPoint.y -
        TerrainGenerator::sampleHeight(config, previousPoint.x, previousPoint.z);

    for (uint32_t step = 1; step <= stepCount; ++step) {
      const float alpha =
          static_cast<float>(step) / static_cast<float>(stepCount);
      const float currentT = glm::mix(startT, endT, alpha);
      const glm::vec3 currentPoint = origin + direction * currentT;
      const float currentDelta =
          currentPoint.y -
          TerrainGenerator::sampleHeight(config, currentPoint.x, currentPoint.z);
      const bool crossedSurface =
          (previousDelta >= 0.0f && currentDelta <= 0.0f) ||
          std::abs(currentDelta) <= 1e-4f;
      if (!crossedSurface) {
        previousT = currentT;
        previousPoint = currentPoint;
        previousDelta = currentDelta;
        continue;
      }

      float lowT = previousT;
      float highT = currentT;
      for (int iteration = 0; iteration < 10; ++iteration) {
        const float midT = 0.5f * (lowT + highT);
        const glm::vec3 midPoint = origin + direction * midT;
        const float midDelta =
            midPoint.y -
            TerrainGenerator::sampleHeight(config, midPoint.x, midPoint.z);
        if (midDelta > 0.0f) {
          lowT = midT;
        } else {
          highT = midT;
        }
      }

      const float hitT = 0.5f * (lowT + highT);
      const glm::vec3 localHit = origin + direction * hitT;
      const glm::vec3 localPoint(
          localHit.x, TerrainGenerator::sampleHeight(config, localHit.x, localHit.z),
          localHit.z);
      return TerrainLocalRaycastHit{
          .position = localPoint,
          .normal = sampleLocalNormal(config, localPoint.x, localPoint.z),
          .distance = hitT,
      };
    }

    return std::nullopt;
  }
};
