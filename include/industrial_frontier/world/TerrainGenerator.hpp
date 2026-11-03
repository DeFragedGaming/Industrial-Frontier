#pragma once

#include <cstdint>

namespace industrial_frontier
{
    struct TerrainSettings
    {
        int seed = 12345;

        float heightScale = 35.0f;
        float baseHeight = -5.0f;

        float continentalScale = 0.0012f;
        float mountainScale = 0.0022f;
        float detailScale = 0.012f;

        int noiseOctaves = 5;
        float persistence = 0.5f;
        float lacunarity = 2.0f;

        float mountainStrength = 1.0f;
        float valleyStrength = 0.65f;

        float plainsStrength = 1.0f;
        float mountainRangeStrength = 1.0f;
        float foothillStrength = 1.0f;

        float canyonStrength = 1.0f;
        float canyonScale = 0.0018f;
        float canyonWidth = 0.16f;
        float canyonDepth = 22.0f;

        float valleyScale = 0.0010f;
        float valleyWidth = 0.30f;
        float valleyDepth = 12.0f;

        float plateauStrength = 0.45f;
        float detailStrength = 1.0f;

        float mountainHeight = 58.0f;
        float plainsFlatness = 0.82f;
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