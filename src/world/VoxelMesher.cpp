#include "industrial_frontier/world/VoxelMesher.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        struct Vec3
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        struct Sample
        {
            Vec3 position;
            float density = 0.0f;
        };

        constexpr int CubeCorners[8][3] =
        {
            {0, 0, 0},
            {1, 0, 0},
            {1, 1, 0},
            {0, 1, 0},
            {0, 0, 1},
            {1, 0, 1},
            {1, 1, 1},
            {0, 1, 1}
        };

        constexpr int Tetrahedra[6][4] =
        {
            {0, 5, 1, 6},
            {0, 1, 2, 6},
            {0, 2, 3, 6},
            {0, 3, 7, 6},
            {0, 7, 4, 6},
            {0, 4, 5, 6}
        };

        Vec3 Subtract(const Vec3& a, const Vec3& b)
        {
            return {a.x - b.x, a.y - b.y, a.z - b.z};
        }

        Vec3 Cross(const Vec3& a, const Vec3& b)
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        float Dot(const Vec3& a, const Vec3& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        Vec3 Normalize(const Vec3& value)
        {
            const float length = std::sqrt(Dot(value, value));
            if (length < 0.000001f)
            {
                return {0.0f, 1.0f, 0.0f};
            }
            return {
                value.x / length,
                value.y / length,
                value.z / length
            };
        }

        Vec3 Interpolate(const Sample& a, const Sample& b)
        {
            const float denominator = a.density - b.density;
            float t = 0.5f;
            if (std::abs(denominator) > 0.000001f)
            {
                t = std::clamp(a.density / denominator, 0.0f, 1.0f);
            }
            return {
                a.position.x + (b.position.x - a.position.x) * t,
                a.position.y + (b.position.y - a.position.y) * t,
                a.position.z + (b.position.z - a.position.z) * t
            };
        }

        Vec3 CalculateNormal(
            const VoxelTerrain& terrain,
            const Vec3& position,
            float epsilon)
        {
            const float dx =
                terrain.SampleDensity(position.x + epsilon, position.y, position.z) -
                terrain.SampleDensity(position.x - epsilon, position.y, position.z);
            const float dy =
                terrain.SampleDensity(position.x, position.y + epsilon, position.z) -
                terrain.SampleDensity(position.x, position.y - epsilon, position.z);
            const float dz =
                terrain.SampleDensity(position.x, position.y, position.z + epsilon) -
                terrain.SampleDensity(position.x, position.y, position.z - epsilon);

            return Normalize({-dx, -dy, -dz});
        }

        float ColorVariation(const Vec3& position)
        {
            const float variation =
                0.5f * std::sin(position.x * 0.43f + position.z * 0.28f) +
                0.5f * std::sin(position.y * 0.57f - position.x * 0.19f);
            return std::clamp(1.0f + variation * 0.085f, 0.90f, 1.10f);
        }

        TerrainVertex CreateVertex(
            const VoxelTerrain& terrain,
            const Vec3& position,
            float epsilon)
        {
            TerrainVertex vertex{};
            const Vec3 normal = CalculateNormal(terrain, position, epsilon);

            vertex.position[0] = position.x;
            vertex.position[1] = position.y;
            vertex.position[2] = position.z;
            vertex.normal[0] = normal.x;
            vertex.normal[1] = normal.y;
            vertex.normal[2] = normal.z;

            const float surfaceHeight = terrain.GetGenerator().GetHeight(
                position.x, position.z
            );
            const float depth = surfaceHeight - position.y;

            float red = 0.43f;
            float green = 0.41f;
            float blue = 0.38f;

            if (depth < 0.7f && normal.y > 0.55f)
            {
                if (surfaceHeight > 45.0f)
                {
                    red = 0.75f;
                    green = 0.77f;
                    blue = 0.78f;
                }
                else if (surfaceHeight > 25.0f)
                {
                    red = 0.45f;
                    green = 0.46f;
                    blue = 0.40f;
                }
                else if (surfaceHeight < -10.0f)
                {
                    red = 0.43f;
                    green = 0.28f;
                    blue = 0.17f;
                }
                else
                {
                    red = 0.30f;
                    green = 0.45f;
                    blue = 0.23f;
                }
            }
            else if (depth > 8.0f)
            {
                red = 0.34f;
                green = 0.33f;
                blue = 0.32f;
            }
            else if (depth > 1.0f)
            {
                red = 0.46f;
                green = 0.38f;
                blue = 0.31f;
            }

            const float variation = ColorVariation(position);
            vertex.color[0] = std::clamp(red * variation, 0.0f, 1.0f);
            vertex.color[1] = std::clamp(green * variation, 0.0f, 1.0f);
            vertex.color[2] = std::clamp(blue * variation, 0.0f, 1.0f);
            return vertex;
        }

        void AddTriangle(
            const VoxelTerrain& terrain,
            VoxelMesh& mesh,
            Vec3 a,
            Vec3 b,
            Vec3 c,
            float epsilon)
        {
            const Vec3 edgeA = Subtract(b, a);
            const Vec3 edgeB = Subtract(c, a);
            const Vec3 faceNormal = Cross(edgeA, edgeB);
            if (Dot(faceNormal, faceNormal) < 0.00000001f)
            {
                return;
            }

            const Vec3 center = {
                (a.x + b.x + c.x) / 3.0f,
                (a.y + b.y + c.y) / 3.0f,
                (a.z + b.z + c.z) / 3.0f
            };

            const Vec3 expectedNormal = CalculateNormal(terrain, center, epsilon);
            if (Dot(faceNormal, expectedNormal) < 0.0f)
            {
                std::swap(b, c);
            }

            const unsigned int base =
                static_cast<unsigned int>(mesh.vertices.size());
            mesh.vertices.push_back(CreateVertex(terrain, a, epsilon));
            mesh.vertices.push_back(CreateVertex(terrain, b, epsilon));
            mesh.vertices.push_back(CreateVertex(terrain, c, epsilon));
            mesh.indices.push_back(base);
            mesh.indices.push_back(base + 1);
            mesh.indices.push_back(base + 2);
        }

        void PolygonizeTetrahedron(
            const VoxelTerrain& terrain,
            const std::array<Sample, 8>& cube,
            const int* tetrahedron,
            VoxelMesh& mesh,
            float epsilon)
        {
            int inside[4]{};
            int outside[4]{};
            int insideCount = 0;
            int outsideCount = 0;

            for (int i = 0; i < 4; ++i)
            {
                const int index = tetrahedron[i];
                if (cube[index].density > 0.0f)
                {
                    inside[insideCount++] = index;
                }
                else
                {
                    outside[outsideCount++] = index;
                }
            }

            if (insideCount == 0 || insideCount == 4)
            {
                return;
            }

            if (insideCount == 1)
            {
                AddTriangle(
                    terrain, mesh,
                    Interpolate(cube[inside[0]], cube[outside[0]]),
                    Interpolate(cube[inside[0]], cube[outside[1]]),
                    Interpolate(cube[inside[0]], cube[outside[2]]),
                    epsilon
                );
                return;
            }

            if (insideCount == 3)
            {
                AddTriangle(
                    terrain, mesh,
                    Interpolate(cube[outside[0]], cube[inside[0]]),
                    Interpolate(cube[outside[0]], cube[inside[1]]),
                    Interpolate(cube[outside[0]], cube[inside[2]]),
                    epsilon
                );
                return;
            }

            const Vec3 a = Interpolate(cube[inside[0]], cube[outside[0]]);
            const Vec3 b = Interpolate(cube[inside[0]], cube[outside[1]]);
            const Vec3 c = Interpolate(cube[inside[1]], cube[outside[0]]);
            const Vec3 d = Interpolate(cube[inside[1]], cube[outside[1]]);

            AddTriangle(terrain, mesh, a, b, c, epsilon);
            AddTriangle(terrain, mesh, b, d, c, epsilon);
        }
    }

    VoxelMesher::VoxelMesher(
        const VoxelMesherSettings& settings)
        : m_settings(settings)
    {
        m_settings.spacing = std::max(m_settings.spacing, 0.25f);
        if (m_settings.maximumY <= m_settings.minimumY)
        {
            m_settings.maximumY = m_settings.minimumY + m_settings.spacing;
        }
    }

    const VoxelMesherSettings& VoxelMesher::GetSettings() const
    {
        return m_settings;
    }

    bool VoxelMesher::GenerateChunk(
        const VoxelTerrain& terrain,
        int chunkX,
        int chunkZ,
        VoxelMesh& output) const
    {
        output.Clear();

        const float chunkSize = terrain.GetChunkSize();
        const int horizontalSteps = std::max(
            1, static_cast<int>(std::ceil(chunkSize / m_settings.spacing))
        );
        const int verticalSteps = std::max(
            1, static_cast<int>(std::ceil(
                (m_settings.maximumY - m_settings.minimumY) /
                m_settings.spacing
            ))
        );

        const float horizontalSpacing =
            chunkSize / static_cast<float>(horizontalSteps);
        const float verticalSpacing =
            (m_settings.maximumY - m_settings.minimumY) /
            static_cast<float>(verticalSteps);

        const float startX = static_cast<float>(chunkX) * chunkSize;
        const float startZ = static_cast<float>(chunkZ) * chunkSize;
        const float epsilon = std::max(
            std::min(horizontalSpacing, verticalSpacing) * 0.25f,
            0.05f
        );

        const int width = horizontalSteps + 1;
        const int depth = horizontalSteps + 1;
        const std::size_t layerSize =
            static_cast<std::size_t>(width) * static_cast<std::size_t>(depth);

        std::vector<Sample> samples(
            layerSize * static_cast<std::size_t>(verticalSteps + 1)
        );

        const auto index = [width, depth](int x, int y, int z) -> std::size_t
        {
            return (
                static_cast<std::size_t>(y) *
                static_cast<std::size_t>(depth) +
                static_cast<std::size_t>(z)
            ) * static_cast<std::size_t>(width) +
                static_cast<std::size_t>(x);
        };

        for (int y = 0; y <= verticalSteps; ++y)
        {
            const float worldY =
                m_settings.minimumY + static_cast<float>(y) * verticalSpacing;

            for (int z = 0; z <= horizontalSteps; ++z)
            {
                const float worldZ =
                    startZ + static_cast<float>(z) * horizontalSpacing;

                for (int x = 0; x <= horizontalSteps; ++x)
                {
                    const float worldX =
                        startX + static_cast<float>(x) * horizontalSpacing;
                    Sample& sample = samples[index(x, y, z)];
                    sample.position = {worldX, worldY, worldZ};
                    sample.density = terrain.SampleDensity(worldX, worldY, worldZ);
                }
            }
        }

        for (int y = 0; y < verticalSteps; ++y)
        {
            for (int z = 0; z < horizontalSteps; ++z)
            {
                for (int x = 0; x < horizontalSteps; ++x)
                {
                    std::array<Sample, 8> cube{};
                    bool hasSolid = false;
                    bool hasEmpty = false;

                    for (int corner = 0; corner < 8; ++corner)
                    {
                        const int sx = x + CubeCorners[corner][0];
                        const int sy = y + CubeCorners[corner][1];
                        const int sz = z + CubeCorners[corner][2];
                        cube[corner] = samples[index(sx, sy, sz)];
                        if (cube[corner].density > 0.0f)
                        {
                            hasSolid = true;
                        }
                        else
                        {
                            hasEmpty = true;
                        }
                    }

                    if (!hasSolid || !hasEmpty)
                    {
                        continue;
                    }

                    for (const auto& tetrahedron : Tetrahedra)
                    {
                        PolygonizeTetrahedron(
                            terrain, cube, tetrahedron, output, epsilon
                        );
                    }
                }
            }
        }

        return true;
    }
}
