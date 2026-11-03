#include "industrial_frontier/world/TerrainChunk.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace industrial_frontier
{
    namespace
    {
        struct Color
        {
            float r;
            float g;
            float b;
        };

        Color LerpColor(const Color& a, const Color& b, float t)
        {
            t = std::clamp(t, 0.0f, 1.0f);

            return {
                a.r + (b.r - a.r) * t,
                a.g + (b.g - a.g) * t,
                a.b + (b.b - a.b) * t
            };
        }

        float SmoothBlend(float value, float minimum, float maximum)
        {
            if (maximum <= minimum)
            {
                return 0.0f;
            }

            const float t = std::clamp(
                (value - minimum) / (maximum - minimum),
                0.0f,
                1.0f
            );

            return t * t * (3.0f - 2.0f * t);
        }

        float Length(float x, float y, float z)
        {
            return std::sqrt(x * x + y * y + z * z);
        }
    }

    TerrainChunk::TerrainChunk()
    {
    }

    TerrainChunk::TerrainChunk(
        int chunkX,
        int chunkZ,
        float chunkSize,
        float vertexSpacing)
        : m_chunkX(chunkX),
          m_chunkZ(chunkZ),
          m_chunkSize(std::max(chunkSize, 1.0f)),
          m_vertexSpacing(std::max(vertexSpacing, 0.1f))
    {
    }

    bool TerrainChunk::Generate(const TerrainGenerator& generator)
    {
        Clear();

        const int segments = std::max(
            1,
            static_cast<int>(
                std::ceil(m_chunkSize / m_vertexSpacing)
            )
        );

        const float spacing = m_chunkSize /
            static_cast<float>(segments);

        const float startX =
            static_cast<float>(m_chunkX) * m_chunkSize;

        const float startZ =
            static_cast<float>(m_chunkZ) * m_chunkSize;

        const std::size_t vertexCount =
            static_cast<std::size_t>(segments + 1) *
            static_cast<std::size_t>(segments + 1);

        m_vertices.reserve(vertexCount);

        m_indices.reserve(
            static_cast<std::size_t>(segments) *
            static_cast<std::size_t>(segments) * 6
        );

        for (int z = 0; z <= segments; ++z)
        {
            for (int x = 0; x <= segments; ++x)
            {
                const float worldX =
                    startX + static_cast<float>(x) * spacing;

                const float worldZ =
                    startZ + static_cast<float>(z) * spacing;

                m_vertices.push_back(
                    CreateVertex(generator, worldX, worldZ)
                );
            }
        }

        for (int z = 0; z < segments; ++z)
        {
            for (int x = 0; x < segments; ++x)
            {
                const unsigned int topLeft =
                    static_cast<unsigned int>(
                        z * (segments + 1) + x
                    );

                const unsigned int topRight = topLeft + 1;

                const unsigned int bottomLeft =
                    topLeft + static_cast<unsigned int>(segments + 1);

                const unsigned int bottomRight = bottomLeft + 1;

                m_indices.push_back(topLeft);
                m_indices.push_back(bottomLeft);
                m_indices.push_back(topRight);

                m_indices.push_back(topRight);
                m_indices.push_back(bottomLeft);
                m_indices.push_back(bottomRight);
            }
        }

        m_generated = true;
        return true;
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

    const std::vector<TerrainVertex>& TerrainChunk::GetVertices() const
    {
        return m_vertices;
    }

    const std::vector<unsigned int>& TerrainChunk::GetIndices() const
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
        float worldZ) const
    {
        TerrainVertex vertex{};

        const float sampleDistance =
            std::max(m_vertexSpacing * 0.5f, 0.1f);

        const float height = generator.GetHeight(
            worldX,
            worldZ
        );

        const float leftHeight = generator.GetHeight(
            worldX - sampleDistance,
            worldZ
        );

        const float rightHeight = generator.GetHeight(
            worldX + sampleDistance,
            worldZ
        );

        const float backHeight = generator.GetHeight(
            worldX,
            worldZ - sampleDistance
        );

        const float frontHeight = generator.GetHeight(
            worldX,
            worldZ + sampleDistance
        );

        float normalX = leftHeight - rightHeight;
        float normalY = sampleDistance * 2.0f;
        float normalZ = backHeight - frontHeight;

        const float normalLength = Length(
            normalX,
            normalY,
            normalZ
        );

        if (normalLength > 0.0001f)
        {
            normalX /= normalLength;
            normalY /= normalLength;
            normalZ /= normalLength;
        }
        else
        {
            normalX = 0.0f;
            normalY = 1.0f;
            normalZ = 0.0f;
        }

        vertex.position[0] = worldX;
        vertex.position[1] = height;
        vertex.position[2] = worldZ;

        vertex.normal[0] = normalX;
        vertex.normal[1] = normalY;
        vertex.normal[2] = normalZ;

        CalculateVertexColor(vertex, height);

        const float slope = 1.0f - std::clamp(normalY, 0.0f, 1.0f);

        const Color terrainColor = {
            vertex.color[0],
            vertex.color[1],
            vertex.color[2]
        };

        const Color exposedRock = {
            0.40f, 0.39f, 0.37f
        };

        const Color darkRock = {
            0.29f, 0.28f, 0.27f
        };

        const float rockBlend = SmoothBlend(
            slope,
            0.12f,
            0.58f
        ) * 0.88f;

        const Color slopeColor = LerpColor(
            exposedRock,
            darkRock,
            SmoothBlend(slope, 0.55f, 0.90f) * 0.55f
        );

        const Color finalColor = LerpColor(
            terrainColor,
            slopeColor,
            rockBlend
        );

        vertex.color[0] = finalColor.r;
        vertex.color[1] = finalColor.g;
        vertex.color[2] = finalColor.b;

        return vertex;
    }

    void TerrainChunk::CalculateVertexColor(
        TerrainVertex& vertex,
        float height) const
    {
        const Color deepEarth = {
            0.19f, 0.14f, 0.10f
        };

        const Color canyonSoil = {
            0.43f, 0.27f, 0.16f
        };

        const Color dryGrass = {
            0.48f, 0.43f, 0.23f
        };

        const Color grass = {
            0.28f, 0.48f, 0.19f
        };

        const Color richGrass = {
            0.22f, 0.40f, 0.16f
        };

        const Color highlandGrass = {
            0.39f, 0.45f, 0.27f
        };

        const Color rock = {
            0.42f, 0.40f, 0.37f
        };

        const Color highRock = {
            0.57f, 0.55f, 0.51f
        };

        const Color snow = {
            0.84f, 0.87f, 0.90f
        };

        Color result;

        if (height < -12.0f)
        {
            result = LerpColor(
                deepEarth,
                canyonSoil,
                SmoothBlend(height, -35.0f, -12.0f)
            );
        }
        else if (height < -2.0f)
        {
            result = LerpColor(
                canyonSoil,
                dryGrass,
                SmoothBlend(height, -12.0f, -2.0f)
            );
        }
        else if (height < 10.0f)
        {
            result = LerpColor(
                dryGrass,
                grass,
                SmoothBlend(height, -2.0f, 10.0f)
            );
        }
        else if (height < 24.0f)
        {
            result = LerpColor(
                grass,
                richGrass,
                SmoothBlend(height, 10.0f, 24.0f)
            );
        }
        else if (height < 38.0f)
        {
            result = LerpColor(
                richGrass,
                highlandGrass,
                SmoothBlend(height, 24.0f, 38.0f)
            );
        }
        else if (height < 55.0f)
        {
            result = LerpColor(
                highlandGrass,
                rock,
                SmoothBlend(height, 38.0f, 55.0f)
            );
        }
        else if (height < 75.0f)
        {
            result = LerpColor(
                rock,
                highRock,
                SmoothBlend(height, 55.0f, 75.0f)
            );
        }
        else
        {
            result = LerpColor(
                highRock,
                snow,
                SmoothBlend(height, 75.0f, 105.0f)
            );
        }

        vertex.color[0] = result.r;
        vertex.color[1] = result.g;
        vertex.color[2] = result.b;
    }
}