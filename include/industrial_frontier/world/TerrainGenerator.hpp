#pragma once

#include <cstdint>

namespace industrial_frontier
{
    struct TerrainSettings
    {
        int seed = 12345;

        float heightScale = 35.0f;
        float baseHeight = -5.0f;

        float continentalScale = 0.0015f;
        float mountainScale = 0.003f;
        float detailScale = 0.012f;

        int noiseOctaves = 5;
        float persistence = 0.5f;
        float lacunarity = 2.0f;

        float mountainStrength = 1.0f;
        float valleyStrength = 0.65f;
    };

    class TerrainGenerator
    {
    public:
        TerrainGenerator();
        explicit TerrainGenerator(const TerrainSettings& settings);

        void SetSettings(const TerrainSettings& settings);

        const TerrainSettings& GetSettings() const;

        float GetHeight(float worldX, float worldZ) const;

    private:
        TerrainSettings m_settings;

        static std::uint32_t Hash(
            int x,
            int z,
            std::uint32_t seed
        );

        static float SmoothStep(float value);

        static float Interpolate(
            float a,
            float b,
            float t
        );

        static float ValueNoise(
            float x,
            float z,
            std::uint32_t seed
        );

        static float FractalNoise(
            float x,
            float z,
            std::uint32_t seed,
            int octaves,
            float persistence,
            float lacunarity
        );
    };
}