#include "world/HeightfieldGrid.h"
#include "world/HeightfieldMeshBuilder.h"
#include "world/HeightfieldNoise.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static bool nearlyEqual(float a, float b, float epsilon = 1e-4f) {
  return std::abs(a - b) <= epsilon;
}

static void require(bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

static void requireVec3(const glm::vec3 &actual, const glm::vec3 &expected,
                        const std::string &label) {
  if (nearlyEqual(actual.x, expected.x) && nearlyEqual(actual.y, expected.y) &&
      nearlyEqual(actual.z, expected.z)) {
    return;
  }

  std::ostringstream message;
  message << label << " expected (" << expected.x << ", " << expected.y << ", "
          << expected.z << ") but got (" << actual.x << ", " << actual.y
          << ", " << actual.z << ")";
  throw std::runtime_error(message.str());
}

static bool nearlyEqualVec(const std::vector<float> &lhs,
                           const std::vector<float> &rhs,
                           float epsilon = 1e-4f) {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t index = 0; index < lhs.size(); ++index) {
    if (!nearlyEqual(lhs[index], rhs[index], epsilon)) {
      return false;
    }
  }
  return true;
}

int main() {
  try {
    const HeightfieldGrid grid =
        HeightfieldGrid::flat(4, 3, 2.0f, {-4.0f, -3.0f});
    require(grid.isValid(), "flat grid should be valid");
    require(grid.pointCount() == 20, "grid should have 20 points");
    require(grid.pointsX() == 5, "grid should expose 5 points along x");
    require(grid.pointsZ() == 4, "grid should expose 4 points along z");

    const HeightfieldMeshData mesh = HeightfieldMeshBuilder::build(grid);
    require(mesh.vertices.size() == 20,
            "mesh vertex count should match the grid point count");
    require(mesh.indices.size() == 72,
            "mesh index count should be cellsX * cellsZ * 6");

    requireVec3(mesh.vertices.front().position, {-4.0f, 0.0f, -3.0f},
                "first vertex");
    requireVec3(mesh.vertices.back().position, {4.0f, 0.0f, 3.0f},
                "last vertex");

    const std::vector<uint32_t> expectedFirstCell{0, 5, 6, 6, 1, 0};
    require(std::equal(expectedFirstCell.begin(), expectedFirstCell.end(),
                       mesh.indices.begin()),
            "first cell indices should match the expected triangle layout");

    const HeightfieldSample originSample = grid.sample({-4.0f, -3.0f});
    require(originSample.insideBounds,
            "origin sample should be reported inside bounds");
    require(nearlyEqual(originSample.height, 0.0f),
            "origin sample height should be flat");
    requireVec3(originSample.position, {-4.0f, 0.0f, -3.0f},
                "origin sample position");

    const HeightfieldSample centerSample = grid.sample({-1.0f, -1.0f});
    require(centerSample.insideBounds,
            "interior sample should be reported inside bounds");
    require(nearlyEqual(centerSample.height, 0.0f),
            "interior sample height should be flat");
    requireVec3(centerSample.normal, {0.0f, 1.0f, 0.0f},
                "interior sample normal");

    const HeightfieldSample edgeSample = grid.sample({4.0f, 3.0f});
    require(edgeSample.insideBounds,
            "max edge sample should still be inside bounds");
    require(nearlyEqual(edgeSample.height, 0.0f),
            "edge sample height should be flat");

    const HeightfieldSample outsideSample = grid.sample({4.25f, 3.0f});
    require(!outsideSample.insideBounds,
            "sample beyond the grid width should be out of bounds");

    HeightfieldGrid noisyGrid =
        HeightfieldGrid::flat(4, 3, 2.0f, {-4.0f, -3.0f});
    HeightfieldGrid repeatedNoisyGrid =
        HeightfieldGrid::flat(4, 3, 2.0f, {-4.0f, -3.0f});
    HeightfieldGrid differentSeedGrid =
        HeightfieldGrid::flat(4, 3, 2.0f, {-4.0f, -3.0f});

    const HeightfieldNoiseSettings noiseSettings{
        .seed = 42,
        .baseHeight = 1.5f,
        .amplitude = 3.0f,
        .frequency = 0.17f,
        .octaves = 5,
        .lacunarity = 2.1f,
        .persistence = 0.48f,
        .offset = {8.0f, -3.5f},
    };
    HeightfieldNoise::apply(noisyGrid, noiseSettings);
    HeightfieldNoise::apply(repeatedNoisyGrid, noiseSettings);

    HeightfieldNoiseSettings differentSeedSettings = noiseSettings;
    differentSeedSettings.seed += 1;
    HeightfieldNoise::apply(differentSeedGrid, differentSeedSettings);

    require(nearlyEqualVec(noisyGrid.heights, repeatedNoisyGrid.heights),
            "noise application should be deterministic for the same settings");
    require(!nearlyEqualVec(noisyGrid.heights, differentSeedGrid.heights),
            "changing the seed should change the generated terrain");

    const auto [minHeightIt, maxHeightIt] =
        std::minmax_element(noisyGrid.heights.begin(), noisyGrid.heights.end());
    require(minHeightIt != noisyGrid.heights.end(),
            "noise grid should contain generated heights");
    require(*minHeightIt >= noiseSettings.baseHeight - noiseSettings.amplitude -
                1e-4f,
            "noise heights should respect the configured minimum range");
    require(*maxHeightIt <= noiseSettings.baseHeight + noiseSettings.amplitude +
                1e-4f,
            "noise heights should respect the configured maximum range");

    const float averageHeight =
        std::accumulate(noisyGrid.heights.begin(), noisyGrid.heights.end(), 0.0f) /
        static_cast<float>(noisyGrid.heights.size());
    require(std::abs(averageHeight - noiseSettings.baseHeight) < 1.2f,
            "noise average height should stay near the base height");

    bool foundVariation = false;
    for (const float height : noisyGrid.heights) {
      if (!nearlyEqual(height, noiseSettings.baseHeight, 1e-3f)) {
        foundVariation = true;
        break;
      }
    }
    require(foundVariation, "noise should generate non-flat terrain heights");

    const HeightfieldSample noisyPointSample =
        noisyGrid.sample({-2.0f, -1.0f});
    require(noisyPointSample.insideBounds,
            "noisy grid sample should stay queryable");
    require(nearlyEqual(noisyPointSample.height, noisyGrid.heightAt(1, 1)),
            "sampling at a grid vertex should match the stored noisy height");

    const HeightfieldMeshData noisyMesh = HeightfieldMeshBuilder::build(noisyGrid);
    require(nearlyEqual(noisyMesh.vertices[noisyGrid.pointIndex(1, 1)].position.y,
                        noisyGrid.heightAt(1, 1)),
            "mesh vertex y should match the generated noisy height");

    std::cout << "AbstractoWorldGridTest PASS\n";
    std::cout << "  cells: " << grid.cellsX << " x " << grid.cellsZ << "\n";
    std::cout << "  cellSize: " << std::fixed << std::setprecision(2)
              << grid.cellSize << "\n";
    std::cout << "  vertices: " << mesh.vertices.size() << "\n";
    std::cout << "  indices: " << mesh.indices.size() << "\n";
    std::cout << "  world span: [(" << grid.originXZ.x << ", " << grid.originXZ.y
              << ") -> (" << grid.originXZ.x + grid.width() << ", "
              << grid.originXZ.y + grid.depth() << ")]\n";
    std::cout << "  noisy height range: [" << *minHeightIt << ", "
              << *maxHeightIt << "]\n";
    std::cout << "  noisy sample (1,1): " << noisyGrid.heightAt(1, 1) << "\n";
    return 0;
  } catch (const std::exception &exception) {
    std::cerr << "AbstractoWorldGridTest FAIL: " << exception.what() << "\n";
    return 1;
  }
}
