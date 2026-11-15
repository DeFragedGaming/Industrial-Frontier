#include "industrial_frontier/forest/ForestGenerator.hpp"
#include "industrial_frontier/resources/GeologyGenerator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float CellSize = 19.0f;
        constexpr int CrownSides = 7;

        struct Point
        {
            float x;
            float y;
            float z;
        };

        struct Color
        {
            float r;
            float g;
            float b;
        };

        std::uint32_t Mix(std::uint32_t n)
        {
            n ^= n >> 16;
            n *= 0x7feb352du;
            n ^= n >> 15;
            n *= 0x846ca68bu;
            return n ^ (n >> 16);
        }

        std::uint32_t Hash(int x, int z, std::uint32_t seed)
        {
            return Mix(seed ^ Mix(static_cast<std::uint32_t>(x) * 0x9e3779b9u)
                ^ Mix(static_cast<std::uint32_t>(z) * 0x85ebca6bu));
        }

        float Random01(std::uint32_t value)
        {
            return static_cast<float>(Mix(value) & 0xffffffu) / 16777215.0f;
        }

        float Length(const Point& a)
        {
            return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
        }

        void Triangle(VoxelMesh& mesh, const Point& a, const Point& b,
            const Point& c, const Color& color)
        {
            const Point u{b.x - a.x, b.y - a.y, b.z - a.z};
            const Point v{c.x - a.x, c.y - a.y, c.z - a.z};
            Point n{u.y * v.z - u.z * v.y,
                u.z * v.x - u.x * v.z,
                u.x * v.y - u.y * v.x};
            const float length = Length(n);
            if (length < 0.00001f)
            {
                return;
            }
            n.x /= length;
            n.y /= length;
            n.z /= length;

            const unsigned int first = static_cast<unsigned int>(mesh.vertices.size());
            for (const Point& p : {a, b, c})
            {
                TerrainVertex vertex{};
                vertex.position[0] = p.x;
                vertex.position[1] = p.y;
                vertex.position[2] = p.z;
                vertex.normal[0] = n.x;
                vertex.normal[1] = n.y;
                vertex.normal[2] = n.z;
                vertex.color[0] = color.r;
                vertex.color[1] = color.g;
                vertex.color[2] = color.b;
                mesh.vertices.push_back(vertex);
            }
            mesh.indices.push_back(first);
            mesh.indices.push_back(first + 1);
            mesh.indices.push_back(first + 2);
        }

        Color Shade(Color color, float multiplier)
        {
            return {std::clamp(color.r * multiplier, 0.0f, 1.0f),
                std::clamp(color.g * multiplier, 0.0f, 1.0f),
                std::clamp(color.b * multiplier, 0.0f, 1.0f)};
        }

        void Frustum(VoxelMesh& mesh, const ForestTree& tree, float bottom,
            float top, float lowerRadius, float upperRadius, int sides,
            Color color, float offsetX = 0.0f, float offsetZ = 0.0f)
        {
            for (int side = 0; side < sides; ++side)
            {
                const float t0 = tree.rotation + 2.0f * Pi * side / sides;
                const float t1 = tree.rotation + 2.0f * Pi * (side + 1) / sides;
                const Point a{tree.x + offsetX + std::cos(t0) * lowerRadius,
                    tree.y + bottom, tree.z + offsetZ + std::sin(t0) * lowerRadius};
                const Point b{tree.x + offsetX + std::cos(t1) * lowerRadius,
                    tree.y + bottom, tree.z + offsetZ + std::sin(t1) * lowerRadius};
                const Point c{tree.x + offsetX + std::cos(t1) * upperRadius,
                    tree.y + top, tree.z + offsetZ + std::sin(t1) * upperRadius};
                const Point d{tree.x + offsetX + std::cos(t0) * upperRadius,
                    tree.y + top, tree.z + offsetZ + std::sin(t0) * upperRadius};
                const float variation = 0.89f + 0.075f * static_cast<float>((side * 7 + sides) % 4);
                const Color face = Shade(color, variation);
                Triangle(mesh, a, c, b, face);
                Triangle(mesh, a, d, c, face);
            }
        }

        void Cone(VoxelMesh& mesh, const ForestTree& tree, float bottom,
            float top, float radius, Color color, float rotationOffset)
        {
            const Point tip{tree.x, tree.y + top, tree.z};
            for (int side = 0; side < CrownSides; ++side)
            {
                const float t0 = tree.rotation + rotationOffset +
                    2.0f * Pi * side / CrownSides;
                const float t1 = tree.rotation + rotationOffset +
                    2.0f * Pi * (side + 1) / CrownSides;
                const float waviness0 = 0.94f + 0.06f * static_cast<float>((side + 1) % 3);
                const float waviness1 = 0.94f + 0.06f * static_cast<float>((side + 2) % 3);
                const Point a{tree.x + std::cos(t0) * radius * waviness0,
                    tree.y + bottom, tree.z + std::sin(t0) * radius * waviness0};
                const Point b{tree.x + std::cos(t1) * radius * waviness1,
                    tree.y + bottom, tree.z + std::sin(t1) * radius * waviness1};
                Triangle(mesh, a, tip, b, Shade(color, 0.85f + 0.065f * (side % 4)));
            }
        }

        void Canopy(VoxelMesh& mesh, const ForestTree& tree, float xOffset,
            float zOffset, float centerHeight, float radius, float yScale, Color color)
        {
            constexpr int sides = 8;
            const Point top{tree.x + xOffset, tree.y + centerHeight + radius * yScale,
                tree.z + zOffset};
            const Point bottom{tree.x + xOffset,
                tree.y + centerHeight - radius * yScale * 0.85f,
                tree.z + zOffset};
            for (int side = 0; side < sides; ++side)
            {
                const float a0 = tree.rotation + 2.0f * Pi * side / sides;
                const float a1 = tree.rotation + 2.0f * Pi * (side + 1) / sides;
                const float r0 = radius * (0.92f + 0.06f * (side % 3));
                const float r1 = radius * (0.92f + 0.06f * ((side + 1) % 3));
                const Point p0{tree.x + xOffset + std::cos(a0) * r0,
                    tree.y + centerHeight, tree.z + zOffset + std::sin(a0) * r0};
                const Point p1{tree.x + xOffset + std::cos(a1) * r1,
                    tree.y + centerHeight, tree.z + zOffset + std::sin(a1) * r1};
                const float variation = 0.85f + 0.055f * (side % 4);
                Triangle(mesh, p0, top, p1, Shade(color, variation));
                Triangle(mesh, bottom, p0, p1, Shade(color, variation * 0.78f));
            }
        }

        void BuildTree(VoxelMesh& mesh, const ForestTree& tree)
        {
            const float h = tree.height;
            const float r = tree.crownRadius;
            const float trunkRadius = (tree.species == TreeSpecies::Pine ? 0.18f : 0.23f) * r;
            if (tree.species == TreeSpecies::Pine)
            {
                Frustum(mesh, tree, -0.25f, h * 0.81f,
                    trunkRadius * 1.25f, trunkRadius * 0.56f, 6,
                    {0.37f, 0.245f, 0.13f});
                const Color green{0.14f + 0.03f * (r - 2.3f), 0.31f, 0.16f};
                Cone(mesh, tree, h * 0.19f, h * 0.67f, r, green, 0.0f);
                Cone(mesh, tree, h * 0.39f, h * 0.85f, r * 0.81f,
                    Shade(green, 1.10f), 0.19f);
                Cone(mesh, tree, h * 0.60f, h * 1.06f, r * 0.59f,
                    Shade(green, 1.18f), 0.38f);
            }
            else if (tree.species == TreeSpecies::Broadleaf)
            {
                Frustum(mesh, tree, -0.28f, h * 0.72f,
                    trunkRadius * 1.1f, trunkRadius * 0.60f, 7,
                    {0.39f, 0.27f, 0.16f});
                const Color green{0.23f, 0.42f, 0.18f};
                Canopy(mesh, tree, -0.32f * r, 0.14f * r, h * 0.70f,
                    r * 0.78f, 0.78f, green);
                Canopy(mesh, tree, 0.38f * r, 0.20f * r, h * 0.77f,
                    r * 0.80f, 0.72f, Shade(green, 1.10f));
                Canopy(mesh, tree, 0.0f, -0.31f * r, h * 0.85f,
                    r * 0.84f, 0.75f, Shade(green, 0.96f));
            }
            else
            {
                Frustum(mesh, tree, -0.25f, h * 0.78f,
                    trunkRadius * 0.80f, trunkRadius * 0.36f, 6,
                    {0.79f, 0.78f, 0.69f});
                for (int i = 0; i < 3; ++i)
                {
                    const float level = h * (0.17f + 0.18f * i);
                    Frustum(mesh, tree, level, level + h * 0.022f,
                        trunkRadius * (0.81f - 0.13f * i),
                        trunkRadius * (0.79f - 0.13f * i), 6,
                        {0.20f, 0.20f, 0.18f});
                }
                const Color green{0.41f, 0.56f, 0.23f};
                Canopy(mesh, tree, -0.22f * r, 0.12f * r, h * 0.75f,
                    r * 0.80f, 0.95f, green);
                Canopy(mesh, tree, 0.22f * r, -0.10f * r, h * 0.84f,
                    r * 0.82f, 0.92f, Shade(green, 1.05f));
            }
        }
    }

    void ForestGenerator::GenerateTrees(const VoxelTerrain& terrain,
        int chunkX, int chunkZ, std::vector<ForestTree>& trees)
    {
        trees.clear();
        const float chunkSize = terrain.GetChunkSize();
        const float minX = static_cast<float>(chunkX) * chunkSize;
        const float minZ = static_cast<float>(chunkZ) * chunkSize;
        const float maxX = minX + chunkSize;
        const float maxZ = minZ + chunkSize;
        const int startX = static_cast<int>(std::floor(minX / CellSize));
        const int endX = static_cast<int>(std::floor((maxX - 0.0001f) / CellSize));
        const int startZ = static_cast<int>(std::floor(minZ / CellSize));
        const int endZ = static_cast<int>(std::floor((maxZ - 0.0001f) / CellSize));
        const std::uint32_t seed = static_cast<std::uint32_t>(terrain.GetGenerator().GetSettings().seed);
        const GeologyGenerator* geology = terrain.GetGeology();
        const std::vector<GeologicalDeposit> outcrops = geology && geology->IsReady()
            ? geology->FindSurfaceDeposits(minX - 10.0f, maxX + 10.0f,
                minZ - 10.0f, maxZ + 10.0f)
            : std::vector<GeologicalDeposit>{};

        for (int cellZ = startZ; cellZ <= endZ; ++cellZ)
        {
            for (int cellX = startX; cellX <= endX; ++cellX)
            {
                const std::uint32_t hash = Hash(cellX, cellZ, seed ^ 0x61c88647u);
                const float x = (cellX + 0.17f + 0.66f * Random01(hash + 11u)) * CellSize;
                const float z = (cellZ + 0.17f + 0.66f * Random01(hash + 19u)) * CellSize;
                if (x < minX || x >= maxX || z < minZ || z >= maxZ)
                {
                    continue;
                }

                const float region = Random01(Hash(
                    static_cast<int>(std::floor(x / 115.0f)),
                    static_cast<int>(std::floor(z / 115.0f)), seed ^ 0x1423329bu));
                const float chance = region < 0.18f ? 0.16f :
                    region < 0.36f ? 0.36f : 0.76f;
                if (Random01(hash + 29u) > chance)
                {
                    continue;
                }

                const float y = terrain.GetSurfaceHeight(x, z);
                if (!std::isfinite(y) || y < -14.0f || y > 37.0f)
                {
                    continue;
                }

                const float delta = 2.25f;
                const float dx = (terrain.GetSurfaceHeight(x + delta, z) -
                    terrain.GetSurfaceHeight(x - delta, z)) / (2.0f * delta);
                const float dz = (terrain.GetSurfaceHeight(x, z + delta) -
                    terrain.GetSurfaceHeight(x, z - delta)) / (2.0f * delta);
                const float normalY = 1.0f / std::sqrt(1.0f + dx * dx + dz * dz);
                if (normalY < 0.88f || terrain.GetEntranceRockWeight(x, z) > 0.075f)
                {
                    continue;
                }

                if (terrain.GetMaterialAt(x, y - 0.12f, z, normalY) !=
                    TerrainMaterial::Grass ||
                    !terrain.IsSolid(x, y - 0.80f, z) ||
                    terrain.IsSolid(x, y + 1.30f, z))
                {
                    continue;
                }

                bool grassyFootprint = true;
                constexpr float rootOffset = 1.15f;
                for (int corner = 0; corner < 4; ++corner)
                {
                    const float sampleX = x + (corner & 1 ? rootOffset : -rootOffset);
                    const float sampleZ = z + (corner & 2 ? rootOffset : -rootOffset);
                    const float sampleY = terrain.GetSurfaceHeight(sampleX, sampleZ);
                    if (!std::isfinite(sampleY) ||
                        std::abs(sampleY - y) > 1.7f ||
                        terrain.GetMaterialAt(sampleX, sampleY - 0.12f,
                            sampleZ, normalY) != TerrainMaterial::Grass ||
                        !terrain.IsSolid(sampleX, sampleY - 0.65f, sampleZ))
                    {
                        grassyFootprint = false;
                        break;
                    }
                }
                if (!grassyFootprint)
                {
                    continue;
                }

                ForestTree tree{};
                tree.id = ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellX)) << 32) |
                    static_cast<std::uint32_t>(cellZ)) ^ 0x93d62fd9f56c41b5ULL;
                tree.x = x;
                tree.y = y - 0.20f;
                tree.z = z;
                tree.rotation = 2.0f * Pi * Random01(hash + 41u);
                tree.height = 5.6f + 4.2f * Random01(hash + 47u);
                tree.crownRadius = 1.9f + 1.35f * Random01(hash + 59u);
                const float type = Random01(hash + 67u);
                tree.species = type < 0.48f ? TreeSpecies::Pine :
                    type < 0.84f ? TreeSpecies::Broadleaf : TreeSpecies::Birch;

                bool clearOfOre = true;
                for (const auto& deposit : outcrops)
                {
                    const float dxOre = x - deposit.x;
                    const float dzOre = z - deposit.z;
                    const float clearance = deposit.radius + tree.crownRadius + 2.0f;
                    if (dxOre * dxOre + dzOre * dzOre < clearance * clearance)
                    {
                        clearOfOre = false;
                        break;
                    }
                }
                if (clearOfOre)
                {
                    trees.push_back(tree);
                }
            }
        }
    }

    void ForestGenerator::GeneratePebbles(const VoxelTerrain& terrain,
        int chunkX, int chunkZ, std::vector<ForestPebble>& pebbles)
    {
        pebbles.clear();
        const float chunkSize = terrain.GetChunkSize();
        const float cell = 7.5f;
        const float minX = chunkX * chunkSize;
        const float maxX = minX + chunkSize;
        const float minZ = chunkZ * chunkSize;
        const float maxZ = minZ + chunkSize;
        const auto seed = static_cast<std::uint32_t>(terrain.GetGenerator().GetSettings().seed);
        const int startX = static_cast<int>(std::floor(minX / cell));
        const int endX = static_cast<int>(std::ceil(maxX / cell));
        const int startZ = static_cast<int>(std::floor(minZ / cell));
        const int endZ = static_cast<int>(std::ceil(maxZ / cell));
        for (int cz = startZ; cz <= endZ; ++cz)
        {
            for (int cx = startX; cx <= endX; ++cx)
            {
                const auto hash = Hash(cx, cz, seed ^ 0x74ab110fu);
                if (Random01(hash + 19u) > 0.08f)
                    continue;
                const float x = (cx + 0.18f + Random01(hash + 3u) * 0.64f) * cell;
                const float z = (cz + 0.18f + Random01(hash + 11u) * 0.64f) * cell;
                if (x < minX || x >= maxX || z < minZ || z >= maxZ)
                    continue;
                const float y = terrain.GetSurfaceHeight(x, z);
                if (!std::isfinite(y) || y < -25.0f || y > 60.0f ||
                    terrain.GetEntranceRockWeight(x, z) > 0.075f ||
                    !terrain.IsSolid(x, y - 0.6f, z) ||
                    terrain.IsSolid(x, y + 0.7f, z))
                    continue;
                const float delta = 1.25f;
                const float sx = terrain.GetSurfaceHeight(x + delta, z) -
                    terrain.GetSurfaceHeight(x - delta, z);
                const float sz = terrain.GetSurfaceHeight(x, z + delta) -
                    terrain.GetSurfaceHeight(x, z - delta);
                const float normalY = 1.0f / std::sqrt(1.0f +
                    (sx * sx + sz * sz) / (4.0f * delta * delta));
                const auto material = terrain.GetMaterialAt(x, y - 0.15f, z, normalY);
                if (normalY < 0.82f || (material != TerrainMaterial::Stone &&
                    material != TerrainMaterial::Grass &&
                    material != TerrainMaterial::DirtClay))
                    continue;
                ForestPebble pebble{};
                pebble.id = ((static_cast<std::uint64_t>(static_cast<std::uint32_t>(cx)) << 32) |
                    static_cast<std::uint32_t>(cz)) ^ 0x3b1659ea4217d80bULL;
                pebble.x = x;
                pebble.y = y - 0.02f;
                pebble.z = z;
                pebble.radius = 0.22f + Random01(hash + 47u) * 0.17f;
                pebbles.push_back(pebble);
            }
        }
    }

    void ForestGenerator::BuildPebbleMesh(
        const std::vector<ForestPebble>& pebbles, VoxelMesh& mesh)
    {
        mesh.Clear();
        constexpr int sides = 7;
        for (const auto& stone : pebbles)
        {
            const Point top{stone.x, stone.y + stone.radius * 0.78f, stone.z};
            const Point base{stone.x, stone.y + 0.015f, stone.z};
            for (int i = 0; i < sides; ++i)
            {
                const float a = 2.0f * Pi * i / sides;
                const float b = 2.0f * Pi * (i + 1) / sides;
                const Point p0{stone.x + std::cos(a) * stone.radius,
                    stone.y + 0.08f, stone.z + std::sin(a) * stone.radius};
                const Point p1{stone.x + std::cos(b) * stone.radius,
                    stone.y + 0.08f, stone.z + std::sin(b) * stone.radius};
                const float shade = 0.58f + static_cast<float>(i % 3) * 0.07f;
                Triangle(mesh, p0, top, p1, {shade, shade * 0.98f, shade * 0.92f});
                Triangle(mesh, p1, base, p0, {0.32f, 0.32f, 0.33f});
            }
        }
    }

    void ForestGenerator::BuildMesh(const std::vector<ForestTree>& trees, VoxelMesh& mesh)
    {
        mesh.Clear();
        mesh.vertices.reserve(trees.size() * 250);
        mesh.indices.reserve(trees.size() * 250);
        for (const auto& tree : trees)
        {
            BuildTree(mesh, tree);
        }
    }

    void ForestGenerator::GenerateChunk(const VoxelTerrain& terrain,
        int chunkX, int chunkZ, VoxelMesh& mesh)
    {
        std::vector<ForestTree> trees;
        GenerateTrees(terrain, chunkX, chunkZ, trees);
        BuildMesh(trees, mesh);
    }
}
