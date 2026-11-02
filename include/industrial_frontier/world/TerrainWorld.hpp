#pragma once

#include "industrial_frontier/world/TerrainChunk.hpp"
#include "industrial_frontier/world/TerrainGenerator.hpp"

#include <cstdint>
#include <unordered_map>

namespace industrial_frontier
{
    class TerrainWorld
    {
    public:
        TerrainWorld();

        explicit TerrainWorld(
            const TerrainSettings& settings,
            float chunkSize = 32.0f,
            float vertexSpacing = 1.0f
        );

        TerrainWorld(const TerrainWorld&) = delete;
        TerrainWorld& operator=(const TerrainWorld&) = delete;

        TerrainWorld(TerrainWorld&&) noexcept = default;
        TerrainWorld& operator=(TerrainWorld&&) noexcept = default;

        bool GenerateWorld(int chunkRadius);
        bool GenerateChunk(int chunkX, int chunkZ);

        void Clear();

        TerrainChunk* GetChunk(int chunkX, int chunkZ);

        const TerrainChunk* GetChunk(
            int chunkX,
            int chunkZ
        ) const;

        const std::unordered_map<
            std::uint64_t,
            TerrainChunk
        >& GetChunks() const;

        const TerrainGenerator& GetGenerator() const;
        const TerrainSettings& GetSettings() const;

        float GetChunkSize() const;
        float GetVertexSpacing() const;

        int GetChunkCount() const;

    private:
        TerrainGenerator m_generator;

        float m_chunkSize = 32.0f;
        float m_vertexSpacing = 1.0f;

        std::unordered_map<
            std::uint64_t,
            TerrainChunk
        > m_chunks;

        static std::uint64_t MakeChunkKey(
            int chunkX,
            int chunkZ
        );
    };
}