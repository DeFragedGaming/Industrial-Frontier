
#pragma once

#include "industrial_frontier/world/TerrainGenerator.hpp"

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

namespace industrial_frontier
{
    class GeologyGenerator;
    enum class TerrainMaterial
    {
        None,
        Grass,
        DirtClay,
        Stone,
        Snow,
        Bedrock
    };

    struct VoxelEdit
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        float radius = 2.0f;
        float strength = 1.0f;

        bool addMaterial = false;
    };

    struct CaveEntrance
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float chamberX = 0.0f;
        float chamberY = 0.0f;
        float chamberZ = 0.0f;
    };

    struct VoxelChunkRange
    {
        int minX = 0;
        int maxX = 0;
        int minZ = 0;
        int maxZ = 0;
    };

    class VoxelTerrain
    {
    public:
        static constexpr float BedrockSurfaceY = -80.0f;

        explicit VoxelTerrain(
            const TerrainSettings& settings = TerrainSettings{},
            float chunkSize = 32.0f,
            float voxelSpacing = 1.0f
        );

        ~VoxelTerrain();

        const GeologyGenerator* GetGeology() const;

        VoxelTerrain(const VoxelTerrain&) = delete;
        VoxelTerrain& operator=(const VoxelTerrain&) = delete;

        float SampleDensity(
            float x,
            float y,
            float z
        ) const;

        float GetSurfaceHeight(float x, float z) const;
        float GetEntranceRockWeight(float x, float z) const;
        TerrainMaterial GetMaterialAt(
            float x, float y, float z, float normalY) const;

        bool IsSolid(
            float x,
            float y,
            float z
        ) const;

        bool DigSphere(
            float x,
            float y,
            float z,
            float radius,
            float strength = 1.0f
        );

        bool FillSphere(
            float x,
            float y,
            float z,
            float radius,
            float strength = 1.0f
        );

        bool ApplyEdit(const VoxelEdit& edit);

        VoxelChunkRange GetAffectedChunks(
            const VoxelEdit& edit
        ) const;

        std::vector<VoxelEdit> GetEdits() const;
        bool DepleteDeposit(std::uint64_t depositId);
        bool IsDepositDepleted(std::uint64_t depositId) const;
        std::vector<std::uint64_t> GetDepletedDeposits() const;
        void SetDepletedDeposits(const std::vector<std::uint64_t>& ids);

        std::vector<CaveEntrance> FindCaveEntrances(
            float minX,
            float maxX,
            float minZ,
            float maxZ
        ) const;

        void SetEdits(
            const std::vector<VoxelEdit>& edits
        );

        void ClearEdits();

        float GetChunkSize() const;
        float GetVoxelSpacing() const;

        const TerrainGenerator& GetGenerator() const;

        std::uint64_t GetRevision() const;

    private:
        TerrainGenerator m_generator;
        std::unique_ptr<GeologyGenerator> m_geology;

        float m_chunkSize = 32.0f;
        float m_voxelSpacing = 1.0f;

        mutable std::shared_mutex m_mutex;

        std::vector<VoxelEdit> m_edits;
        std::unordered_set<std::uint64_t> m_depletedDeposits;

        std::uint64_t m_revision = 0;
        std::uint64_t m_generationId = 0;

        static float Clamp01(float value);

        static float SmoothFalloff(float value);

        static float DistanceSquared(
            float ax,
            float ay,
            float az,
            float bx,
            float by,
            float bz
        );
    };
}
