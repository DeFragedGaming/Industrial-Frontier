
#pragma once

#include "industrial_frontier/world/TerrainGenerator.hpp"

#include <cstdint>
#include <shared_mutex>
#include <vector>

namespace industrial_frontier
{
    struct VoxelEdit
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        float radius = 2.0f;
        float strength = 1.0f;

        bool addMaterial = false;
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
        explicit VoxelTerrain(
            const TerrainSettings& settings = TerrainSettings{},
            float chunkSize = 32.0f,
            float voxelSpacing = 1.0f
        );

        VoxelTerrain(const VoxelTerrain&) = delete;
        VoxelTerrain& operator=(const VoxelTerrain&) = delete;

        float SampleDensity(
            float x,
            float y,
            float z
        ) const;

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

        float m_chunkSize = 32.0f;
        float m_voxelSpacing = 1.0f;

        mutable std::shared_mutex m_mutex;

        std::vector<VoxelEdit> m_edits;

        std::uint64_t m_revision = 0;

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
