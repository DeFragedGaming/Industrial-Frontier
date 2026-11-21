#include "industrial_frontier/renderer/RiverVisuals.hpp"
#include "industrial_frontier/world/RiverSystem.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        struct WaterPoint
        {
            float x = 0.0f;
            float z = 0.0f;
            float y = 0.0f;
            float signedDistance = -1000.0f;
            float flowX = 0.0f;
            float flowZ = 1.0f;
            float speed = 0.0f;
            float lake = 0.0f;
            bool heightReady = false;
        };

        WaterPoint Interpolate(const WaterPoint& a, const WaterPoint& b, float t)
        {
            WaterPoint p;
            p.x = a.x + (b.x - a.x) * t;
            p.z = a.z + (b.z - a.z) * t;
            p.y = a.y + (b.y - a.y) * t;
            p.signedDistance = a.signedDistance + (b.signedDistance - a.signedDistance) * t;
            p.flowX = a.flowX + (b.flowX - a.flowX) * t;
            p.flowZ = a.flowZ + (b.flowZ - a.flowZ) * t;
            p.speed = a.speed + (b.speed - a.speed) * t;
            p.lake = a.lake + (b.lake - a.lake) * t;
            p.heightReady = true;
            return p;
        }

        void Emit(VoxelMesh& mesh, const WaterPoint& a,
            const WaterPoint& b, const WaterPoint& c)
        {
            const float area = (b.x - a.x) * (c.z - a.z) -
                (b.z - a.z) * (c.x - a.x);
            if (std::abs(area) < 0.0001f)
                return;
            const unsigned int first = static_cast<unsigned int>(mesh.vertices.size());
            const WaterPoint* corners[3] = {&a, &b, &c};
            for (const WaterPoint* p : corners)
            {
                TerrainVertex vertex{};
                vertex.position[0] = p->x;
                vertex.position[1] = p->y + 0.035f;
                vertex.position[2] = p->z;
                const float f = std::sqrt(p->flowX * p->flowX + p->flowZ * p->flowZ);
                vertex.normal[0] = f > 0.0001f ? p->flowX / f : 0.0f;
                vertex.normal[1] = std::max(0.0f, p->speed);
                vertex.normal[2] = f > 0.0001f ? p->flowZ / f : 1.0f;
                vertex.color[0] = 0.080f + p->lake * 0.035f;
                vertex.color[1] = 0.350f + p->lake * 0.050f;
                vertex.color[2] = 0.440f + p->lake * 0.035f;
                mesh.vertices.push_back(vertex);
            }
            mesh.indices.push_back(first);
            mesh.indices.push_back(first + 1);
            mesh.indices.push_back(first + 2);
        }

        void ClipAndEmit(VoxelMesh& mesh, const std::array<WaterPoint, 4>& corners)
        {
            std::vector<WaterPoint> polygon;
            polygon.reserve(6);
            for (int i = 0; i < 4; ++i)
            {
                const WaterPoint& a = corners[static_cast<std::size_t>(i)];
                const WaterPoint& b = corners[static_cast<std::size_t>((i + 1) % 4)];
                const bool aInside = a.signedDistance >= 0.0f;
                const bool bInside = b.signedDistance >= 0.0f;
                if (aInside)
                    polygon.push_back(a);
                if (aInside != bInside)
                {
                    const float denominator = a.signedDistance - b.signedDistance;
                    const float t = std::abs(denominator) < 0.00001f ? 0.5f :
                        std::clamp(a.signedDistance / denominator, 0.0f, 1.0f);
                    polygon.push_back(Interpolate(a, b, t));
                }
            }
            if (polygon.size() < 3)
                return;
            for (std::size_t index = 2; index < polygon.size(); ++index)
                Emit(mesh, polygon[0], polygon[index - 1], polygon[index]);
        }
    }

    void RiverVisuals::BuildChunk(const TerrainGenerator& terrain,
        int chunkX, int chunkZ, float chunkSize, VoxelMesh& mesh)
    {
        mesh.Clear();
        if (!std::isfinite(chunkSize) || chunkSize <= 0.0f)
            return;
        const float originX = static_cast<float>(chunkX) * chunkSize;
        const float originZ = static_cast<float>(chunkZ) * chunkSize;
        const int side = std::max(1, static_cast<int>(std::ceil(chunkSize / 2.0f)));
        const int stride = side + 1;
        std::vector<WaterPoint> grid(static_cast<std::size_t>(stride * stride));
        const int seed = terrain.GetSettings().seed;

        for (int iz = 0; iz <= side; ++iz)
        {
            for (int ix = 0; ix <= side; ++ix)
            {
                WaterPoint& p = grid[static_cast<std::size_t>(iz * stride + ix)];
                p.x = originX + chunkSize * static_cast<float>(ix) / static_cast<float>(side);
                p.z = originZ + chunkSize * static_cast<float>(iz) / static_cast<float>(side);
                const RiverSample river = RiverSystem::Sample(p.x, p.z, seed);
                p.signedDistance = river.signedDistance;
                p.flowX = river.flowX;
                p.flowZ = river.flowZ;
                p.speed = river.speed;
                p.lake = river.isLake ? 1.0f : river.lakeInfluence;
            }
        }

        for (int iz = 0; iz < side; ++iz)
        {
            for (int ix = 0; ix < side; ++ix)
            {
                const std::size_t a = static_cast<std::size_t>(iz * stride + ix);
                const std::size_t b = a + 1;
                const std::size_t d = a + static_cast<std::size_t>(stride);
                const std::size_t c = d + 1;
                if (std::max({grid[a].signedDistance, grid[b].signedDistance,
                    grid[c].signedDistance, grid[d].signedDistance}) < 0.0f)
                    continue;
                for (std::size_t key : {a, b, c, d})
                {
                    WaterPoint& p = grid[key];
                    if (!p.heightReady)
                    {
                        p.y = terrain.GetRiverSurfaceHeight(p.x, p.z);
                        p.heightReady = true;
                    }
                }
                const std::array<WaterPoint, 4> cell = {
                    grid[a], grid[b], grid[c], grid[d]};
                ClipAndEmit(mesh, cell);
            }
        }
    }
}
