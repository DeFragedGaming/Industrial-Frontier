#include "industrial_frontier/world/TerrainGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace industrial_frontier
{
    namespace
    {
        constexpr float PI = 3.14159265358979323846f;

        float Clamp01(float value)
        {
            return std::clamp(value, 0.0f, 1.0f);
        }

        float Remap01(float value, float minimum, float maximum)
        {
            if (maximum <= minimum)
            {
                return 0.0f;
            }

            return Clamp01((value - minimum) / (maximum - minimum));
        }

        float SmoothRange(float value, float minimum, float maximum)
        {
            float t = Remap01(value, minimum, maximum);
            return t * t * (3.0f - 2.0f * t);
        }
    }

    TerrainGenerator::TerrainGenerator()
        : TerrainGenerator(TerrainSettings{})
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
        h *= 3266489917u;
        h ^= h >> 16;

        return h;
    }

    float TerrainGenerator::SmoothStep(float value)
    {
        value = Clamp01(value);
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

        const auto RandomValue = [seed](int px, int pz)
        {
            const std::uint32_t hash = Hash(px, pz, seed);

            return static_cast<float>(hash) /
                static_cast<float>(UINT32_MAX);
        };

        const float v00 = RandomValue(x0, z0);
        const float v10 = RandomValue(x1, z0);
        const float v01 = RandomValue(x0, z1);
        const float v11 = RandomValue(x1, z1);

        const float lower = Interpolate(v00, v10, tx);
        const float upper = Interpolate(v01, v11, tx);

        return Interpolate(lower, upper, tz);
    }

    float TerrainGenerator::FractalNoise(
        float x,
        float z,
        std::uint32_t seed,
        int octaves,
        float persistence,
        float lacunarity)
    {
        float total = 0.0f;
        float amplitude = 1.0f;
        float frequency = 1.0f;
        float maximumAmplitude = 0.0f;

        octaves = std::max(octaves, 1);

        for (int octave = 0; octave < octaves; ++octave)
        {
            const std::uint32_t octaveSeed =
                seed + static_cast<std::uint32_t>(octave) * 1013u;

            const float noise = ValueNoise(
                x * frequency,
                z * frequency,
                octaveSeed
            );

            total += noise * amplitude;
            maximumAmplitude += amplitude;

            amplitude *= persistence;
            frequency *= lacunarity;
        }

        if (maximumAmplitude <= 0.0f)
        {
            return 0.5f;
        }

        return total / maximumAmplitude;
    }

    float TerrainGenerator::GetHeight(
        float worldX,
        float worldZ) const
    {
        const std::uint32_t seed =
            static_cast<std::uint32_t>(m_settings.seed);

        const float continentalScale =
            std::max(m_settings.continentalScale, 0.00001f);

        const float mountainScale =
            std::max(m_settings.mountainScale, 0.00001f);

        const float detailScale =
            std::max(m_settings.detailScale, 0.00001f);

        const float persistence =
            std::clamp(m_settings.persistence, 0.1f, 0.9f);

        const float lacunarity =
            std::clamp(m_settings.lacunarity, 1.1f, 4.0f);

        const int octaves =
            std::clamp(m_settings.noiseOctaves, 1, 10);

        const float warpScale = continentalScale * 1.8f;

        const float warpX =
            (ValueNoise(
                worldX * warpScale + 31.7f,
                worldZ * warpScale - 12.4f,
                seed + 1103u
            ) - 0.5f) * 2.0f;

        const float warpZ =
            (ValueNoise(
                worldX * warpScale - 53.2f,
                worldZ * warpScale + 27.9f,
                seed + 2207u
            ) - 0.5f) * 2.0f;

        const float warpedX = worldX + warpX * 180.0f;
        const float warpedZ = worldZ + warpZ * 180.0f;

        const float continental = FractalNoise(
            warpedX * continentalScale,
            warpedZ * continentalScale,
            seed + 10u,
            4,
            0.52f,
            2.0f
        );

        const float continentalShape =
            (continental - 0.5f) * 2.0f;

        const float lowlandNoise = FractalNoise(
            warpedX * continentalScale * 2.7f,
            warpedZ * continentalScale * 2.7f,
            seed + 20u,
            3,
            0.5f,
            2.0f
        );

        const float highlandMask = SmoothRange(
            continental,
            0.43f,
            0.68f
        );

        const float mountainWarpX = FractalNoise(
            warpedX * mountainScale * 0.45f,
            warpedZ * mountainScale * 0.45f,
            seed + 30u,
            3,
            0.5f,
            2.0f
        );

        const float mountainWarpZ = FractalNoise(
            warpedX * mountainScale * 0.45f + 91.0f,
            warpedZ * mountainScale * 0.45f - 47.0f,
            seed + 40u,
            3,
            0.5f,
            2.0f
        );

        const float mountainX =
            warpedX + (mountainWarpX - 0.5f) * 95.0f;

        const float mountainZ =
            warpedZ + (mountainWarpZ - 0.5f) * 95.0f;

        const float mountainBase = FractalNoise(
            mountainX * mountainScale,
            mountainZ * mountainScale,
            seed + 50u,
            octaves,
            persistence,
            lacunarity
        );

        const float ridgeNoise = 1.0f - std::abs(
            mountainBase * 2.0f - 1.0f
        );

        const float ridges = std::pow(
            Clamp01(ridgeNoise),
            1.45f
        );

        const float ridgeDetail = FractalNoise(
            mountainX * mountainScale * 2.2f,
            mountainZ * mountainScale * 2.2f,
            seed + 60u,
            4,
            0.52f,
            2.0f
        );

        const float mountainShape =
            ridges * 0.72f +
            ridgeDetail * 0.28f;

        const float mountainStrength =
            std::max(m_settings.mountainStrength, 0.0f);

        const float mountainHeight =
            mountainShape *
            highlandMask *
            mountainStrength *
            m_settings.heightScale *
            1.65f;

        const float broadTerrain =
            continentalShape *
            m_settings.heightScale *
            0.55f;

        const float rollingTerrain =
            (lowlandNoise - 0.5f) *
            m_settings.heightScale *
            0.30f;

        const float foothills =
            highlandMask *
            (lowlandNoise - 0.35f) *
            m_settings.heightScale *
            0.55f;

        const float valleyNoise = FractalNoise(
            warpedX * continentalScale * 3.2f,
            warpedZ * continentalScale * 3.2f,
            seed + 70u,
            4,
            0.55f,
            2.0f
        );

        const float valleyShape = 1.0f - std::abs(
            valleyNoise * 2.0f - 1.0f
        );

        const float valleyMask = SmoothRange(
            valleyShape,
            0.55f,
            0.90f
        );

        const float valleyStrength =
            std::max(m_settings.valleyStrength, 0.0f);

        const float valleyCut =
            valleyMask *
            valleyStrength *
            highlandMask *
            m_settings.heightScale *
            0.32f;

        const float cliffNoise = FractalNoise(
            warpedX * mountainScale * 1.6f,
            warpedZ * mountainScale * 1.6f,
            seed + 80u,
            3,
            0.55f,
            2.0f
        );

        const float cliffMask = SmoothRange(
            cliffNoise,
            0.58f,
            0.82f
        ) * highlandMask;

        const float plateauNoise = FractalNoise(
            warpedX * mountainScale * 0.8f,
            warpedZ * mountainScale * 0.8f,
            seed + 90u,
            3,
            0.5f,
            2.0f
        );

        const float plateauLevel =
            std::round(plateauNoise * 5.0f) / 5.0f;

        const float plateauShape = Interpolate(
            plateauNoise,
            plateauLevel,
            cliffMask * 0.28f
        );

        const float cliffFormation =
            (plateauShape - plateauNoise) *
            m_settings.heightScale *
            0.85f;

        const float detail = FractalNoise(
            worldX * detailScale,
            worldZ * detailScale,
            seed + 100u,
            std::max(2, octaves - 1),
            persistence,
            lacunarity
        );

        const float fineDetail =
            (detail - 0.5f) *
            m_settings.heightScale *
            0.055f;

        const float mediumDetail = FractalNoise(
            worldX * detailScale * 0.28f,
            worldZ * detailScale * 0.28f,
            seed + 120u,
            3,
            0.5f,
            2.0f
        );

        const float surfaceVariation =
            (mediumDetail - 0.5f) *
            m_settings.heightScale *
            0.12f;

        const float height =
            m_settings.baseHeight +
            broadTerrain +
            rollingTerrain +
            foothills +
            mountainHeight -
            valleyCut +
            cliffFormation +
            fineDetail +
            surfaceVariation;

        return height;
    }
}