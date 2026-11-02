
#include "industrial_frontier/world/TerrainChunk.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace industrial_frontier
{
    TerrainChunk::TerrainChunk() = default;

    TerrainChunk::TerrainChunk(
        int chunkX,
        int chunkZ,
        float chunkSize,
        float vertexSpacing
    )
        : m_chunkX(chunkX),
          m_chunkZ(chunkZ),
          m_chunkSize(chunkSize),
          m_vertexSpacing(vertexSpacing)
    {
    }

    bool TerrainChunk::Generate(const TerrainGenerator& generator)
    {
        Clear();

        if (m_chunkSize <= 0.0f ||
            m_vertexSpacing <= 0.0f)
        {
            return false;
        }

        const int segments = static_cast<int>(
            std::ceil(m_chunkSize / m_vertexSpacing)
        );

        if (segments <= 0)
        {
            return false;
        }

        const float spacing =
            m_chunkSize / static_cast<float>(segments);

        const std::size_t vertexCount =
            static_cast<std::size_t>(segments + 1) *
            static_cast<std::size_t>(segments + 1);

        m_vertices.reserve(vertexCount);

        const float originX =
            static_cast<float>(m_chunkX) * m_chunkSize;

        const float originZ =
            static_cast<float>(m_chunkZ) * m_chunkSize;

        for (int z = 0; z <= segments; ++z)
        {
            for (int x = 0; x <= segments; ++x)
            {
                const float worldX =
                    originX + static_cast<float>(x) * spacing;

                const float worldZ =
                    originZ + static_cast<float>(z) * spacing;

                m_vertices.push_back(
                    CreateVertex(generator, worldX, worldZ)
                );
            }
        }

        const std::size_t indexCount =
            static_cast<std::size_t>(segments) *
            static_cast<std::size_t>(segments) * 6;

        m_indices.reserve(indexCount);

        const unsigned int rowWidth =
            static_cast<unsigned int>(segments + 1);

        for (int z = 0; z < segments; ++z)
        {
            for (int x = 0; x < segments; ++x)
            {
                const unsigned int a =
                    static_cast<unsigned int>(z) * rowWidth +
                    static_cast<unsigned int>(x);

                const unsigned int b = a + 1;
                const unsigned int c = a + rowWidth;
                const unsigned int d = c + 1;

                m_indices.push_back(a);
                m_indices.push_back(c);
                m_indices.push_back(b);

                m_indices.push_back(b);
                m_indices.push_back(c);
                m_indices.push_back(d);
            }
        }

        m_generated =
            !m_vertices.empty() &&
            !m_indices.empty();

        return m_generated;
    }

    int TerrainChunk::GetChunkX() const
    {
        return m_chunkX;
    }

    int TerrainChunk::GetChunkZ() const
    {
        return m_chunkZ;
    }

    float TerrainChunk::GetChunkSize() const
    {
        return m_chunkSize;
    }

    const std::vector<TerrainVertex>&
    TerrainChunk::GetVertices() const
    {
        return m_vertices;
    }

    const std::vector<unsigned int>&
    TerrainChunk::GetIndices() const
    {
        return m_indices;
    }

    bool TerrainChunk::IsGenerated() const
    {
        return m_generated;
    }

    void TerrainChunk::Clear()
    {
        m_vertices.clear();
        m_indices.clear();
        m_generated = false;
    }

    TerrainVertex TerrainChunk::CreateVertex(
        const TerrainGenerator& generator,
        float worldX,
        float worldZ
    ) const
    {
        TerrainVertex vertex{};

        const float height =
            generator.GetHeight(worldX, worldZ);

        const float normalSampleDistance =
            std::max(m_vertexSpacing * 0.5f, 0.01f);

        const float leftHeight = generator.GetHeight(
            worldX - normalSampleDistance,
            worldZ
        );

        const float rightHeight = generator.GetHeight(
            worldX + normalSampleDistance,
            worldZ
        );

        const float backHeight = generator.GetHeight(
            worldX,
            worldZ - normalSampleDistance
        );

        const float frontHeight = generator.GetHeight(
            worldX,
            worldZ + normalSampleDistance
        );

        const float slopeX =
            (rightHeight - leftHeight) /
            (normalSampleDistance * 2.0f);

        const float slopeZ =
            (frontHeight - backHeight) /
            (normalSampleDistance * 2.0f);

        const float normalLength = std::sqrt(
            slopeX * slopeX +
            slopeZ * slopeZ +
            1.0f
        );

        vertex.position[0] = worldX;
        vertex.position[1] = height;
        vertex.position[2] = worldZ;

        vertex.normal[0] = -slopeX / normalLength;
        vertex.normal[1] = 1.0f / normalLength;
        vertex.normal[2] = -slopeZ / normalLength;

        CalculateVertexColor(vertex, height);

        return vertex;
    }

    void TerrainChunk::CalculateVertexColor(
        TerrainVertex& vertex,
        float height
    ) const
    {
        const float slope = 1.0f - vertex.normal[1];

        float red = 0.20f;
        float green = 0.42f;
        float blue = 0.18f;

        if (height < -2.0f)
        {
            red = 0.24f;
            green = 0.25f;
            blue = 0.27f;
        }
        else if (height < 5.0f)
        {
            red = 0.32f;
            green = 0.43f;
            blue = 0.22f;
        }
        else if (height < 18.0f)
        {
            red = 0.35f;
            green = 0.38f;
            blue = 0.24f;
        }
        else if (height < 32.0f)
        {
            red = 0.42f;
            green = 0.38f;
            blue = 0.31f;
        }
        else
        {
            red = 0.72f;
            green = 0.72f;
            blue = 0.70f;
        }

        if (slope > 0.30f)
        {
            red = std::min(red + 0.12f, 1.0f);
            green = std::min(green + 0.10f, 1.0f);
            blue = std::min(blue + 0.08f, 1.0f);
        }

        vertex.color[0] = red;
        vertex.color[1] = green;
        vertex.color[2] = blue;
    }
}
