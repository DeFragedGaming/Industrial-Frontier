#pragma once

#include "industrial_frontier/world/VoxelMesher.hpp"

#include <cstdint>
#include <vector>

namespace industrial_frontier
{
    enum class TreeSpecies : std::uint8_t
    {
        Pine,
        Broadleaf,
        Birch
    };

    struct ForestTree
    {
        std::uint64_t id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float height = 0.0f;
        float crownRadius = 0.0f;
        float rotation = 0.0f;
        TreeSpecies species = TreeSpecies::Pine;
    };

    struct ForestPebble
    {
        std::uint64_t id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float radius = 0.0f;
    };

    class ForestGenerator
    {
    public:
        static void GenerateTrees(
            const VoxelTerrain& terrain,
            int chunkX,
            int chunkZ,
            std::vector<ForestTree>& trees);

        static void GeneratePebbles(const VoxelTerrain& terrain, int chunkX,
            int chunkZ, std::vector<ForestPebble>& pebbles);

        static void BuildPebbleMesh(const std::vector<ForestPebble>& pebbles,
            VoxelMesh& mesh);

        static void BuildMesh(
            const std::vector<ForestTree>& trees,
            VoxelMesh& mesh);

        static void GenerateChunk(
            const VoxelTerrain& terrain,
            int chunkX,
            int chunkZ,
            VoxelMesh& mesh);
    };
}
