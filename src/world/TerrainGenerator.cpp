#include "industrial_frontier/world/TerrainGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace industrial_frontier
{
    TerrainGenerator::TerrainGenerator()
        : m_settings()
    {
    }

    TerrainGenerator::TerrainGenerator(
        const TerrainSettings& settings
    )
        : m_settings(settings)
    {
    }

    void TerrainGenerator::SetSettings(
        const TerrainSettings& settings
    )
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
        std::uint32_t seed
    )
    {
        std::uint32_t value =
            static_cast<std::uint32_t>(x) * 374761393u;

        value +=
            static_cast<std::uint32_t>(z) * 668265263u;

        value += seed * 1442695041u;

        value = (value ^ (value >> 13)) * 1274126177u;
        value ^= value >> 16;

        return value;
    }

    float TerrainGenerator::SmoothStep(float value)
    {
        value = std::clamp(value, 0.0f, 1.0f);

        return value * value * (3.0f - 2.0f * value);
    }

    float TerrainGenerator::Interpolate(
        float a,
        float b,
        float t
    )
    {
        return a + (b - a) * t;
    }

    float TerrainGenerator::ValueNoise(
        float x,
        float z,
        std::uint32_t seed
    )
    {
        const int x0 = static_cast<int>(std::floor(x));
        const int z0 = static_cast<int>(std::floor(z));

        const int x1 = x0 + 1;
        const int z1 = z0 + 1;

        const float tx = SmoothStep(x - static_cast<float>(x0));
        const float tz = SmoothStep(z - static_cast<float>(z0));

        const auto randomValue = [seed](int ix, int iz)
        {
            const std::uint32_t hash = Hash(ix, iz, seed);

            return static_cast<float>(hash) /
                static_cast<float>(UINT32_MAX);
        };

        const float a = randomValue(x0, z0);
        const float b = randomValue(x1, z0);
        const float c = randomValue(x0, z1);
        const float d = randomValue(x1, z1);

        const float top = Interpolate(a, b, tx);
        const float bottom = Interpolate(c, d, tx);

        return Interpolate(top, bottom, tz) * 2.0f - 1.0f;
    }

    float TerrainGenerator::FractalNoise(
        float x,
        float z,
        std::uint32_t seed,
        int octaves,
        float persistence,
        float lacunarity
    )
    {
        float amplitude = 1.0f;
        float frequency = 1.0f;

        float total = 0.0f;
        float maximumValue = 0.0f;

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
            maximumValue += amplitude;

            amplitude *= persistence;
            frequency *= lacunarity;
        }

        if (maximumValue <= 0.0f)
        {
            return 0.0f;
        }

        return total / maximumValue;
    }

    float TerrainGenerator::GetHeight(
        float worldX,
        float worldZ
    ) const
    {
        const std::uint32_t seed =
            static_cast<std::uint32_t>(m_settings.seed);

        const float continental = FractalNoise(
            worldX * m_settings.continentalScale,
            worldZ * m_settings.continentalScale,
            seed,
            m_settings.noiseOctaves,
            m_settings.persistence,
            m_settings.lacunarity
        );

        const float mountainNoise = FractalNoise(
            worldX * m_settings.mountainScale,
            worldZ * m_settings.mountainScale,
            seed + 10000u,
            4,
            0.52f,
            2.1f
        );

        const float detail = FractalNoise(
            worldX * m_settings.detailScale,
            worldZ * m_settings.detailScale,
            seed + 20000u,
            3,
            0.5f,
            2.0f
        );

        const float mountainMask = std::clamp(
            (continental + 0.15f) * 1.5f,
            0.0f,
            1.0f
        );

        const float mountainShape =
            1.0f - std::abs(mountainNoise);

        const float mountains =
            mountainShape *
            mountainMask *
            m_settings.heightScale *
            m_settings.mountainStrength;

        const float rollingTerrain =
            continental * m_settings.heightScale * 0.45f;

        const float valleys =
            std::max(0.0f, -continental) *
            m_settings.heightScale *
            m_settings.valleyStrength;

        const float surfaceDetail =
            detail * m_settings.heightScale * 0.08f;

        return m_settings.baseHeight
            + rollingTerrain
            + mountains
            - valleys
            + surfaceDetail;
    }
}