#pragma once

#include "HeightfieldGrid.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

#include <glm/glm.hpp>

struct HeightfieldNoiseSettings {
  uint32_t seed = 1337;
  float baseHeight = 0.0f;
  float amplitude = 4.0f;
  float frequency = 0.08f;
  uint32_t octaves = 4;
  float lacunarity = 2.0f;
  float persistence = 0.5f;
  glm::vec2 offset{0.0f, 0.0f};
};

class HeightfieldNoise {
public:
  static float sample(glm::vec2 worldXZ,
                      const HeightfieldNoiseSettings &settings) {
    validateSettings(settings);

    float total = 0.0f;
    float amplitude = 1.0f;
    float frequency = settings.frequency;
    float amplitudeSum = 0.0f;

    for (uint32_t octave = 0; octave < settings.octaves; ++octave) {
      total += gradientNoise(worldXZ * frequency + settings.offset,
                             settings.seed + octave * 1013u) *
               amplitude;
      amplitudeSum += amplitude;
      amplitude *= settings.persistence;
      frequency *= settings.lacunarity;
    }

    if (amplitudeSum <= 1e-6f) {
      return settings.baseHeight;
    }

    const float normalized = total / amplitudeSum;
    return settings.baseHeight + normalized * settings.amplitude;
  }

  static void apply(HeightfieldGrid &grid,
                    const HeightfieldNoiseSettings &settings) {
    if (!grid.isValid()) {
      throw std::runtime_error("cannot apply noise to an invalid heightfield");
    }
    validateSettings(settings);

    for (uint32_t pointZ = 0; pointZ < grid.pointsZ(); ++pointZ) {
      for (uint32_t pointX = 0; pointX < grid.pointsX(); ++pointX) {
        const glm::vec3 pointPosition = grid.pointPosition(pointX, pointZ);
        const float height = sample({pointPosition.x, pointPosition.z}, settings);
        grid.setHeightAt(pointX, pointZ, height);
      }
    }
  }

private:
  static void validateSettings(const HeightfieldNoiseSettings &settings) {
    if (settings.frequency <= 0.0f) {
      throw std::runtime_error("heightfield noise frequency must be positive");
    }
    if (settings.octaves == 0) {
      throw std::runtime_error("heightfield noise must use at least one octave");
    }
    if (settings.lacunarity <= 0.0f) {
      throw std::runtime_error("heightfield noise lacunarity must be positive");
    }
    if (settings.persistence < 0.0f) {
      throw std::runtime_error(
          "heightfield noise persistence must be non-negative");
    }
  }

  static uint32_t mixBits(uint32_t value) {
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
  }

  static uint32_t latticeHash(int x, int z, uint32_t seed) {
    const uint32_t ux = static_cast<uint32_t>(x) * 0x8da6b343u;
    const uint32_t uz = static_cast<uint32_t>(z) * 0xd8163841u;
    return mixBits(seed ^ ux ^ uz);
  }

  static glm::vec2 gradient(int x, int z, uint32_t seed) {
    constexpr glm::vec2 gradients[] = {
        {1.0f, 0.0f},   {-1.0f, 0.0f},  {0.0f, 1.0f},   {0.0f, -1.0f},
        {0.7071068f, 0.7071068f},       {-0.7071068f, 0.7071068f},
        {0.7071068f, -0.7071068f},      {-0.7071068f, -0.7071068f},
    };
    return gradients[latticeHash(x, z, seed) % 8u];
  }

  static float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

  static float gradientNoise(glm::vec2 point, uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(point.x));
    const int z0 = static_cast<int>(std::floor(point.y));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const glm::vec2 local(point.x - static_cast<float>(x0),
                          point.y - static_cast<float>(z0));

    const float n00 = glm::dot(gradient(x0, z0, seed), local);
    const float n10 = glm::dot(gradient(x1, z0, seed),
                               local - glm::vec2(1.0f, 0.0f));
    const float n01 = glm::dot(gradient(x0, z1, seed),
                               local - glm::vec2(0.0f, 1.0f));
    const float n11 = glm::dot(gradient(x1, z1, seed),
                               local - glm::vec2(1.0f, 1.0f));

    const float u = fade(local.x);
    const float v = fade(local.y);
    const float nx0 = glm::mix(n00, n10, u);
    const float nx1 = glm::mix(n01, n11, u);
    return glm::clamp(glm::mix(nx0, nx1, v) * 1.41421356f, -1.0f, 1.0f);
  }
};
