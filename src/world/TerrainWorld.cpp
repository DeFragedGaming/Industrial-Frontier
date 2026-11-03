#include "industrial_frontier/world/TerrainWorld.hpp"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace industrial_frontier
{
    TerrainWorld::TerrainWorld()
        : TerrainWorld(TerrainSettings{})
    {
    }

    TerrainWorld::TerrainWorld(
        const TerrainSettings& settings,
        float chunkSize,
        float vertexSpacing)
        : m_generator(settings),
          m_chunkSize(std::max(chunkSize, 1.0f)),
          m_vertexSpacing(std::max(vertexSpacing, 0.1f))
    {
    }

    std::uint64_t TerrainWorld::MakeChunkKey(int chunkX, int chunkZ)
    {
        return
            (static_cast<std::uint64_t>(
                static_cast<std::uint32_t>(chunkX)
            ) << 32) |
            static_cast<std::uint32_t>(chunkZ);
    }

    bool TerrainWorld::GenerateWorld(int chunkRadius)
    {
        Clear();

        chunkRadius = std::max(chunkRadius, 0);

        bool success = true;

        for (int z = -chunkRadius; z <= chunkRadius; ++z)
        {
            for (int x = -chunkRadius; x <= chunkRadius; ++x)
            {
                if (!GenerateChunk(x, z))
                {
                    success = false;
                }
            }
        }

        return success;
    }

    bool TerrainWorld::GenerateChunk(int chunkX, int chunkZ)
    {
        const std::uint64_t key = MakeChunkKey(chunkX, chunkZ);

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

    bool TerrainWorld::AddGeneratedChunk(TerrainChunk&& chunk)
    {
        if (!chunk.IsGenerated())
        {
            return false;
        }

        const std::uint64_t key = MakeChunkKey(
            chunk.GetChunkX(),
            chunk.GetChunkZ()
        );

        return m_chunks.emplace(
            key,
            std::move(chunk)
        ).second;
    }

    bool TerrainWorld::RemoveChunk(int chunkX, int chunkZ)
    {
        return m_chunks.erase(
            MakeChunkKey(chunkX, chunkZ)
        ) > 0;
    }

    void TerrainWorld::Clear()
    {
        m_chunks.clear();
    }

    TerrainChunk* TerrainWorld::GetChunk(int chunkX, int chunkZ)
    {
        const auto iterator = m_chunks.find(
            MakeChunkKey(chunkX, chunkZ)
        );

        return iterator == m_chunks.end()
            ? nullptr
            : &iterator->second;
    }

    const TerrainChunk* TerrainWorld::GetChunk(
        int chunkX,
        int chunkZ) const
    {
        const auto iterator = m_chunks.find(
            MakeChunkKey(chunkX, chunkZ)
        );

        return iterator == m_chunks.end()
            ? nullptr
            : &iterator->second;
    }

    const std::unordered_map<std::uint64_t, TerrainChunk>&
    TerrainWorld::GetChunks() const
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