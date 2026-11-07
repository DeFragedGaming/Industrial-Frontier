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

        float SmoothRange(float low, float high, float value)
        {
            const float t = std::clamp((value - low) / (high - low), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        Vec3 MixColor(const Vec3& a, const Vec3& b, float amount)
        {
            const float t = std::clamp(amount, 0.0f, 1.0f);
            return {
                a.x + (b.x - a.x) * t,
                a.y + (b.y - a.y) * t,
                a.z + (b.z - a.z) * t
            };
        }

        float ColorVariation(const Vec3& position)
        {
            const float variation =
                0.5f * std::sin(position.x * 0.43f + position.z * 0.28f) +
                0.5f * std::sin(position.y * 0.57f - position.x * 0.19f);
            return std::clamp(1.0f + variation * 0.055f, 0.94f, 1.06f);
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

            const float surfaceHeight = terrain.GetSurfaceHeight(
                position.x, position.z
            );
            const float originalHeight = terrain.GetGenerator().GetHeight(
                position.x, position.z
            );
            const float depth = surfaceHeight - position.y;

            const Vec3 grass = {0.30f, 0.45f, 0.23f};
            const Vec3 dirtClay = {0.46f, 0.38f, 0.31f};
            const Vec3 canyonClay = {0.43f, 0.28f, 0.17f};
            const Vec3 mountainRock = {0.45f, 0.46f, 0.40f};
            const Vec3 highMountainRock = {0.49f, 0.49f, 0.49f};
            const Vec3 deepStone = {0.34f, 0.33f, 0.32f};
            const Vec3 snow = {0.75f, 0.77f, 0.78f};

            const float earthVariation =
                std::sin(position.x * 0.19f + position.z * 0.23f) * 1.3f +
                std::sin(position.x * 0.07f - position.z * 0.11f) * 1.9f;

            const float deepRockAmount = SmoothRange(
                5.0f, 10.0f, depth + earthVariation * 0.75f
            );
            const float canyonAmount =
                1.0f - SmoothRange(-16.0f, -6.0f, originalHeight);

            const Vec3 shallowClay = MixColor(
                dirtClay, canyonClay, canyonAmount * 0.8f
            );
            const Vec3 underground = MixColor(
                shallowClay, deepStone, deepRockAmount
            );

            const Vec3 rock = MixColor(
                mountainRock,
                highMountainRock,
                SmoothRange(24.0f, 55.0f, originalHeight)
            );

            const float grassAmount =
                SmoothRange(0.40f, 0.76f, normal.y) *
                (1.0f - SmoothRange(18.0f, 34.0f, originalHeight)) *
                SmoothRange(-18.0f, -7.0f, originalHeight);

            Vec3 exposedSurface = MixColor(rock, grass, grassAmount);
            exposedSurface = MixColor(
                exposedSurface,
                canyonClay,
                canyonAmount * (1.0f - SmoothRange(0.26f, 0.68f, normal.y) * 0.35f)
            );

            const float snowAmount =
                SmoothRange(37.0f, 53.0f, originalHeight + earthVariation) *
                SmoothRange(0.18f, 0.66f, normal.y);

            exposedSurface = MixColor(exposedSurface, snow, snowAmount);

            const float rockRim = terrain.GetEntranceRockWeight(
                position.x, position.z) *
                (1.0f - SmoothRange(0.32f, 0.72f, normal.y));
            exposedSurface = MixColor(exposedSurface, rock, rockRim);

            const float surfaceAmount =
                1.0f - SmoothRange(0.45f, 1.65f, depth);

            Vec3 color = MixColor(underground, exposedSurface, surfaceAmount);
            color = MixColor(color, rock,
                rockRim * (1.0f - SmoothRange(6.0f, 12.0f, depth)));
            const float variation = ColorVariation(position);

            vertex.color[0] = std::clamp(color.x * variation, 0.0f, 1.0f);
            vertex.color[1] = std::clamp(color.y * variation, 0.0f, 1.0f);
            vertex.color[2] = std::clamp(color.z * variation, 0.0f, 1.0f);
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
