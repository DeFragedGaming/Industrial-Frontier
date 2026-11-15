#pragma once

#include "industrial_frontier/resources/MineralDatabase.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace industrial_frontier
{
    class TerrainGenerator;
    class VoxelTerrain;

    enum class DepositKind
    {
        UndergroundVein,
        FiniteOutcrop,
        PermanentNode
    };

    struct DepositComponent
    {
        std::string mineralId;
        float fraction = 0.0f;
    };

    struct GeologicalDeposit
    {
        std::uint64_t id = 0;
        DepositKind kind = DepositKind::UndergroundVein;
        std::string mineralId;
        std::vector<DepositComponent> components;
        std::vector<DepositComponent> gangue;
        std::string hostGeology;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float radius = 2.0f;
        float length = 10.0f;
        float directionX = 1.0f;
        float directionY = 0.0f;
        float directionZ = 0.0f;
        float grade = 0.0f;
        float reserveKg = 0.0f;
        float color[3] = {0.5f, 0.5f, 0.5f};
        bool continuousExtraction = false;
    };

    class GeologyGenerator
    {
    public:
        explicit GeologyGenerator(int worldSeed,
            const TerrainGenerator* terrainGenerator = nullptr,
            const VoxelTerrain* voxelTerrain = nullptr);

        bool LoadCatalog(const std::filesystem::path& path, std::string& error);
        bool IsReady() const;
        std::size_t MineralCount() const;

        std::vector<GeologicalDeposit> FindSurfaceDeposits(
            float minX, float maxX, float minZ, float maxZ) const;

        std::vector<GeologicalDeposit> FindUndergroundVeins(
            float minX, float maxX,
            float minY, float maxY,
            float minZ, float maxZ) const;

        std::vector<GeologicalDeposit> FindUndergroundNodes(
            float minX, float maxX,
            float minY, float maxY,
            float minZ, float maxZ) const;

        const MineralDefinition* FindMineral(const std::string& id) const;

        static float VeinInfluence(
            const GeologicalDeposit& deposit,
            float x, float y, float z);

    private:
        int m_worldSeed = 0;
        const TerrainGenerator* m_terrainGenerator = nullptr;
        const VoxelTerrain* m_voxelTerrain = nullptr;
        MineralDatabase m_database;

        GeologicalDeposit GenerateSurfaceSite(
            int cellX, int cellZ, DepositKind kind) const;

        void AssignComposition(GeologicalDeposit& deposit,
            std::uint32_t siteHash, float depth) const;
    };
}
