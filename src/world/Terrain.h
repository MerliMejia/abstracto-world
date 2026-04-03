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
};

struct TerrainVertex {
  glm::vec3 position{0.0f};
  glm::vec3 normal{0.0f, 1.0f, 0.0f};
  glm::vec2 uv{0.0f};
};

struct TerrainMeshData {
  std::vector<TerrainVertex> vertices;
  std::vector<uint32_t> indices;
};

class TerrainGenerator {
public:
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

private:
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
