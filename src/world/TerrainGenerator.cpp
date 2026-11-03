
#include "industrial_frontier/world/TerrainGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace industrial_frontier
{
    namespace
    {
        float Clamp01(float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        }

        float SmoothRange(float value, float minimum, float maximum)
        {
            if (maximum <= minimum)
            {
                return value >= maximum ? 1.0f : 0.0f;
            }

            const float t = Clamp01(
                (value - minimum) / (maximum - minimum)
            );

            return t * t * (3.0f - 2.0f * t);
        }

        float Saturate(float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        }
    }

    TerrainGenerator::TerrainGenerator()
        : m_settings()
    {
    }

    TerrainGenerator::TerrainGenerator(const TerrainSettings& settings)
        : m_settings(settings)
    {
    }

    void TerrainGenerator::SetSettings(const TerrainSettings& settings)
    {
        m_settings = settings;
    }

    const TerrainSettings& TerrainGenerator::GetSettings() const
    {
        return m_settings;
    }

    std::uint32_t TerrainGenerator::Hash(
        int x,
        int z,
        std::uint32_t seed)
    {
        std::uint32_t h = seed;

        h ^= static_cast<std::uint32_t>(x) * 374761393u;
        h = (h << 13) | (h >> 19);
        h ^= static_cast<std::uint32_t>(z) * 668265263u;

        h *= 1274126177u;
        h ^= h >> 16;
        h *= 2246822519u;
        h ^= h >> 13;

        return h;
    }

    float TerrainGenerator::SmoothStep(float value)
    {
        value = Saturate(value);
        return value * value * (3.0f - 2.0f * value);
    }

    float TerrainGenerator::Interpolate(
        float a,
        float b,
        float t)
    {
        return a + (b - a) * t;
    }

    float TerrainGenerator::ValueNoise(
        float x,
        float z,
        std::uint32_t seed)
    {
        const int x0 = static_cast<int>(std::floor(x));
        const int z0 = static_cast<int>(std::floor(z));

        const int x1 = x0 + 1;
        const int z1 = z0 + 1;

        const float tx = SmoothStep(x - static_cast<float>(x0));
        const float tz = SmoothStep(z - static_cast<float>(z0));

        const auto ToFloat = [](std::uint32_t value)
        {
            return static_cast<float>(value & 0x00FFFFFFu) /
                16777215.0f;
        };

        const float a = ToFloat(Hash(x0, z0, seed));
        const float b = ToFloat(Hash(x1, z0, seed));
        const float c = ToFloat(Hash(x0, z1, seed));
        const float d = ToFloat(Hash(x1, z1, seed));

        const float top = Interpolate(a, b, tx);
        const float bottom = Interpolate(c, d, tx);

        return Interpolate(top, bottom, tz);
    }

    float TerrainGenerator::FractalNoise(
        float x,
        float z,
        std::uint32_t seed,
        int octaves,
        float persistence,
        float lacunarity)
    {
        octaves = std::clamp(octaves, 1, 10);
        persistence = std::clamp(persistence, 0.05f, 0.95f);
        lacunarity = std::clamp(lacunarity, 1.1f, 4.0f);

        float frequency = 1.0f;
        float amplitude = 1.0f;
        float total = 0.0f;
        float totalAmplitude = 0.0f;

        for (int i = 0; i < octaves; ++i)
        {
            const std::uint32_t octaveSeed =
                seed + static_cast<std::uint32_t>(i) * 1013u;

            total += ValueNoise(
                x * frequency,
                z * frequency,
                octaveSeed
            ) * amplitude;

            totalAmplitude += amplitude;
            amplitude *= persistence;
            frequency *= lacunarity;
        }

        if (totalAmplitude <= 0.0001f)
        {
            return 0.5f;
        }

        return total / totalAmplitude;
    }

    float TerrainGenerator::GetHeight(float worldX, float worldZ) const
    {
        const TerrainSettings& s = m_settings;

        const std::uint32_t seed =
            static_cast<std::uint32_t>(s.seed);

        const float continentalScale =
            std::max(s.continentalScale, 0.00001f);

        const float mountainScale =
            std::max(s.mountainScale, 0.00001f);

        const float detailScale =
            std::max(s.detailScale, 0.00001f);

        const float valleyScale =
            std::max(s.valleyScale, 0.00001f);

        const float canyonScale =
            std::max(s.canyonScale, 0.00001f);

        const int octaves = std::clamp(s.noiseOctaves, 1, 10);

        const float persistence =
            std::clamp(s.persistence, 0.05f, 0.95f);

        const float lacunarity =
            std::clamp(s.lacunarity, 1.1f, 4.0f);

        const float continentalWarpX =
            (FractalNoise(
                worldX * continentalScale * 0.65f,
                worldZ * continentalScale * 0.65f,
                seed + 11u, 3, 0.5f, 2.0f
            ) - 0.5f) * 180.0f;

        const float continentalWarpZ =
            (FractalNoise(
                worldX * continentalScale * 0.65f,
                worldZ * continentalScale * 0.65f,
                seed + 29u, 3, 0.5f, 2.0f
            ) - 0.5f) * 180.0f;

        const float wx = worldX + continentalWarpX;
        const float wz = worldZ + continentalWarpZ;

        const float continental = FractalNoise(
            wx * continentalScale,
            wz * continentalScale,
            seed + 101u,
            4,
            persistence,
            lacunarity
        );

        const float broadNoise = FractalNoise(
            wx * continentalScale * 2.2f,
            wz * continentalScale * 2.2f,
            seed + 202u,
            4,
            persistence,
            lacunarity
        );

        const float detailNoise = FractalNoise(
            worldX * detailScale,
            worldZ * detailScale,
            seed + 303u,
            3,
            0.48f,
            2.0f
        );

        const float plainsNoise = FractalNoise(
            wx * continentalScale * 3.2f,
            wz * continentalScale * 3.2f,
            seed + 404u,
            3,
            0.5f,
            2.0f
        );

        const float highlandMask = SmoothRange(
            continental,
            0.48f,
            0.72f
        );

        const float mountainRegion = SmoothRange(
            FractalNoise(
                wx * continentalScale * 0.85f,
                wz * continentalScale * 0.85f,
                seed + 505u,
                3,
                0.5f,
                2.0f
            ),
            0.48f,
            0.68f
        );

        const float plainsMask =
            (1.0f - SmoothRange(continental, 0.42f, 0.67f)) *
            (1.0f - SmoothRange(plainsNoise, 0.62f, 0.82f));

        const float broadLand =
            (continental - 0.48f) * s.heightScale * 1.5f;

        const float rollingTerrain =
            (broadNoise - 0.5f) * 12.0f *
            (1.0f - plainsMask * Saturate(s.plainsFlatness));

        const float plainsVariation =
            (plainsNoise - 0.5f) * 5.0f *
            (1.0f - Saturate(s.plainsFlatness));

        const float mountainWarpX =
            (FractalNoise(
                wx * mountainScale * 0.35f,
                wz * mountainScale * 0.35f,
                seed + 606u, 3, 0.5f, 2.0f
            ) - 0.5f) * 75.0f;

        const float mountainWarpZ =
            (FractalNoise(
                wx * mountainScale * 0.35f,
                wz * mountainScale * 0.35f,
                seed + 707u, 3, 0.5f, 2.0f
            ) - 0.5f) * 75.0f;

        const float mx = wx + mountainWarpX;
        const float mz = wz + mountainWarpZ;

        const float mountainNoise = FractalNoise(
            mx * mountainScale,
            mz * mountainScale,
            seed + 808u,
            octaves,
            persistence,
            lacunarity
        );

        const float ridgeNoise = FractalNoise(
            mx * mountainScale * 1.35f,
            mz * mountainScale * 1.35f,
            seed + 909u,
            4,
            0.52f,
            2.0f
        );

        const float ridgedShape =
            1.0f - std::abs(ridgeNoise * 2.0f - 1.0f);

        const float ridgePeaks = std::pow(
            Saturate(ridgedShape),
            1.65f
        );

        const float mountainBody =
            SmoothRange(mountainNoise, 0.36f, 0.72f);

        const float mountainHeight =
            (mountainBody * 0.48f + ridgePeaks * 0.52f) *
            s.mountainHeight *
            s.mountainStrength *
            s.mountainRangeStrength *
            mountainRegion *
            highlandMask;

        const float foothillMask =
            SmoothRange(mountainRegion, 0.12f, 0.68f) *
            (1.0f - SmoothRange(mountainRegion, 0.72f, 0.98f));

        const float foothillNoise = FractalNoise(
            wx * mountainScale * 0.65f,
            wz * mountainScale * 0.65f,
            seed + 1001u,
            4,
            0.55f,
            2.0f
        );

        const float foothills =
            foothillMask *
            (6.0f + foothillNoise * 13.0f) *
            s.foothillStrength;

        const float valleyWarp =
            (FractalNoise(
                wx * valleyScale * 0.4f,
                wz * valleyScale * 0.4f,
                seed + 1102u, 3, 0.5f, 2.0f
            ) - 0.5f) * 0.18f;

        const float valleyField = FractalNoise(
            wx * valleyScale + valleyWarp,
            wz * valleyScale - valleyWarp,
            seed + 1203u,
            3,
            0.55f,
            2.0f
        );

        const float valleyChannel =
            1.0f - std::abs(valleyField * 2.0f - 1.0f);

        const float valleyWidth =
            std::clamp(s.valleyWidth, 0.03f, 0.85f);

        const float valleyMask = SmoothRange(
            valleyChannel,
            1.0f - valleyWidth,
            1.0f - valleyWidth * 0.35f
        );

        const float valleyCut =
            valleyMask *
            s.valleyDepth *
            s.valleyStrength;

        const float canyonWarpX =
            (FractalNoise(
                wx * canyonScale * 0.55f,
                wz * canyonScale * 0.55f,
                seed + 1304u, 3, 0.5f, 2.0f
            ) - 0.5f) * 55.0f;

        const float canyonWarpZ =
            (FractalNoise(
                wx * canyonScale * 0.55f,
                wz * canyonScale * 0.55f,
                seed + 1405u, 3, 0.5f, 2.0f
            ) - 0.5f) * 55.0f;

        const float canyonField = FractalNoise(
            (wx + canyonWarpX) * canyonScale,
            (wz + canyonWarpZ) * canyonScale,
            seed + 1506u,
            3,
            0.55f,
            2.0f
        );

        const float canyonRidge =
            1.0f - std::abs(canyonField * 2.0f - 1.0f);

        const float canyonWidth =
            std::clamp(s.canyonWidth, 0.025f, 0.75f);

        const float canyonMask = SmoothRange(
            canyonRidge,
            1.0f - canyonWidth,
            1.0f - canyonWidth * 0.25f
        );

        const float canyonStrengthMask =
            SmoothRange(mountainRegion, 0.10f, 0.45f) * 0.65f +
            0.35f;

        const float canyonCut =
            canyonMask *
            s.canyonDepth *
            s.canyonStrength *
            canyonStrengthMask;

        const float plateauNoise = FractalNoise(
            wx * mountainScale * 0.75f,
            wz * mountainScale * 0.75f,
            seed + 1607u,
            3,
            0.5f,
            2.0f
        );

        const float plateauMask =
            SmoothRange(plateauNoise, 0.57f, 0.78f) *
            highlandMask *
            s.plateauStrength;

        const float plateauLevel =
            std::round(mountainHeight / 9.0f) * 9.0f;

        const float plateauFormation =
            plateauLevel * plateauMask * 0.12f;

        const float fineDetail =
            (detailNoise - 0.5f) *
            4.0f *
            s.detailStrength;

        const float mediumDetail =
            (broadNoise - 0.5f) * 3.5f;

        const float plainsBlend =
            Saturate(plainsMask * s.plainsStrength);

        const float terrainHeight =
            broadLand +
            rollingTerrain +
            foothills +
            mountainHeight -
            valleyCut -
            canyonCut +
            plateauFormation +
            fineDetail +
            mediumDetail +
            plainsVariation;

        const float flattenedHeight =
            broadLand +
            (rollingTerrain + fineDetail + mediumDetail) *
            (1.0f - plainsBlend * 0.82f) +
            plainsVariation;

        return s.baseHeight +
            Interpolate(flattenedHeight, terrainHeight, 1.0f - plainsBlend);
    }
}
