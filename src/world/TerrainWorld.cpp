#include "industrial_frontier/world/TerrainWorld.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace industrial_frontier
{
    TerrainWorld::TerrainWorld()
        : m_generator(),
          m_chunkSize(32.0f),
          m_vertexSpacing(1.0f)
    {
    }

    TerrainWorld::TerrainWorld(
        const TerrainSettings& settings,
        float chunkSize,
        float vertexSpacing
    )
        : m_generator(settings),
          m_chunkSize(std::max(chunkSize, 1.0f)),
          m_vertexSpacing(std::max(vertexSpacing, 0.1f))
    {
    }

    std::uint64_t TerrainWorld::MakeChunkKey(
        int chunkX,
        int chunkZ
    )
    {
        const std::uint64_t x =
            static_cast<std::uint32_t>(chunkX);

        const std::uint64_t z =
            static_cast<std::uint32_t>(chunkZ);

        return (x << 32) | z;
    }

    bool TerrainWorld::GenerateWorld(int chunkRadius)
    {
        Clear();

        chunkRadius = std::max(chunkRadius, 0);

        const std::uint64_t sideLength =
            static_cast<std::uint64_t>(chunkRadius) * 2u + 1u;

        m_chunks.reserve(
            static_cast<std::size_t>(sideLength * sideLength)
        );

        for (int chunkZ = -chunkRadius;
             chunkZ <= chunkRadius;
             ++chunkZ)
        {
            for (int chunkX = -chunkRadius;
                 chunkX <= chunkRadius;
                 ++chunkX)
            {
                if (!GenerateChunk(chunkX, chunkZ))
                {
                    Clear();
                    return false;
                }
            }
        }

        return !m_chunks.empty();
    }

    bool TerrainWorld::GenerateChunk(
        int chunkX,
        int chunkZ
    )
    {
        const std::uint64_t key =
            MakeChunkKey(chunkX, chunkZ);

        if (m_chunks.find(key) != m_chunks.end())
        {
            return true;
        }

        TerrainChunk chunk(
            chunkX,
            chunkZ,
            m_chunkSize,
            m_vertexSpacing
        );

        if (!chunk.Generate(m_generator))
        {
            return false;
        }

        m_chunks.emplace(key, std::move(chunk));

        return true;
    }

    void TerrainWorld::Clear()
    {
        m_chunks.clear();
    }

    TerrainChunk* TerrainWorld::GetChunk(
        int chunkX,
        int chunkZ
    )
    {
        const std::uint64_t key =
            MakeChunkKey(chunkX, chunkZ);

        const auto iterator = m_chunks.find(key);

        if (iterator == m_chunks.end())
        {
            return nullptr;
        }

        return &iterator->second;
    }

    const TerrainChunk* TerrainWorld::GetChunk(
        int chunkX,
        int chunkZ
    ) const
    {
        const std::uint64_t key =
            MakeChunkKey(chunkX, chunkZ);

        const auto iterator = m_chunks.find(key);

        if (iterator == m_chunks.end())
        {
            return nullptr;
        }

        return &iterator->second;
    }

    const std::unordered_map<
        std::uint64_t,
        TerrainChunk
    >& TerrainWorld::GetChunks() const
    {
        return m_chunks;
    }

    const TerrainGenerator& TerrainWorld::GetGenerator() const
    {
        return m_generator;
    }

    const TerrainSettings& TerrainWorld::GetSettings() const
    {
        return m_generator.GetSettings();
    }

    float TerrainWorld::GetChunkSize() const
    {
        return m_chunkSize;
    }

    float TerrainWorld::GetVertexSpacing() const
    {
        return m_vertexSpacing;
    }

    int TerrainWorld::GetChunkCount() const
    {
        return static_cast<int>(m_chunks.size());
    }
}