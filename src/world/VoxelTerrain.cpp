#include "industrial_frontier/world/VoxelTerrain.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <mutex>
#include <utility>

namespace industrial_frontier
{
    namespace
    {
        constexpr float CaveCellSize = 256.0f;
        constexpr float CorridorLength = 74.0f;
        constexpr float CorridorSlope = 0.28f;
        constexpr float Pi = 3.14159265358979323846f;
        constexpr std::size_t HeightCacheSize = 8192;
        constexpr std::size_t SiteCacheSize = 256;

        std::atomic<std::uint64_t> NextGenerationId{1};

        struct CaveSite
        {
            std::uint64_t generationId = 0;
            int cellX = std::numeric_limits<int>::min();
            int cellZ = std::numeric_limits<int>::min();
            bool active = false;
            float entryX = 0.0f;
            float entryY = 0.0f;
            float entryZ = 0.0f;
            float directionX = 1.0f;
            float directionZ = 0.0f;
            float endX = 0.0f;
            float endY = 0.0f;
            float endZ = 0.0f;
            float corridorRadius = 5.2f;
            float entranceRadius = 6.8f;
            float moundHeight = 14.0f;
            float chamberRadiusX = 18.0f;
            float chamberRadiusY = 9.0f;
            float chamberRadiusZ = 16.0f;
            int entranceStyle = 0;
            float moundLength = 30.0f;
            float moundWidth = 25.0f;
            float moundOffset = 7.0f;
            float moundSkew = 0.0f;
            float moundRidge = 0.0f;
            float mouthLength = 15.0f;
            float mouthHeightScale = 1.0f;
            float corridorBend = 0.0f;
            float branchDirection = 1.0f;
            float pocketScale = 1.0f;
            int pocketCount = 1;
        };

        struct HeightEntry
        {
            std::uint64_t generationId = 0;
            std::uint32_t xBits = 0;
            std::uint32_t zBits = 0;
            float height = 0.0f;
        };

        struct TerrainQueryCache
        {
            std::array<HeightEntry, HeightCacheSize> heights{};
            std::array<CaveSite, SiteCacheSize> sites{};
        };

        TerrainQueryCache& GetCache()
        {
            static thread_local TerrainQueryCache cache;
            return cache;
        }

        std::uint32_t Mix(std::uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return value;
        }

        std::uint32_t HashCell(int x, int z, std::uint32_t seed)
        {
            return Mix(seed ^
                (static_cast<std::uint32_t>(x) * 0x9e3779b9u) ^
                (static_cast<std::uint32_t>(z) * 0x85ebca6bu));
        }

        float Random01(std::uint32_t value)
        {
            return static_cast<float>(Mix(value) & 0x00ffffffu) / 16777215.0f;
        }

        float CachedHeight(
            TerrainQueryCache& cache,
            const TerrainGenerator& generator,
            std::uint64_t generationId,
            float x,
            float z)
        {
            if (!std::isfinite(x) || !std::isfinite(z))
            {
                return generator.GetHeight(x, z);
            }

            const std::uint32_t bx = std::bit_cast<std::uint32_t>(x);
            const std::uint32_t bz = std::bit_cast<std::uint32_t>(z);
            const std::uint32_t hash = Mix(bx ^ Mix(bz));
            HeightEntry& item = cache.heights[hash & (HeightCacheSize - 1)];

            if (item.generationId == generationId &&
                item.xBits == bx && item.zBits == bz)
            {
                return item.height;
            }

            const float height = generator.GetHeight(x, z);
            item = {generationId, bx, bz, height};
            return height;
        }

        CaveSite& GetSite(
            TerrainQueryCache& cache,
            const TerrainGenerator& generator,
            std::uint64_t generationId,
            int cellX,
            int cellZ)
        {
            const std::uint32_t seed =
                static_cast<std::uint32_t>(generator.GetSettings().seed);
            const std::uint32_t hash = HashCell(cellX, cellZ, seed + 42491u);
            const std::size_t index =
                HashCell(cellX, cellZ, 0x98237a49u) & (SiteCacheSize - 1);
            CaveSite& site = cache.sites[index];

            if (site.generationId == generationId &&
                site.cellX == cellX && site.cellZ == cellZ)
            {
                return site;
            }

            site = {};
            site.generationId = generationId;
            site.cellX = cellX;
            site.cellZ = cellZ;

            if ((hash & 3u) == 3u)
            {
                return site;
            }

            site.entryX = (static_cast<float>(cellX) + 0.5f) * CaveCellSize +
                (Random01(hash + 11u) - 0.5f) * 110.0f;
            site.entryZ = (static_cast<float>(cellZ) + 0.5f) * CaveCellSize +
                (Random01(hash + 29u) - 0.5f) * 110.0f;
            site.corridorRadius = 4.9f + Random01(hash + 37u) * 0.45f;
            site.entranceRadius = site.corridorRadius + 1.6f;
            site.moundHeight = 13.5f + Random01(hash + 53u) * 2.8f;
            site.entryY = CachedHeight(
                cache, generator, generationId, site.entryX, site.entryZ) +
                site.corridorRadius - 1.25f;
            site.chamberRadiusX = 17.0f + Random01(hash + 41u) * 4.0f;
            site.chamberRadiusY = 8.5f + Random01(hash + 43u) * 1.5f;
            site.chamberRadiusZ = 15.0f + Random01(hash + 47u) * 4.5f;

            float bestCover = -std::numeric_limits<float>::infinity();
            const int initialDirection = static_cast<int>((hash >> 9) & 7u);

            for (int i = 0; i < 8; ++i)
            {
                const int directionIndex = (i + initialDirection) & 7;
                const float angle = static_cast<float>(directionIndex) * Pi * 0.25f;
                const float dx = std::cos(angle);
                const float dz = std::sin(angle);
                float cover = std::numeric_limits<float>::infinity();

                for (float t : {0.42f, 0.72f, 1.0f})
                {
                    const float distance = CorridorLength * t;
                    const float x = site.entryX + dx * distance;
                    const float z = site.entryZ + dz * distance;
                    const float centreY = site.entryY - distance * CorridorSlope;
                    const float roofRadius = t > 0.95f
                        ? 2.0f * site.chamberRadiusY - site.corridorRadius
                        : site.corridorRadius;
                    cover = std::min(cover, CachedHeight(
                        cache, generator, generationId, x, z) -
                        (centreY + roofRadius));
                }

                if (cover > bestCover)
                {
                    bestCover = cover;
                    site.directionX = dx;
                    site.directionZ = dz;
                }
            }

            if (bestCover < 2.0f ||
                site.entryY - CorridorLength * CorridorSlope -
                    site.corridorRadius < -88.0f ||
                site.entryY + site.entranceRadius + site.moundHeight > 155.0f)
            {
                return site;
            }

            site.endX = site.entryX + site.directionX * CorridorLength;
            site.endZ = site.entryZ + site.directionZ * CorridorLength;
            site.endY = site.entryY - CorridorLength * CorridorSlope;

            const std::uint32_t variation = HashCell(
                cellX, cellZ, seed + 0x3a51c9b7u);
            site.entranceStyle = static_cast<int>(variation & 3u);
            site.moundLength = 27.0f + 19.0f * Random01(variation + 101u);
            site.moundWidth = 19.0f + 16.0f * Random01(variation + 103u);
            site.moundOffset = 5.0f + 10.0f * Random01(variation + 107u);
            site.moundSkew = (Random01(variation + 109u) - 0.5f) * 0.45f;
            site.moundRidge = (Random01(variation + 113u) - 0.5f) * 0.36f;
            site.moundHeight = 10.0f + 10.0f * Random01(variation + 127u);
            site.moundHeight = std::min(site.moundHeight,
                std::max(7.0f, 150.0f - site.entryY - site.entranceRadius));
            site.entranceRadius = site.corridorRadius +
                1.2f + 2.6f * Random01(variation + 131u);
            site.mouthLength = 11.0f + 11.0f * Random01(variation + 137u);
            site.mouthHeightScale = 0.85f +
                0.35f * Random01(variation + 139u);
            site.corridorBend = (Random01(variation + 149u) - 0.5f) * 14.0f;
            site.branchDirection = (variation & 8u) ? -1.0f : 1.0f;
            site.pocketScale = 0.75f + 0.6f * Random01(variation + 151u);
            site.pocketCount = static_cast<int>((variation >> 4) % 3u);
            site.active = true;
            return site;
        }

        float CapsuleDistance(
            float x,
            float y,
            float z,
            float ax,
            float ay,
            float az,
            float bx,
            float by,
            float bz,
            float radius)
        {
            const float vx = bx - ax;
            const float vy = by - ay;
            const float vz = bz - az;
            const float px = x - ax;
            const float py = y - ay;
            const float pz = z - az;
            const float lenSquared = vx * vx + vy * vy + vz * vz;
            const float t = lenSquared > 0.00001f
                ? std::clamp((px * vx + py * vy + pz * vz) / lenSquared, 0.0f, 1.0f)
                : 0.0f;
            const float dx = px - vx * t;
            const float dy = py - vy * t;
            const float dz = pz - vz * t;
            return std::sqrt(dx * dx + dy * dy + dz * dz) - radius;
        }

        float ChamberDistance(
            float x,
            float y,
            float z,
            const CaveSite& site)
        {
            const float px = x - site.endX;
            const float pz = z - site.endZ;
            const float along = px * site.directionX + pz * site.directionZ;
            const float across = -px * site.directionZ + pz * site.directionX;
            const float vertical = y - (
                site.endY + site.chamberRadiusY - site.corridorRadius);
            const float rx = site.chamberRadiusX;
            const float ry = site.chamberRadiusY;
            const float rz = site.chamberRadiusZ;
            const float scaled = std::sqrt(
                (along * along) / (rx * rx) +
                (vertical * vertical) / (ry * ry) +
                (across * across) / (rz * rz));
            float result = (scaled - 1.0f) * std::min({rx, ry, rz});

            if (vertical > -ry * 0.65f)
            {
                result += 0.23f * std::sin(x * 0.38f + y * 0.24f) *
                    std::sin(z * 0.44f - y * 0.17f);
            }

            return result;
        }

        float SiteDistance(float x, float y, float z, const CaveSite& site)
        {
            const float perpendicularX = -site.directionZ;
            const float perpendicularZ = site.directionX;
            const float deltaX = site.endX - site.entryX;
            const float deltaY = site.endY - site.entryY;
            const float deltaZ = site.endZ - site.entryZ;

            const float firstX = site.entryX + deltaX * 0.33f +
                perpendicularX * site.corridorBend * 0.45f;
            const float firstY = site.entryY + deltaY * 0.33f;
            const float firstZ = site.entryZ + deltaZ * 0.33f +
                perpendicularZ * site.corridorBend * 0.45f;
            const float secondX = site.entryX + deltaX * 0.68f +
                perpendicularX * site.corridorBend;
            const float secondY = site.entryY + deltaY * 0.68f;
            const float secondZ = site.entryZ + deltaZ * 0.68f +
                perpendicularZ * site.corridorBend;

            const float corridor = std::min({
                CapsuleDistance(x, y, z,
                    site.entryX, site.entryY, site.entryZ,
                    firstX, firstY, firstZ, site.corridorRadius),
                CapsuleDistance(x, y, z,
                    firstX, firstY, firstZ,
                    secondX, secondY, secondZ, site.corridorRadius),
                CapsuleDistance(x, y, z,
                    secondX, secondY, secondZ,
                    site.endX, site.endY, site.endZ, site.corridorRadius)
            });

            const float mouthY = site.entryY +
                (y - site.entryY) / site.mouthHeightScale;
            const float mouth = CapsuleDistance(
                x, mouthY, z,
                site.entryX - site.directionX * site.mouthLength,
                site.entryY + 1.5f,
                site.entryZ - site.directionZ * site.mouthLength,
                site.entryX + site.directionX * 15.0f,
                site.entryY - 2.8f,
                site.entryZ + site.directionZ * 15.0f,
                site.entranceRadius);
            const float chamber = ChamberDistance(x, y, z, site);
            float result = std::min({corridor, mouth, chamber});

            if (site.pocketCount >= 1)
            {
                const float branchX = site.endX + site.directionX * 11.0f +
                    perpendicularX * site.branchDirection * 8.0f;
                const float branchY = site.endY + 5.6f - site.corridorRadius;
                const float branchZ = site.endZ + site.directionZ * 11.0f +
                    perpendicularZ * site.branchDirection * 8.0f;
                const float bx = (x - branchX) / (9.5f * site.pocketScale);
                const float by = (y - branchY) / (5.6f * site.pocketScale);
                const float bz = (z - branchZ) / (8.0f * site.pocketScale);
                const float pocket =
                    (std::sqrt(bx * bx + by * by + bz * bz) - 1.0f) *
                    (5.6f * site.pocketScale);
                result = std::min(result, pocket);
            }

            if (site.pocketCount >= 2)
            {
                const float branchX = site.endX - site.directionX * 7.0f -
                    perpendicularX * site.branchDirection * 15.0f;
                const float branchY = site.endY + 5.0f - site.corridorRadius;
                const float branchZ = site.endZ - site.directionZ * 7.0f -
                    perpendicularZ * site.branchDirection * 15.0f;
                const float bx = (x - branchX) / 11.0f;
                const float by = (y - branchY) / 6.0f;
                const float bz = (z - branchZ) / 8.5f;
                const float pocket =
                    (std::sqrt(bx * bx + by * by + bz * bz) - 1.0f) * 6.0f;
                result = std::min(result, pocket);
            }

            return result;
        }

        float MoundHeightAt(float x, float z, const CaveSite& site)
        {
            const float px = x - (site.entryX +
                site.directionX * site.moundOffset);
            const float pz = z - (site.entryZ +
                site.directionZ * site.moundOffset);
            const float along = px * site.directionX + pz * site.directionZ;
            const float across = -px * site.directionZ + pz * site.directionX;
            const float skewedAcross = across - along * site.moundSkew;
            const float nx = along / site.moundLength;
            const float nz = skewedAcross / site.moundWidth;
            const float normalized = nx * nx + nz * nz;
            const float fade = std::max(0.0f, 1.0f - normalized);
            if (fade <= 0.0f)
            {
                return 0.0f;
            }

            const float irregularity = 1.0f +
                site.moundRidge * nx * nz +
                0.065f * std::sin(x * 0.16f + z * 0.12f) *
                    std::sin(x * 0.075f - z * 0.13f);

            float profile = fade * fade;
            switch (site.entranceStyle)
            {
                case 0:
                    profile = fade * fade * (3.0f - 2.0f * fade);
                    break;
                case 1:
                    profile = fade * fade *
                        (1.25f - 0.25f * std::abs(nz));
                    break;
                case 2:
                {
                    const float left = std::max(0.0f,
                        1.0f - ((along - 4.0f) * (along - 4.0f)) / 500.0f -
                        ((across - 10.0f) * (across - 10.0f)) / 180.0f);
                    const float right = std::max(0.0f,
                        1.0f - ((along + 2.0f) * (along + 2.0f)) / 420.0f -
                        ((across + 10.0f) * (across + 10.0f)) / 180.0f);
                    profile = 0.55f * fade * fade +
                        0.45f * std::max(left * left, right * right);
                    break;
                }
                default:
                    profile = fade * fade *
                        (1.0f + 0.35f * nx - 0.15f * nz);
                    break;
            }

            return std::max(0.0f,
                site.moundHeight * profile * irregularity);
        }

        float SurfaceWithMounds(
            TerrainQueryCache& cache,
            const TerrainGenerator& generator,
            std::uint64_t generationId,
            float x,
            float z)
        {
            float result = CachedHeight(cache, generator, generationId, x, z);
            const int cellX = static_cast<int>(std::floor(x / CaveCellSize));
            const int cellZ = static_cast<int>(std::floor(z / CaveCellSize));
            for (int dz = -1; dz <= 1; ++dz)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const CaveSite& site = GetSite(
                        cache, generator, generationId, cellX + dx, cellZ + dz);
                    if (!site.active ||
                        std::abs(x - site.entryX) > 70.0f ||
                        std::abs(z - site.entryZ) > 70.0f)
                    {
                        continue;
                    }
                    result += MoundHeightAt(x, z, site);
                }
            }
            return result;
        }

        float EntranceRockInfluence(
            TerrainQueryCache& cache,
            const TerrainGenerator& generator,
            std::uint64_t generationId,
            float x,
            float z)
        {
            const int cellX = static_cast<int>(std::floor(x / CaveCellSize));
            const int cellZ = static_cast<int>(std::floor(z / CaveCellSize));
            float strongest = 0.0f;
            for (int dz = -1; dz <= 1; ++dz)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const CaveSite& site = GetSite(
                        cache, generator, generationId, cellX + dx, cellZ + dz);
                    if (!site.active)
                    {
                        continue;
                    }
                    const float px = x - site.entryX;
                    const float pz = z - site.entryZ;
                    const float along = px * site.directionX + pz * site.directionZ;
                    const float across = -px * site.directionZ + pz * site.directionX;
                    const float relative = std::sqrt(
                        (along * along) /
                            ((site.entranceRadius * 2.6f) *
                             (site.entranceRadius * 2.6f)) +
                        (across * across) /
                            ((site.entranceRadius * 2.0f) *
                             (site.entranceRadius * 2.0f)));
                    const float influence = std::clamp(
                        (1.15f - relative) * 1.7f, 0.0f, 1.0f);
                    strongest = std::max(strongest, influence);
                }
            }
            return strongest;
        }

        float CaveSignedDistance(
            TerrainQueryCache& cache,
            const TerrainGenerator& generator,
            std::uint64_t generationId,
            float x,
            float y,
            float z)
        {
            const int cellX = static_cast<int>(std::floor(x / CaveCellSize));
            const int cellZ = static_cast<int>(std::floor(z / CaveCellSize));
            float best = std::numeric_limits<float>::infinity();

            for (int dz = -1; dz <= 1; ++dz)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const CaveSite& site = GetSite(
                        cache, generator, generationId, cellX + dx, cellZ + dz);
                    if (!site.active ||
                        y > site.entryY + 15.0f ||
                        y < site.endY - site.chamberRadiusY - 5.0f)
                    {
                        continue;
                    }

                    const float horizontalX = x - site.entryX;
                    const float horizontalZ = z - site.entryZ;
                    if (horizontalX * horizontalX + horizontalZ * horizontalZ >
                        112.0f * 112.0f)
                    {
                        continue;
                    }

                    best = std::min(best, SiteDistance(x, y, z, site));
                }
            }
            return best;
        }

        float SmoothRange(float a, float b, float value)
        {
            const float t = std::clamp((value - a) / (b - a), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }
    }

    VoxelTerrain::VoxelTerrain(
        const TerrainSettings& settings,
        float chunkSize,
        float voxelSpacing)
        : m_generator(settings),
          m_chunkSize(std::max(chunkSize, 1.0f)),
          m_voxelSpacing(std::max(voxelSpacing, 0.1f)),
          m_generationId(NextGenerationId.fetch_add(1, std::memory_order_relaxed))
    {
    }

    float VoxelTerrain::Clamp01(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    float VoxelTerrain::SmoothFalloff(float value)
    {
        const float t = Clamp01(value);
        return t * t * (3.0f - 2.0f * t);
    }

    float VoxelTerrain::DistanceSquared(
        float ax,
        float ay,
        float az,
        float bx,
        float by,
        float bz)
    {
        const float dx = ax - bx;
        const float dy = ay - by;
        const float dz = az - bz;
        return dx * dx + dy * dy + dz * dz;
    }

    float VoxelTerrain::SampleDensity(
        float x,
        float y,
        float z) const
    {
        TerrainQueryCache& cache = GetCache();
        const float surface = SurfaceWithMounds(
            cache, m_generator, m_generationId, x, z);
        float density = surface - y;

        if (density > -5.0f)
        {
            density = std::min(density, CaveSignedDistance(
                cache, m_generator, m_generationId, x, y, z));
        }

        std::shared_lock<std::shared_mutex> lock(m_mutex);

        for (const VoxelEdit& edit : m_edits)
        {
            const float radius = edit.radius * edit.strength;
            if (radius <= 0.0f)
            {
                continue;
            }

            const float limit = radius + m_voxelSpacing * 3.0f;
            const float distanceSquared = DistanceSquared(
                x, y, z, edit.x, edit.y, edit.z);

            if (distanceSquared > limit * limit)
            {
                continue;
            }

            const float signedSphereDistance =
                std::sqrt(distanceSquared) - radius;

            if (edit.addMaterial)
            {
                density = std::max(density, -signedSphereDistance);
            }
            else
            {
                density = std::min(density, signedSphereDistance);
            }
        }

        return density;
    }

    float VoxelTerrain::GetSurfaceHeight(float x, float z) const
    {
        return SurfaceWithMounds(
            GetCache(), m_generator, m_generationId, x, z);
    }

    float VoxelTerrain::GetEntranceRockWeight(float x, float z) const
    {
        return EntranceRockInfluence(
            GetCache(), m_generator, m_generationId, x, z);
    }

    TerrainMaterial VoxelTerrain::GetMaterialAt(
        float x, float y, float z, float normalY) const
    {
        const float originalHeight = m_generator.GetHeight(x, z);
        const float surfaceHeight = GetSurfaceHeight(x, z);
        const float depth = surfaceHeight - y;
        if (depth > 7.0f)
        {
            return TerrainMaterial::Stone;
        }
        const float rim = GetEntranceRockWeight(x, z) *
            (1.0f - SmoothRange(0.32f, 0.72f, normalY));
        if (rim > 0.56f)
        {
            return TerrainMaterial::Stone;
        }
        if (depth > 1.55f)
        {
            return TerrainMaterial::DirtClay;
        }
        const float surfaceAmount = 1.0f - SmoothRange(0.45f, 1.65f, depth);
        if (surfaceAmount < 0.42f)
        {
            return TerrainMaterial::DirtClay;
        }
        const float earthVariation =
            std::sin(x * 0.19f + z * 0.23f) * 1.3f +
            std::sin(x * 0.07f - z * 0.11f) * 1.9f;
        const float snow = SmoothRange(37.0f, 53.0f,
                originalHeight + earthVariation) *
            SmoothRange(0.18f, 0.66f, normalY);
        if (snow > 0.55f)
        {
            return TerrainMaterial::Snow;
        }
        const float grass = SmoothRange(0.40f, 0.76f, normalY) *
            (1.0f - SmoothRange(18.0f, 34.0f, originalHeight)) *
            SmoothRange(-18.0f, -7.0f, originalHeight);
        if (grass > 0.46f)
        {
            return TerrainMaterial::Grass;
        }
        if (originalHeight < -10.0f && normalY > 0.55f)
        {
            return TerrainMaterial::DirtClay;
        }
        return TerrainMaterial::Stone;
    }

    bool VoxelTerrain::IsSolid(float x, float y, float z) const
    {
        return SampleDensity(x, y, z) > 0.0f;
    }

    bool VoxelTerrain::DigSphere(
        float x, float y, float z, float radius, float strength)
    {
        VoxelEdit edit{};
        edit.x = x;
        edit.y = y;
        edit.z = z;
        edit.radius = radius;
        edit.strength = strength;
        edit.addMaterial = false;
        return ApplyEdit(edit);
    }

    bool VoxelTerrain::FillSphere(
        float x, float y, float z, float radius, float strength)
    {
        VoxelEdit edit{};
        edit.x = x;
        edit.y = y;
        edit.z = z;
        edit.radius = radius;
        edit.strength = strength;
        edit.addMaterial = true;
        return ApplyEdit(edit);
    }

    bool VoxelTerrain::ApplyEdit(const VoxelEdit& edit)
    {
        if (!std::isfinite(edit.x) ||
            !std::isfinite(edit.y) ||
            !std::isfinite(edit.z) ||
            !std::isfinite(edit.radius) ||
            !std::isfinite(edit.strength) ||
            edit.radius <= 0.0f ||
            edit.strength <= 0.0f)
        {
            return false;
        }

        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_edits.push_back(edit);
        ++m_revision;
        return true;
    }

    VoxelChunkRange VoxelTerrain::GetAffectedChunks(const VoxelEdit& edit) const
    {
        const float radius = std::max(edit.radius * edit.strength, 0.0f);
        const float influenceRadius = radius + m_voxelSpacing;

        const auto toChunk = [this](float position)
        {
            return static_cast<int>(std::floor(position / m_chunkSize));
        };

        VoxelChunkRange range{};
        range.minX = toChunk(edit.x - influenceRadius);
        range.maxX = toChunk(edit.x + influenceRadius);
        range.minZ = toChunk(edit.z - influenceRadius);
        range.maxZ = toChunk(edit.z + influenceRadius);
        return range;
    }

    std::vector<VoxelEdit> VoxelTerrain::GetEdits() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_edits;
    }

    std::vector<CaveEntrance> VoxelTerrain::FindCaveEntrances(
        float minX,
        float maxX,
        float minZ,
        float maxZ) const
    {
        std::vector<CaveEntrance> found;
        if (!std::isfinite(minX) || !std::isfinite(maxX) ||
            !std::isfinite(minZ) || !std::isfinite(maxZ) ||
            maxX < minX || maxZ < minZ)
        {
            return found;
        }

        TerrainQueryCache& cache = GetCache();
        const int startX = static_cast<int>(std::floor(minX / CaveCellSize)) - 1;
        const int endX = static_cast<int>(std::floor(maxX / CaveCellSize)) + 1;
        const int startZ = static_cast<int>(std::floor(minZ / CaveCellSize)) - 1;
        const int endZ = static_cast<int>(std::floor(maxZ / CaveCellSize)) + 1;

        for (int z = startZ; z <= endZ; ++z)
        {
            for (int x = startX; x <= endX; ++x)
            {
                const CaveSite& site = GetSite(
                    cache, m_generator, m_generationId, x, z);
                if (site.active && site.entryX >= minX &&
                    site.entryX <= maxX && site.entryZ >= minZ &&
                    site.entryZ <= maxZ)
                {
                    found.push_back({
                        site.entryX, site.entryY, site.entryZ,
                        site.endX,
                        site.endY + site.chamberRadiusY - site.corridorRadius,
                        site.endZ
                    });
                }
            }
        }
        return found;
    }

    void VoxelTerrain::SetEdits(const std::vector<VoxelEdit>& edits)
    {
        std::vector<VoxelEdit> validEdits;
        validEdits.reserve(edits.size());
        for (const VoxelEdit& edit : edits)
        {
            if (std::isfinite(edit.x) &&
                std::isfinite(edit.y) &&
                std::isfinite(edit.z) &&
                std::isfinite(edit.radius) &&
                std::isfinite(edit.strength) &&
                edit.radius > 0.0f &&
                edit.strength > 0.0f)
            {
                validEdits.push_back(edit);
            }
        }

        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_edits = std::move(validEdits);
        ++m_revision;
    }

    void VoxelTerrain::ClearEdits()
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (!m_edits.empty())
        {
            m_edits.clear();
            ++m_revision;
        }
    }

    float VoxelTerrain::GetChunkSize() const
    {
        return m_chunkSize;
    }

    float VoxelTerrain::GetVoxelSpacing() const
    {
        return m_voxelSpacing;
    }

    const TerrainGenerator& VoxelTerrain::GetGenerator() const
    {
        return m_generator;
    }

    std::uint64_t VoxelTerrain::GetRevision() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_revision;
    }
}
