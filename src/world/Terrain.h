#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

struct TerrainConfig {
  float sizeX = 20.0f;
  float sizeZ = 20.0f;
  uint32_t xSegments = 1;
  uint32_t zSegments = 1;
  glm::vec2 uvScale = {1.0f, 1.0f};
  float heightScale = 0.0f;
  float noiseFrequency = 0.15f;
  uint32_t noiseOctaves = 4;
  float noisePersistence = 0.5f;
  float noiseLacunarity = 2.0f;
  uint32_t noiseSeed = 0;
  std::vector<float> heightOffsets;
  std::vector<glm::vec4> vertexColors;
};

struct TerrainVertex {
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f, 1.0f, 0.0f};
  glm::vec2 uv{0.0f};
  glm::vec4 color{1.0f};
};

struct TerrainMeshData {
  std::vector<TerrainVertex> vertices;
  std::vector<uint32_t> indices;
};

class TerrainGenerator {
public:
  static size_t vertexCount(const TerrainConfig &config) {
    return static_cast<size_t>(std::max(config.xSegments, 1u) + 1u) *
           static_cast<size_t>(std::max(config.zSegments, 1u) + 1u);
  }

  static void ensureHeightOffsets(TerrainConfig &config) {
    const size_t requiredVertexCount = vertexCount(config);
    if (config.heightOffsets.size() == requiredVertexCount) {
      return;
    }
    config.heightOffsets.assign(requiredVertexCount, 0.0f);
  }

  static void ensureVertexColors(TerrainConfig &config) {
    const size_t requiredVertexCount = vertexCount(config);
    if (config.vertexColors.size() == requiredVertexCount) {
      return;
    }
    config.vertexColors.assign(requiredVertexCount, glm::vec4(1.0f));
  }

  static void resampleSurfaceLayers(TerrainConfig &config, uint32_t xSegments,
                                    uint32_t zSegments) {
    const uint32_t resolvedXSegments = std::max(xSegments, 1u);
    const uint32_t resolvedZSegments = std::max(zSegments, 1u);
    if (config.xSegments == resolvedXSegments &&
        config.zSegments == resolvedZSegments &&
        config.heightOffsets.size() == vertexCount(config) &&
        config.vertexColors.size() == vertexCount(config)) {
      return;
    }

    const TerrainConfig sourceConfig = config;
    const uint32_t xVertexCount = resolvedXSegments + 1;
    const uint32_t zVertexCount = resolvedZSegments + 1;
    std::vector<float> resampledOffsets;
    std::vector<glm::vec4> resampledColors;
    resampledOffsets.reserve(static_cast<size_t>(xVertexCount) * zVertexCount);
    resampledColors.reserve(static_cast<size_t>(xVertexCount) * zVertexCount);

    for (uint32_t zIndex = 0; zIndex < zVertexCount; ++zIndex) {
      const float zAlpha =
          static_cast<float>(zIndex) / static_cast<float>(resolvedZSegments);
      const float z = (zAlpha - 0.5f) * sourceConfig.sizeZ;
      for (uint32_t xIndex = 0; xIndex < xVertexCount; ++xIndex) {
        const float xAlpha =
            static_cast<float>(xIndex) / static_cast<float>(resolvedXSegments);
        const float x = (xAlpha - 0.5f) * sourceConfig.sizeX;
        resampledOffsets.push_back(sampleHeightOffset(sourceConfig, x, z));
        resampledColors.push_back(sampleVertexColor(sourceConfig, x, z));
      }
    }

    config.xSegments = resolvedXSegments;
    config.zSegments = resolvedZSegments;
    config.heightOffsets = std::move(resampledOffsets);
    config.vertexColors = std::move(resampledColors);
  }

  static TerrainMeshData buildMesh(const TerrainConfig &config) {
    TerrainMeshData mesh;

    const uint32_t xSegments = std::max(config.xSegments, 1u);
    const uint32_t zSegments = std::max(config.zSegments, 1u);
    const uint32_t xVertexCount = xSegments + 1;
    const uint32_t zVertexCount = zSegments + 1;

    mesh.vertices.reserve(static_cast<size_t>(xVertexCount) * zVertexCount);
    mesh.indices.reserve(static_cast<size_t>(xSegments) * zSegments * 6);

    for (uint32_t zIndex = 0; zIndex < zVertexCount; ++zIndex) {
      const float zAlpha =
          static_cast<float>(zIndex) / static_cast<float>(zSegments);
      const float z = (zAlpha - 0.5f) * config.sizeZ;

      for (uint32_t xIndex = 0; xIndex < xVertexCount; ++xIndex) {
        const float xAlpha =
            static_cast<float>(xIndex) / static_cast<float>(xSegments);
        const float x = (xAlpha - 0.5f) * config.sizeX;

        mesh.vertices.push_back(TerrainVertex{
            .position = {x, sampleHeight(config, x, z), z},
            .uv = {xAlpha * config.uvScale.x, zAlpha * config.uvScale.y},
            .color = sampleVertexColor(config, x, z),
        });
      }
    }

    for (uint32_t zIndex = 0; zIndex < zSegments; ++zIndex) {
      for (uint32_t xIndex = 0; xIndex < xSegments; ++xIndex) {
        const uint32_t topLeft = zIndex * xVertexCount + xIndex;
        const uint32_t topRight = topLeft + 1;
        const uint32_t bottomLeft = (zIndex + 1) * xVertexCount + xIndex;
        const uint32_t bottomRight = bottomLeft + 1;

        mesh.indices.push_back(topLeft);
        mesh.indices.push_back(bottomLeft);
        mesh.indices.push_back(topRight);
        mesh.indices.push_back(topRight);
        mesh.indices.push_back(bottomLeft);
        mesh.indices.push_back(bottomRight);
      }
    }

    computeNormals(mesh);
    return mesh;
  }

  static float sampleHeight(const TerrainConfig &config, float x, float z) {
    return sampleBaseHeight(config, x, z) + sampleHeightOffset(config, x, z);
  }

  static bool applyBrush(TerrainConfig &config, const glm::vec2 &center,
                         float radius, float heightDelta) {
    if (radius <= 1e-6f || std::abs(heightDelta) <= 1e-6f) {
      return false;
    }

    ensureHeightOffsets(config);
    const uint32_t xSegments = std::max(config.xSegments, 1u);
    const uint32_t zSegments = std::max(config.zSegments, 1u);
    const uint32_t xVertexCount = xSegments + 1;
    const float radiusSquared = radius * radius;
    bool changed = false;

    for (uint32_t zIndex = 0; zIndex <= zSegments; ++zIndex) {
      const float zAlpha =
          static_cast<float>(zIndex) / static_cast<float>(zSegments);
      const float z = (zAlpha - 0.5f) * config.sizeZ;
      for (uint32_t xIndex = 0; xIndex <= xSegments; ++xIndex) {
        const float xAlpha =
            static_cast<float>(xIndex) / static_cast<float>(xSegments);
        const float x = (xAlpha - 0.5f) * config.sizeX;
        const glm::vec2 delta = glm::vec2(x, z) - center;
        const float distanceSquared = glm::dot(delta, delta);
        if (distanceSquared > radiusSquared) {
          continue;
        }

        const float distance = std::sqrt(distanceSquared);
        const float linearFalloff = 1.0f - distance / radius;
        const float weight = smoothstep(linearFalloff);
        const size_t vertexIndex =
            static_cast<size_t>(zIndex) * xVertexCount + xIndex;
        config.heightOffsets[vertexIndex] += heightDelta * weight;
        changed = true;
      }
    }

    return changed;
  }

  static bool applyFlattenBrush(TerrainConfig &config, const glm::vec2 &center,
                                float radius, float targetHeight,
                                float maxHeightDelta) {
    if (radius <= 1e-6f || std::abs(maxHeightDelta) <= 1e-6f) {
      return false;
    }

    ensureHeightOffsets(config);
    const uint32_t xSegments = std::max(config.xSegments, 1u);
    const uint32_t zSegments = std::max(config.zSegments, 1u);
    const uint32_t xVertexCount = xSegments + 1;
    const float radiusSquared = radius * radius;
    bool changed = false;

    for (uint32_t zIndex = 0; zIndex <= zSegments; ++zIndex) {
      const float zAlpha =
          static_cast<float>(zIndex) / static_cast<float>(zSegments);
      const float z = (zAlpha - 0.5f) * config.sizeZ;
      for (uint32_t xIndex = 0; xIndex <= xSegments; ++xIndex) {
        const float xAlpha =
            static_cast<float>(xIndex) / static_cast<float>(xSegments);
        const float x = (xAlpha - 0.5f) * config.sizeX;
        const glm::vec2 delta = glm::vec2(x, z) - center;
        const float distanceSquared = glm::dot(delta, delta);
        if (distanceSquared > radiusSquared) {
          continue;
        }

        const float distance = std::sqrt(distanceSquared);
        const float linearFalloff = 1.0f - distance / radius;
        const float weight = smoothstep(linearFalloff);
        const size_t vertexIndex =
            static_cast<size_t>(zIndex) * xVertexCount + xIndex;
        const float baseHeight = sampleBaseHeight(config, x, z);
        const float desiredOffset = targetHeight - baseHeight;
        const float currentOffset = config.heightOffsets[vertexIndex];
        const float offsetDelta = glm::clamp(
            desiredOffset - currentOffset, -maxHeightDelta * weight,
            maxHeightDelta * weight);
        if (std::abs(offsetDelta) <= 1e-6f) {
          continue;
        }

        config.heightOffsets[vertexIndex] += offsetDelta;
        changed = true;
      }
    }

    return changed;
  }

  static bool applyColorBrush(TerrainConfig &config, const glm::vec2 &center,
                              float radius, const glm::vec4 &targetColor,
                              float blendAmount) {
    if (radius <= 1e-6f || blendAmount <= 1e-6f) {
      return false;
    }

    ensureVertexColors(config);
    const uint32_t xSegments = std::max(config.xSegments, 1u);
    const uint32_t zSegments = std::max(config.zSegments, 1u);
    const uint32_t xVertexCount = xSegments + 1;
    const float radiusSquared = radius * radius;
    bool changed = false;

    for (uint32_t zIndex = 0; zIndex <= zSegments; ++zIndex) {
      const float zAlpha =
          static_cast<float>(zIndex) / static_cast<float>(zSegments);
      const float z = (zAlpha - 0.5f) * config.sizeZ;
      for (uint32_t xIndex = 0; xIndex <= xSegments; ++xIndex) {
        const float xAlpha =
            static_cast<float>(xIndex) / static_cast<float>(xSegments);
        const float x = (xAlpha - 0.5f) * config.sizeX;
        const glm::vec2 delta = glm::vec2(x, z) - center;
        const float distanceSquared = glm::dot(delta, delta);
        if (distanceSquared > radiusSquared) {
          continue;
        }

        const float distance = std::sqrt(distanceSquared);
        const float linearFalloff = 1.0f - distance / radius;
        const float weight = smoothstep(linearFalloff);
        const float colorBlend = glm::clamp(blendAmount * weight, 0.0f, 1.0f);
        if (colorBlend <= 1e-6f) {
          continue;
        }

        const size_t vertexIndex =
            static_cast<size_t>(zIndex) * xVertexCount + xIndex;
        const glm::vec4 currentColor = config.vertexColors[vertexIndex];
        const glm::vec4 nextColor =
            glm::mix(currentColor, targetColor, colorBlend);
        if (glm::all(glm::epsilonEqual(currentColor, nextColor, 1e-6f))) {
          continue;
        }

        config.vertexColors[vertexIndex] = nextColor;
        changed = true;
      }
    }

    return changed;
  }

  static float maxHeightOffsetMagnitude(const TerrainConfig &config) {
    float maxOffset = 0.0f;
    for (const float offset : config.heightOffsets) {
      maxOffset = std::max(maxOffset, std::abs(offset));
    }
    return maxOffset;
  }

private:
  static float sampleBaseHeight(const TerrainConfig &config, float x, float z) {
    if (config.heightScale <= 1e-6f || config.noiseOctaves == 0) {
      return 0.0f;
    }

    float amplitude = 1.0f;
    float frequency = std::max(config.noiseFrequency, 1e-4f);
    float value = 0.0f;
    float amplitudeSum = 0.0f;

    for (uint32_t octave = 0; octave < config.noiseOctaves; ++octave) {
      value += amplitude *
               sampleValueNoise(x * frequency, z * frequency,
                                config.noiseSeed + octave * 1013u);
      amplitudeSum += amplitude;
      amplitude *= config.noisePersistence;
      frequency *= std::max(config.noiseLacunarity, 1.0f);
    }

    if (amplitudeSum <= 1e-6f) {
      return 0.0f;
    }
    return (value / amplitudeSum) * config.heightScale;
  }

  static float sampleHeightOffset(const TerrainConfig &config, float x, float z) {
    if (config.heightOffsets.empty()) {
      return 0.0f;
    }

    const uint32_t xSegments = std::max(config.xSegments, 1u);
    const uint32_t zSegments = std::max(config.zSegments, 1u);
    const uint32_t xVertexCount = xSegments + 1;
    const uint32_t zVertexCount = zSegments + 1;
    const size_t requiredVertexCount =
        static_cast<size_t>(xVertexCount) * zVertexCount;
    if (config.heightOffsets.size() != requiredVertexCount) {
      return 0.0f;
    }

    const float normalizedX =
        glm::clamp((x / std::max(config.sizeX, 1e-6f)) + 0.5f, 0.0f, 1.0f);
    const float normalizedZ =
        glm::clamp((z / std::max(config.sizeZ, 1e-6f)) + 0.5f, 0.0f, 1.0f);
    const float gridX = normalizedX * static_cast<float>(xSegments);
    const float gridZ = normalizedZ * static_cast<float>(zSegments);
    const uint32_t x0 =
        std::min(static_cast<uint32_t>(std::floor(gridX)), xSegments);
    const uint32_t z0 =
        std::min(static_cast<uint32_t>(std::floor(gridZ)), zSegments);
    const uint32_t x1 = std::min(x0 + 1u, xSegments);
    const uint32_t z1 = std::min(z0 + 1u, zSegments);
    const float tx = gridX - static_cast<float>(x0);
    const float tz = gridZ - static_cast<float>(z0);

    const auto sampleOffset = [&](uint32_t xIndex, uint32_t zIndex) {
      const size_t vertexIndex =
          static_cast<size_t>(zIndex) * xVertexCount + xIndex;
      return config.heightOffsets[vertexIndex];
    };

    const float h00 = sampleOffset(x0, z0);
    const float h10 = sampleOffset(x1, z0);
    const float h01 = sampleOffset(x0, z1);
    const float h11 = sampleOffset(x1, z1);
    const float hx0 = glm::mix(h00, h10, tx);
    const float hx1 = glm::mix(h01, h11, tx);
    return glm::mix(hx0, hx1, tz);
  }

  static glm::vec4 sampleVertexColor(const TerrainConfig &config, float x, float z) {
    if (config.vertexColors.empty()) {
      return glm::vec4(1.0f);
    }

    const uint32_t xSegments = std::max(config.xSegments, 1u);
    const uint32_t zSegments = std::max(config.zSegments, 1u);
    const uint32_t xVertexCount = xSegments + 1;
    const uint32_t zVertexCount = zSegments + 1;
    const size_t requiredVertexCount =
        static_cast<size_t>(xVertexCount) * zVertexCount;
    if (config.vertexColors.size() != requiredVertexCount) {
      return glm::vec4(1.0f);
    }

    const float normalizedX =
        glm::clamp((x / std::max(config.sizeX, 1e-6f)) + 0.5f, 0.0f, 1.0f);
    const float normalizedZ =
        glm::clamp((z / std::max(config.sizeZ, 1e-6f)) + 0.5f, 0.0f, 1.0f);
    const float gridX = normalizedX * static_cast<float>(xSegments);
    const float gridZ = normalizedZ * static_cast<float>(zSegments);
    const uint32_t x0 =
        std::min(static_cast<uint32_t>(std::floor(gridX)), xSegments);
    const uint32_t z0 =
        std::min(static_cast<uint32_t>(std::floor(gridZ)), zSegments);
    const uint32_t x1 = std::min(x0 + 1u, xSegments);
    const uint32_t z1 = std::min(z0 + 1u, zSegments);
    const float tx = gridX - static_cast<float>(x0);
    const float tz = gridZ - static_cast<float>(z0);

    const auto sampleColor = [&](uint32_t xIndex, uint32_t zIndex) {
      const size_t vertexIndex =
          static_cast<size_t>(zIndex) * xVertexCount + xIndex;
      return config.vertexColors[vertexIndex];
    };

    const glm::vec4 c00 = sampleColor(x0, z0);
    const glm::vec4 c10 = sampleColor(x1, z0);
    const glm::vec4 c01 = sampleColor(x0, z1);
    const glm::vec4 c11 = sampleColor(x1, z1);
    const glm::vec4 cx0 = glm::mix(c00, c10, tx);
    const glm::vec4 cx1 = glm::mix(c01, c11, tx);
    return glm::mix(cx0, cx1, tz);
  }

  static void computeNormals(TerrainMeshData &mesh) {
    if (mesh.vertices.empty() || mesh.indices.size() < 3) {
      return;
    }

    for (auto &vertex : mesh.vertices) {
      vertex.normal = glm::vec3(0.0f);
    }

    for (size_t index = 0; index + 2 < mesh.indices.size(); index += 3) {
      const uint32_t i0 = mesh.indices[index];
      const uint32_t i1 = mesh.indices[index + 1];
      const uint32_t i2 = mesh.indices[index + 2];
      if (i0 >= mesh.vertices.size() || i1 >= mesh.vertices.size() ||
          i2 >= mesh.vertices.size()) {
        continue;
      }

      const glm::vec3 edgeA =
          mesh.vertices[i1].position - mesh.vertices[i0].position;
      const glm::vec3 edgeB =
          mesh.vertices[i2].position - mesh.vertices[i0].position;
      const glm::vec3 faceNormal = glm::cross(edgeA, edgeB);
      const float faceLength = glm::length(faceNormal);
      if (faceLength <= 1e-6f) {
        continue;
      }

      const glm::vec3 normalizedFaceNormal = faceNormal / faceLength;
      mesh.vertices[i0].normal += normalizedFaceNormal;
      mesh.vertices[i1].normal += normalizedFaceNormal;
      mesh.vertices[i2].normal += normalizedFaceNormal;
    }

    for (auto &vertex : mesh.vertices) {
      const float normalLength = glm::length(vertex.normal);
      vertex.normal = normalLength <= 1e-6f
                          ? glm::vec3(0.0f, 1.0f, 0.0f)
                          : vertex.normal / normalLength;
    }
  }

  static float sampleValueNoise(float x, float z, uint32_t seed) {
    const int x0 = static_cast<int>(std::floor(x));
    const int z0 = static_cast<int>(std::floor(z));
    const int x1 = x0 + 1;
    const int z1 = z0 + 1;

    const float tx = smoothstep(x - static_cast<float>(x0));
    const float tz = smoothstep(z - static_cast<float>(z0));

    const float v00 = randomSignedUnit(x0, z0, seed);
    const float v10 = randomSignedUnit(x1, z0, seed);
    const float v01 = randomSignedUnit(x0, z1, seed);
    const float v11 = randomSignedUnit(x1, z1, seed);

    const float a = glm::mix(v00, v10, tx);
    const float b = glm::mix(v01, v11, tx);
    return glm::mix(a, b, tz);
  }

  static float smoothstep(float value) {
    const float clamped = glm::clamp(value, 0.0f, 1.0f);
    return clamped * clamped * (3.0f - 2.0f * clamped);
  }

  static float randomSignedUnit(int x, int z, uint32_t seed) {
    uint32_t hash = static_cast<uint32_t>(x) * 0x8da6b343u;
    hash ^= static_cast<uint32_t>(z) * 0xd8163841u;
    hash ^= seed * 0xcb1ab31fu;
    hash ^= hash >> 13;
    hash *= 0x85ebca6bu;
    hash ^= hash >> 16;
    const float normalized =
        static_cast<float>(hash & 0x00ffffffu) / 16777215.0f;
    return normalized * 2.0f - 1.0f;
  }
};
