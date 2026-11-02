#pragma once

#include "industrial_frontier/world/TerrainGenerator.hpp"

#include <cstddef>
#include <vector>

namespace industrial_frontier
{
    struct TerrainVertex
    {
        float position[3];
        float normal[3];
        float color[3];
    };

    class TerrainChunk
    {
    public:
        TerrainChunk();

        TerrainChunk(
            int chunkX,
            int chunkZ,
            float chunkSize,
            float vertexSpacing
        );

        bool Generate(const TerrainGenerator& generator);

        int GetChunkX() const;
        int GetChunkZ() const;

        float GetChunkSize() const;

        const std::vector<TerrainVertex>& GetVertices() const;
        const std::vector<unsigned int>& GetIndices() const;

        bool IsGenerated() const;

        void Clear();

    private:
        int m_chunkX = 0;
        int m_chunkZ = 0;

        float m_chunkSize = 32.0f;
        float m_vertexSpacing = 1.0f;

        bool m_generated = false;

        std::vector<TerrainVertex> m_vertices;
        std::vector<unsigned int> m_indices;

        TerrainVertex CreateVertex(
            const TerrainGenerator& generator,
            float worldX,
            float worldZ
        ) const;

        void CalculateVertexColor(
            TerrainVertex& vertex,
            float height
        ) const;
    };
}