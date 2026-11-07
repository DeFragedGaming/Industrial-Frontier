#include "industrial_frontier/player/PlayerController.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float DegreesToRadians = Pi / 180.0f;
        constexpr float CollisionSkin = 0.025f;
        constexpr float HorizontalSubstep = 0.16f;
        constexpr float VerticalSubstep = 0.065f;
        constexpr float GroundProbeStep = 0.05f;
        constexpr float CameraMeshSpacing = 2.0f;
        constexpr float CameraMeshMinimumY = -96.0f;
        constexpr float CameraMeshMaximumY = 160.0f;
        constexpr float CameraSurfaceClearance = 0.30f;
        constexpr float CameraMaximumDrop = 1.15f;
        constexpr float CameraMaximumRise = 0.55f;
        constexpr float CameraProbeExtent = 1.75f;

        struct CollisionRing
        {
            float height;
            float radiusFactor;
        };

        constexpr std::array<CollisionRing, 6> CollisionRings = {{
            {0.10f, 0.50f},
            {0.36f, 0.96f},
            {0.93f, 0.96f},
            {1.50f, 0.86f},
            {1.60f, 0.70f},
            {1.78f, 0.48f}
        }};

        struct ProbeVec3
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        struct ProbeSample
        {
            ProbeVec3 position;
            float density = 0.0f;
        };

        struct ProbeTriangle
        {
            ProbeVec3 a;
            ProbeVec3 b;
            ProbeVec3 c;
        };

        constexpr int ProbeCubeCorners[8][3] =
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

        constexpr int ProbeTetrahedra[6][4] =
        {
            {0, 5, 1, 6},
            {0, 1, 2, 6},
            {0, 2, 3, 6},
            {0, 3, 7, 6},
            {0, 7, 4, 6},
            {0, 4, 5, 6}
        };

        ProbeVec3 Subtract(const ProbeVec3& a, const ProbeVec3& b)
        {
            return {a.x - b.x, a.y - b.y, a.z - b.z};
        }

        ProbeVec3 Add(const ProbeVec3& a, const ProbeVec3& b)
        {
            return {a.x + b.x, a.y + b.y, a.z + b.z};
        }

        ProbeVec3 Scale(const ProbeVec3& v, float amount)
        {
            return {v.x * amount, v.y * amount, v.z * amount};
        }

        float Dot(const ProbeVec3& a, const ProbeVec3& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        ProbeVec3 Cross(const ProbeVec3& a, const ProbeVec3& b)
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        float SegmentDistanceSquared(
            const ProbeVec3& p,
            const ProbeVec3& a,
            const ProbeVec3& b)
        {
            const ProbeVec3 edge = Subtract(b, a);
            const float len2 = Dot(edge, edge);
            const float t = len2 > 0.000001f
                ? std::clamp(Dot(Subtract(p, a), edge) / len2, 0.0f, 1.0f)
                : 0.0f;
            const ProbeVec3 delta = Subtract(p, Add(a, Scale(edge, t)));
            return Dot(delta, delta);
        }

        float TriangleDistanceSquared(
            const ProbeVec3& p,
            const ProbeTriangle& t)
        {
            const ProbeVec3 ab = Subtract(t.b, t.a);
            const ProbeVec3 ac = Subtract(t.c, t.a);
            const ProbeVec3 normal = Cross(ab, ac);
            const float normalLengthSquared = Dot(normal, normal);

            if (normalLengthSquared < 0.000000001f)
            {
                return std::min({
                    SegmentDistanceSquared(p, t.a, t.b),
                    SegmentDistanceSquared(p, t.b, t.c),
                    SegmentDistanceSquared(p, t.c, t.a)
                });
            }

            const ProbeVec3 ap = Subtract(p, t.a);
            const float d1 = Dot(ab, ap);
            const float d2 = Dot(ac, ap);
            if (d1 <= 0.0f && d2 <= 0.0f)
            {
                return Dot(ap, ap);
            }

            const ProbeVec3 bp = Subtract(p, t.b);
            const float d3 = Dot(ab, bp);
            const float d4 = Dot(ac, bp);
            if (d3 >= 0.0f && d4 <= d3)
            {
                return Dot(bp, bp);
            }

            const float vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
            {
                const float v = d1 / (d1 - d3);
                const ProbeVec3 difference =
                    Subtract(p, Add(t.a, Scale(ab, v)));
                return Dot(difference, difference);
            }

            const ProbeVec3 cp = Subtract(p, t.c);
            const float d5 = Dot(ab, cp);
            const float d6 = Dot(ac, cp);
            if (d6 >= 0.0f && d5 <= d6)
            {
                return Dot(cp, cp);
            }

            const float vb = d5 * d2 - d1 * d6;
            if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
            {
                const float w = d2 / (d2 - d6);
                const ProbeVec3 difference =
                    Subtract(p, Add(t.a, Scale(ac, w)));
                return Dot(difference, difference);
            }

            const float va = d3 * d6 - d5 * d4;
            if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f)
            {
                const ProbeVec3 cb = Subtract(t.c, t.b);
                const float w = (d4 - d3) /
                    ((d4 - d3) + (d5 - d6));
                const ProbeVec3 difference =
                    Subtract(p, Add(t.b, Scale(cb, w)));
                return Dot(difference, difference);
            }

            const float distance = Dot(ap, normal);
            return distance * distance / normalLengthSquared;
        }

        ProbeVec3 Interpolate(const ProbeSample& a, const ProbeSample& b)
        {
            const float difference = a.density - b.density;
            const float t = std::abs(difference) > 0.000001f
                ? std::clamp(a.density / difference, 0.0f, 1.0f)
                : 0.5f;
            return Add(a.position, Scale(Subtract(b.position, a.position), t));
        }

        class CameraSurfaceProbe
        {
        public:
            CameraSurfaceProbe(
                const VoxelTerrain& terrain,
                float cameraX,
                float cameraY,
                float cameraZ)
                : m_terrain(terrain),
                  m_chunkSize(terrain.GetChunkSize())
            {
                m_horizontalSteps = std::max(
                    1,
                    static_cast<int>(std::ceil(m_chunkSize / CameraMeshSpacing))
                );
                m_horizontalSpacing = m_chunkSize /
                    static_cast<float>(m_horizontalSteps);
                m_verticalSteps = std::max(
                    1,
                    static_cast<int>(std::ceil(
                        (CameraMeshMaximumY - CameraMeshMinimumY) /
                        CameraMeshSpacing))
                );
                m_verticalSpacing =
                    (CameraMeshMaximumY - CameraMeshMinimumY) /
                    static_cast<float>(m_verticalSteps);

                m_minX = HorizontalIndex(cameraX - CameraProbeExtent);
                m_maxX = HorizontalIndex(cameraX + CameraProbeExtent);
                m_minZ = HorizontalIndex(cameraZ - CameraProbeExtent);
                m_maxZ = HorizontalIndex(cameraZ + CameraProbeExtent);
                m_minY = VerticalIndex(cameraY - CameraMaximumDrop - 0.5f);
                m_maxY = VerticalIndex(cameraY + CameraMaximumRise + 0.5f);

                m_width = m_maxX - m_minX + 2;
                m_height = m_maxY - m_minY + 2;
                m_depth = m_maxZ - m_minZ + 2;

                m_samples.resize(static_cast<std::size_t>(m_width) *
                    m_height * m_depth);

                for (int iy = 0; iy < m_height; ++iy)
                {
                    const float worldY = CameraMeshMinimumY +
                        static_cast<float>(m_minY + iy) * m_verticalSpacing;

                    for (int iz = 0; iz < m_depth; ++iz)
                    {
                        const float worldZ = HorizontalPosition(m_minZ + iz);

                        for (int ix = 0; ix < m_width; ++ix)
                        {
                            const float worldX = HorizontalPosition(m_minX + ix);
                            ProbeSample& sample = m_samples[Index(ix, iy, iz)];
                            sample.position = {worldX, worldY, worldZ};
                            sample.density = terrain.SampleDensity(
                                worldX, worldY, worldZ);
                        }
                    }
                }

                BuildTriangles();
            }

            bool IsClear(float x, float y, float z) const
            {
                constexpr float DensityRadius = 0.19f;
                constexpr float Diagonal = 0.57735026919f;
                constexpr std::array<std::array<float, 3>, 14> Directions = {{
                    {1, 0, 0}, {-1, 0, 0},
                    {0, 1, 0}, {0, -1, 0},
                    {0, 0, 1}, {0, 0, -1},
                    { Diagonal,  Diagonal,  Diagonal},
                    { Diagonal,  Diagonal, -Diagonal},
                    { Diagonal, -Diagonal,  Diagonal},
                    { Diagonal, -Diagonal, -Diagonal},
                    {-Diagonal,  Diagonal,  Diagonal},
                    {-Diagonal,  Diagonal, -Diagonal},
                    {-Diagonal, -Diagonal,  Diagonal},
                    {-Diagonal, -Diagonal, -Diagonal}
                }};

                if (m_terrain.SampleDensity(x, y, z) >= -0.02f)
                {
                    return false;
                }

                for (const auto& direction : Directions)
                {
                    if (m_terrain.SampleDensity(
                            x + direction[0] * DensityRadius,
                            y + direction[1] * DensityRadius,
                            z + direction[2] * DensityRadius) >= -0.02f)
                    {
                        return false;
                    }
                }

                const ProbeVec3 p{x, y, z};
                const float interpolatedDensity = MeshDensity(p);
                if (interpolatedDensity >= -0.02f)
                {
                    return false;
                }

                const float minimumDistanceSquared =
                    CameraSurfaceClearance * CameraSurfaceClearance;

                for (const ProbeTriangle& triangle : m_triangles)
                {
                    if (TriangleDistanceSquared(p, triangle) <
                        minimumDistanceSquared)
                    {
                        return false;
                    }
                }

                return true;
            }

        private:
            const VoxelTerrain& m_terrain;
            float m_chunkSize = 32.0f;
            float m_horizontalSpacing = 2.0f;
            float m_verticalSpacing = 2.0f;
            int m_horizontalSteps = 16;
            int m_verticalSteps = 128;
            int m_minX = 0;
            int m_maxX = 0;
            int m_minY = 0;
            int m_maxY = 0;
            int m_minZ = 0;
            int m_maxZ = 0;
            int m_width = 0;
            int m_height = 0;
            int m_depth = 0;
            std::vector<ProbeSample> m_samples;
            std::vector<ProbeTriangle> m_triangles;

            int HorizontalIndex(float p) const
            {
                const int chunk = static_cast<int>(std::floor(p / m_chunkSize));
                const float local = p - static_cast<float>(chunk) * m_chunkSize;
                const int index = std::clamp(
                    static_cast<int>(std::floor(local / m_horizontalSpacing)),
                    0, m_horizontalSteps - 1);
                return chunk * m_horizontalSteps + index;
            }

            float HorizontalPosition(int index) const
            {
                int chunk = index / m_horizontalSteps;
                int local = index % m_horizontalSteps;
                if (local < 0)
                {
                    local += m_horizontalSteps;
                    --chunk;
                }
                return static_cast<float>(chunk) * m_chunkSize +
                    static_cast<float>(local) * m_horizontalSpacing;
            }

            int VerticalIndex(float p) const
            {
                return static_cast<int>(std::floor(
                    (p - CameraMeshMinimumY) / m_verticalSpacing));
            }

            std::size_t Index(int x, int y, int z) const
            {
                return (static_cast<std::size_t>(y) * m_depth + z) *
                    m_width + x;
            }

            void AddTriangle(
                const ProbeVec3& a,
                const ProbeVec3& b,
                const ProbeVec3& c)
            {
                const ProbeVec3 normal = Cross(
                    Subtract(b, a), Subtract(c, a));
                if (Dot(normal, normal) > 0.00000001f)
                {
                    m_triangles.push_back({a, b, c});
                }
            }

            void Polygonize(const std::array<ProbeSample, 8>& cube)
            {
                for (const auto& tetra : ProbeTetrahedra)
                {
                    int inside[4]{};
                    int outside[4]{};
                    int insideCount = 0;
                    int outsideCount = 0;

                    for (int i = 0; i < 4; ++i)
                    {
                        const int idx = tetra[i];
                        if (cube[idx].density > 0.0f)
                        {
                            inside[insideCount++] = idx;
                        }
                        else
                        {
                            outside[outsideCount++] = idx;
                        }
                    }

                    if (insideCount == 0 || insideCount == 4)
                    {
                        continue;
                    }

                    if (insideCount == 1)
                    {
                        AddTriangle(
                            Interpolate(cube[inside[0]], cube[outside[0]]),
                            Interpolate(cube[inside[0]], cube[outside[1]]),
                            Interpolate(cube[inside[0]], cube[outside[2]]));
                    }
                    else if (insideCount == 3)
                    {
                        AddTriangle(
                            Interpolate(cube[outside[0]], cube[inside[0]]),
                            Interpolate(cube[outside[0]], cube[inside[1]]),
                            Interpolate(cube[outside[0]], cube[inside[2]]));
                    }
                    else
                    {
                        const ProbeVec3 a = Interpolate(
                            cube[inside[0]], cube[outside[0]]);
                        const ProbeVec3 b = Interpolate(
                            cube[inside[0]], cube[outside[1]]);
                        const ProbeVec3 c = Interpolate(
                            cube[inside[1]], cube[outside[0]]);
                        const ProbeVec3 d = Interpolate(
                            cube[inside[1]], cube[outside[1]]);
                        AddTriangle(a, b, c);
                        AddTriangle(b, d, c);
                    }
                }
            }

            void BuildTriangles()
            {
                for (int y = 0; y < m_height - 1; ++y)
                {
                    for (int z = 0; z < m_depth - 1; ++z)
                    {
                        for (int x = 0; x < m_width - 1; ++x)
                        {
                            std::array<ProbeSample, 8> cube{};
                            int inside = 0;

                            for (int i = 0; i < 8; ++i)
                            {
                                cube[i] = m_samples[Index(
                                    x + ProbeCubeCorners[i][0],
                                    y + ProbeCubeCorners[i][1],
                                    z + ProbeCubeCorners[i][2])];
                                inside += cube[i].density > 0.0f ? 1 : 0;
                            }

                            if (inside > 0 && inside < 8)
                            {
                                Polygonize(cube);
                            }
                        }
                    }
                }
            }

            float MeshDensity(const ProbeVec3& p) const
            {
                const int ix = HorizontalIndex(p.x) - m_minX;
                const int iy = VerticalIndex(p.y) - m_minY;
                const int iz = HorizontalIndex(p.z) - m_minZ;

                if (ix < 0 || ix >= m_width - 1 ||
                    iy < 0 || iy >= m_height - 1 ||
                    iz < 0 || iz >= m_depth - 1)
                {
                    return std::numeric_limits<float>::infinity();
                }

                std::array<ProbeSample, 8> cube{};
                for (int i = 0; i < 8; ++i)
                {
                    cube[i] = m_samples[Index(
                        ix + ProbeCubeCorners[i][0],
                        iy + ProbeCubeCorners[i][1],
                        iz + ProbeCubeCorners[i][2])];
                }

                for (const auto& tetra : ProbeTetrahedra)
                {
                    const ProbeSample& a = cube[tetra[0]];
                    const ProbeSample& b = cube[tetra[1]];
                    const ProbeSample& c = cube[tetra[2]];
                    const ProbeSample& d = cube[tetra[3]];
                    const ProbeVec3 ab = Subtract(a.position, d.position);
                    const ProbeVec3 bb = Subtract(b.position, d.position);
                    const ProbeVec3 cb = Subtract(c.position, d.position);
                    const ProbeVec3 point = Subtract(p, d.position);
                    const float determinant = Dot(ab, Cross(bb, cb));
                    if (std::abs(determinant) < 0.000001f)
                    {
                        continue;
                    }

                    const float wa = Dot(point, Cross(bb, cb)) / determinant;
                    const float wb = Dot(ab, Cross(point, cb)) / determinant;
                    const float wc = Dot(ab, Cross(bb, point)) / determinant;
                    const float wd = 1.0f - wa - wb - wc;

                    if (wa >= -0.0001f && wb >= -0.0001f &&
                        wc >= -0.0001f && wd >= -0.0001f)
                    {
                        return wa * a.density + wb * b.density +
                            wc * c.density + wd * d.density;
                    }
                }

                return std::numeric_limits<float>::infinity();
            }
        };

    }

    void PlayerController::Initialize(const TerrainGenerator& terrain)
    {
        m_x = 0.0f;
        m_z = 0.0f;

        const float radius = m_collisionRadius;
        float height = terrain.GetHeight(m_x, m_z);
        height = std::max(height, terrain.GetHeight(m_x + radius, m_z));
        height = std::max(height, terrain.GetHeight(m_x - radius, m_z));
        height = std::max(height, terrain.GetHeight(m_x, m_z + radius));
        height = std::max(height, terrain.GetHeight(m_x, m_z - radius));

        m_y = height + m_groundOffset + 0.06f;
        m_verticalVelocity = 0.0f;
        m_grounded = true;
        m_yaw = -90.0f;
        m_pitch = 0.0f;
        m_initialized = true;
    }

    bool PlayerController::IsCapsuleClear(
        float x,
        float y,
        float z,
        const VoxelTerrain& terrain) const
    {
        for (std::size_t ringIndex = 0;
             ringIndex < CollisionRings.size();
             ++ringIndex)
        {
            const CollisionRing ring = CollisionRings[ringIndex];
            const float sampleY = y + ring.height;
            const float radius = m_collisionRadius * ring.radiusFactor;

            if (terrain.SampleDensity(x, sampleY, z) > CollisionSkin ||
                terrain.SampleDensity(x + radius, sampleY, z) > CollisionSkin ||
                terrain.SampleDensity(x - radius, sampleY, z) > CollisionSkin ||
                terrain.SampleDensity(x, sampleY, z + radius) > CollisionSkin ||
                terrain.SampleDensity(x, sampleY, z - radius) > CollisionSkin)
            {
                return false;
            }

            if (ringIndex == 1 || ringIndex == 2 || ringIndex == 4)
            {
                const float diagonal = radius * 0.70710678118f;

                if (terrain.SampleDensity(x + diagonal, sampleY, z + diagonal) > CollisionSkin ||
                    terrain.SampleDensity(x + diagonal, sampleY, z - diagonal) > CollisionSkin ||
                    terrain.SampleDensity(x - diagonal, sampleY, z + diagonal) > CollisionSkin ||
                    terrain.SampleDensity(x - diagonal, sampleY, z - diagonal) > CollisionSkin)
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool PlayerController::TryHorizontalStep(
        float deltaX,
        float deltaZ,
        const VoxelTerrain& terrain)
    {
        if (deltaX == 0.0f && deltaZ == 0.0f)
        {
            return true;
        }

        const float candidateX = m_x + deltaX;
        const float candidateZ = m_z + deltaZ;

        if (IsCapsuleClear(candidateX, m_y, candidateZ, terrain))
        {
            m_x = candidateX;
            m_z = candidateZ;
            return true;
        }

        if (!m_grounded)
        {
            return false;
        }

        for (float rise = 0.10f;
             rise <= m_maxStepHeight + 0.0001f;
             rise += 0.10f)
        {
            if (!IsCapsuleClear(m_x, m_y + rise, m_z, terrain))
            {
                continue;
            }

            if (IsCapsuleClear(candidateX, m_y + rise, candidateZ, terrain))
            {
                m_y += rise;
                m_x = candidateX;
                m_z = candidateZ;
                return true;
            }
        }

        if (IsCapsuleClear(m_x, m_y + m_maxStepHeight, m_z, terrain) &&
            IsCapsuleClear(candidateX, m_y + m_maxStepHeight, candidateZ, terrain))
        {
            m_y += m_maxStepHeight;
            m_x = candidateX;
            m_z = candidateZ;
            return true;
        }

        return false;
    }

    bool PlayerController::SnapToGround(
        float maximumDrop,
        const VoxelTerrain& terrain)
    {
        const float startY = m_y;

        for (float distance = GroundProbeStep;
             distance <= maximumDrop + GroundProbeStep * 0.5f;
             distance += GroundProbeStep)
        {
            const float candidateY = startY - distance;

            if (!IsCapsuleClear(m_x, candidateY, m_z, terrain))
            {
                m_grounded = true;
                m_verticalVelocity = 0.0f;
                return true;
            }

            m_y = candidateY;
        }

        m_grounded = false;
        return false;
    }

    void PlayerController::ApplyVerticalMovement(
        float deltaTime,
        const VoxelTerrain& terrain)
    {
        m_verticalVelocity += m_gravity * deltaTime;

        const float deltaY = m_verticalVelocity * deltaTime;

        const int steps = std::max(
            1,
            static_cast<int>(
                std::ceil(std::abs(deltaY) / VerticalSubstep)
            )
        );

        const float stepY = deltaY / static_cast<float>(steps);

        for (int step = 0; step < steps; ++step)
        {
            if (IsCapsuleClear(m_x, m_y + stepY, m_z, terrain))
            {
                m_y += stepY;
                continue;
            }

            if (stepY <= 0.0f)
            {
                m_grounded = true;
            }

            m_verticalVelocity = 0.0f;
            return;
        }

        m_grounded = false;

        if (m_verticalVelocity <= 0.0f)
        {
            SnapToGround(0.09f, terrain);
        }
    }

    void PlayerController::Update(
        float deltaTime,
        const PlayerInput& input,
        const VoxelTerrain& terrain)
    {
        if (!m_initialized || deltaTime <= 0.0f)
        {
            return;
        }

        deltaTime = std::min(deltaTime, 0.05f);

        if (!IsCapsuleClear(m_x, m_y, m_z, terrain))
        {
            for (float rise = 0.05f; rise <= 1.50f; rise += 0.05f)
            {
                if (IsCapsuleClear(m_x, m_y + rise, m_z, terrain))
                {
                    m_y += rise;
                    m_grounded = false;
                    m_verticalVelocity = 0.0f;
                    break;
                }
            }
        }

        const float yaw = m_yaw * DegreesToRadians;
        const float forwardX = std::cos(yaw);
        const float forwardZ = std::sin(yaw);
        const float rightX = -forwardZ;
        const float rightZ = forwardX;

        float moveX = forwardX * input.forward + rightX * input.right;
        float moveZ = forwardZ * input.forward + rightZ * input.right;

        const float length = std::sqrt(moveX * moveX + moveZ * moveZ);

        if (length > 1.0f)
        {
            moveX /= length;
            moveZ /= length;
        }

        const float speed = input.sprint ? m_sprintSpeed : m_walkSpeed;
        const float moveDistance = speed * deltaTime;
        const float totalDistance = std::sqrt(moveX * moveX + moveZ * moveZ) * moveDistance;

        const int steps = std::max(
            1,
            static_cast<int>(std::ceil(totalDistance / HorizontalSubstep))
        );

        const float stepX = moveX * moveDistance / static_cast<float>(steps);
        const float stepZ = moveZ * moveDistance / static_cast<float>(steps);

        for (int step = 0; step < steps; ++step)
        {
            if (TryHorizontalStep(stepX, stepZ, terrain))
            {
                continue;
            }

            TryHorizontalStep(stepX, 0.0f, terrain);
            TryHorizontalStep(0.0f, stepZ, terrain);
        }

        if (input.jumpPressed && m_grounded)
        {
            m_verticalVelocity = m_jumpVelocity;
            m_grounded = false;
        }

        if (m_grounded)
        {
            SnapToGround(0.45f, terrain);
        }

        if (!m_grounded)
        {
            ApplyVerticalMovement(deltaTime, terrain);
        }
    }

    void PlayerController::Rotate(float deltaX, float deltaY)
    {
        if (!m_initialized)
        {
            return;
        }

        m_yaw += deltaX * m_mouseSensitivity;
        m_pitch -= deltaY * m_mouseSensitivity;
        m_pitch = std::clamp(m_pitch, -85.0f, 85.0f);
    }

    void PlayerController::SetPosition(float x, float y, float z)
    {
        m_x = x;
        m_y = y;
        m_z = z;
        m_verticalVelocity = 0.0f;
        m_grounded = false;
    }

    void PlayerController::SetViewRotation(float yaw, float pitch)
    {
        if (!std::isfinite(yaw) || !std::isfinite(pitch))
            return;
        m_yaw = yaw;
        m_pitch = std::clamp(pitch, -85.0f, 85.0f);
    }

    float PlayerController::GetX() const
    {
        return m_x;
    }

    float PlayerController::GetY() const
    {
        return m_y;
    }

    float PlayerController::GetZ() const
    {
        return m_z;
    }

    float PlayerController::GetYaw() const
    {
        return m_yaw;
    }

    float PlayerController::GetPitch() const
    {
        return m_pitch;
    }

    float PlayerController::GetEyeHeight() const
    {
        return m_eyeHeight;
    }

    void PlayerController::GetForward(float& x, float& y, float& z) const
    {
        const float yaw = m_yaw * DegreesToRadians;
        const float pitch = m_pitch * DegreesToRadians;
        const float horizontal = std::cos(pitch);

        x = std::cos(yaw) * horizontal;
        y = std::sin(pitch);
        z = std::sin(yaw) * horizontal;
    }

    void PlayerController::GetFirstPersonCameraPosition(
        float& x,
        float& y,
        float& z) const
    {
        x = m_x;
        y = m_y + m_eyeHeight;
        z = m_z;
    }

    bool PlayerController::GetCollisionSafeCameraPosition(
        const VoxelTerrain& terrain,
        float& x,
        float& y,
        float& z) const
    {
        GetFirstPersonCameraPosition(x, y, z);

        if (!m_initialized)
        {
            return false;
        }

        const float baseX = x;
        const float baseY = y;
        const float baseZ = z;
        const CameraSurfaceProbe surface(terrain, baseX, baseY, baseZ);

        const auto accept = [&](float candidateX,
                                float candidateY,
                                float candidateZ) -> bool
        {
            if (!surface.IsClear(candidateX, candidateY, candidateZ))
            {
                return false;
            }

            x = candidateX;
            y = candidateY;
            z = candidateZ;
            return true;
        };

        if (accept(baseX, baseY, baseZ))
        {
            return true;
        }

        for (float shift = 0.08f; shift <= CameraMaximumDrop;
             shift += 0.08f)
        {
            if (accept(baseX, baseY - shift, baseZ))
            {
                return true;
            }
        }

        for (float shift = 0.08f; shift <= CameraMaximumRise;
             shift += 0.08f)
        {
            if (accept(baseX, baseY + shift, baseZ))
            {
                return true;
            }
        }

        constexpr float Direction = 0.70710678118f;
        constexpr std::array<std::array<float, 2>, 8> Directions = {{
            { 1.0f,  0.0f}, {-1.0f,  0.0f},
            { 0.0f,  1.0f}, { 0.0f, -1.0f},
            { Direction,  Direction},
            { Direction, -Direction},
            {-Direction,  Direction},
            {-Direction, -Direction}
        }};

        constexpr std::array<float, 7> Heights = {{
            0.0f, -0.16f, 0.16f, -0.36f, 0.36f, -0.64f, -0.96f
        }};

        for (float distance : {0.14f, 0.28f, 0.42f, 0.56f})
        {
            for (float offsetY : Heights)
            {
                for (const auto& direction : Directions)
                {
                    if (accept(baseX + direction[0] * distance,
                            baseY + offsetY,
                            baseZ + direction[1] * distance))
                    {
                        return true;
                    }
                }
            }
        }

        x = baseX;
        y = baseY;
        z = baseZ;
        return false;
    }

    bool PlayerController::IsGrounded() const
    {
        return m_grounded;
    }

    bool PlayerController::IsInitialized() const
    {
        return m_initialized;
    }
}
