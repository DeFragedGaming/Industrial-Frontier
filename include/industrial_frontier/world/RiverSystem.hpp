#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace industrial_frontier
{
    struct RiverSample
    {
        float centerX = 0.0f;
        float distance = 10000.0f;
        float halfWidth = 0.0f;
        float flowX = 0.0f;
        float flowZ = 1.0f;
        float speed = 0.0f;
        float signedDistance = -10000.0f;
        float lakeCenterX = 0.0f;
        float lakeCenterZ = 0.0f;
        float lakeInfluence = 0.0f;
        bool inWater = false;
        bool isLake = false;
        bool isTributary = false;
    };

    class RiverSystem
    {
    public:
        static RiverSample Sample(float x, float z, int seed)
        {
            RiverSample best;
            if (!std::isfinite(x) || !std::isfinite(z))
                return best;

            constexpr float mainSpacing = 620.0f;
            constexpr float branchPeriod = 760.0f;
            constexpr float lakePeriod = 1680.0f;
            const std::uint32_t bits = static_cast<std::uint32_t>(seed);
            const float phase = static_cast<float>(bits % 1777u) * 0.0081f;
            const float origin = (static_cast<float>(Mix(bits + 17u) % 141u) - 70.0f);
            const float warp = MainDrift(z, phase);
            const int nearLane = static_cast<int>(std::floor((x - origin - warp) / mainSpacing + 0.5f));
            const float nearestMain = origin + static_cast<float>(nearLane) * mainSpacing + warp;
            if (std::abs(x - nearestMain) > 210.0f)
            {
                best.centerX = nearestMain;
                best.halfWidth = 8.0f;
                best.distance = std::abs(x - nearestMain);
                best.signedDistance = best.halfWidth - best.distance;
                return best;
            }
            const int branchBlock = static_cast<int>(std::floor(z / branchPeriod));
            const int lakeBlock = static_cast<int>(std::floor(z / lakePeriod));
            float nearestLake = -1.0f;

            for (int lane = nearLane; lane <= nearLane; ++lane)
            {
                const float center = origin + static_cast<float>(lane) * mainSpacing + warp;
                const float derivative = MainDerivative(z, phase);
                const std::uint32_t laneHash = Mix(bits ^ Mix(static_cast<std::uint32_t>(lane) * 0x9e3779b9u));
                const float riverWidth = 6.8f + static_cast<float>((laneHash >> 8) % 37u) * 0.10f;
                const float signedMain = riverWidth - std::abs(x - center);
                const float baseSpeed = 0.82f + 0.18f * std::abs(std::sin(z * 0.009f + phase + lane));
                Consider(best, signedMain, center, riverWidth, derivative, baseSpeed, false, false);

                for (int branch = 0; branch < 2; ++branch)
                {
                    const std::uint32_t branchHash = Mix(laneHash ^ Mix(static_cast<std::uint32_t>(branchBlock) * 0x85ebca6bu + static_cast<std::uint32_t>(branch) * 0xc2b2ae35u));
                    const float jitter = (static_cast<float>((branchHash >> 4) & 255u) / 255.0f - 0.5f) * 72.0f;
                    const float join = static_cast<float>(branchBlock) * branchPeriod +
                        (branch == 0 ? 350.0f : 682.0f) + jitter;
                    const float length = (branch == 0 ? 292.0f : 348.0f) +
                        static_cast<float>((branchHash >> 13) & 31u);
                    const float t = (z - (join - length)) / length;
                    if (t < 0.0f || t > 1.0f)
                        continue;

                    const float side = ((branchHash & 1u) == 0u ? 1.0f : -1.0f);
                    const float reach = 115.0f + static_cast<float>((branchHash >> 19) & 63u);
                    const float bend = 20.0f + static_cast<float>((branchHash >> 9) & 15u);
                    const float twopi = 6.2831853071795864769f;
                    const float pi = 3.14159265358979323846f;
                    const float wave = std::sin(twopi * t * 1.5f) * std::sin(pi * t);
                    const float waveDerivative =
                        (twopi * 1.5f * std::cos(twopi * t * 1.5f) * std::sin(pi * t) +
                         pi * std::sin(twopi * t * 1.5f) * std::cos(pi * t)) / length;
                    const float branchOffset = side * (reach * (1.0f - t) * (1.0f - t) + bend * wave);
                    const float branchDerivative = derivative + side *
                        (-2.0f * reach * (1.0f - t) / length + bend * waveDerivative);
                    const float branchCenter = center + branchOffset;
                    const float birth = Smooth(std::min(1.0f, t * 7.0f));
                    const float width = (2.6f + 0.9f * t) * birth;
                    Consider(best, width - std::abs(x - branchCenter), branchCenter,
                        width, branchDerivative, 0.48f + 0.28f * t, false, true);
                }

                if ((branchBlock & 1) == 0)
                {
                    const float base = static_cast<float>(branchBlock) * branchPeriod;
                    const float split = base + 95.0f;
                    const float rejoin = base + 610.0f;
                    const float t = (z - split) / (rejoin - split);
                    if (t >= 0.0f && t <= 1.0f)
                    {
                        const float pi = 3.14159265358979323846f;
                        const float side = ((laneHash & 4u) == 0u ? 1.0f : -1.0f);
                        const float amplitude = 46.0f + static_cast<float>((laneHash >> 14) & 31u);
                        const float offset = side * amplitude * std::sin(pi * t);
                        const float slope = derivative + side * amplitude * pi *
                            std::cos(pi * t) / (rejoin - split);
                        Consider(best, 2.8f - std::abs(x - (center + offset)),
                            center + offset, 2.8f, slope, 0.41f, false, true);
                    }
                }

                for (int lakeIndex = lakeBlock; lakeIndex <= lakeBlock; ++lakeIndex)
                {
                    const std::uint32_t lakeHash = Mix(laneHash ^ Mix(static_cast<std::uint32_t>(lakeIndex) * 0x27d4eb2du + 0x514e0bdfu));
                    const float lakeZ = static_cast<float>(lakeIndex) * lakePeriod + 420.0f +
                        (static_cast<float>((lakeHash >> 7) & 255u) / 255.0f - 0.5f) * 220.0f;
                    const float lakeX = origin + static_cast<float>(lane) * mainSpacing + MainDrift(lakeZ, phase);
                    const float radiusX = 28.0f + static_cast<float>((lakeHash >> 16) & 31u) * 0.65f;
                    const float radiusZ = 75.0f + static_cast<float>((lakeHash >> 23) & 31u) * 1.25f;
                    const float dx = (x - lakeX) / radiusX;
                    const float dz = (z - lakeZ) / radiusZ;
                    if (std::abs(dx) > 1.8f || std::abs(dz) > 1.8f)
                        continue;
                    const float angle = std::atan2(dz, dx);
                    const float irregular = 1.0f + 0.065f * std::sin(5.0f * angle + phase) +
                        0.037f * std::sin(9.0f * angle + phase * 1.7f);
                    const float normalized = std::sqrt(dx * dx + dz * dz) / irregular;
                    const float lakeSigned = (1.0f - normalized) * radiusX;
                    const float influence = 1.0f - Smooth((normalized - 1.0f) / 0.65f);
                    if (influence > nearestLake)
                    {
                        nearestLake = influence;
                        best.lakeCenterX = lakeX;
                        best.lakeCenterZ = lakeZ;
                        best.lakeInfluence = std::clamp(influence, 0.0f, 1.0f);
                    }
                    Consider(best, lakeSigned, center, radiusX, derivative, 0.055f,
                        true, false);
                }
            }

            best.inWater = best.signedDistance > 0.0f;
            if (!best.isLake && best.lakeInfluence > 0.65f)
                best.speed *= 1.0f - 0.76f * Smooth((best.lakeInfluence - 0.65f) / 0.35f);
            return best;
        }

        static bool HasFlow(float x, float z, int seed, float minSpeed = 0.4f)
        {
            const RiverSample r = Sample(x, z, seed);
            return r.inWater && !r.isLake && r.speed >= minSpeed;
        }

    private:
        static std::uint32_t Mix(std::uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            return value ^ (value >> 16);
        }

        static float Smooth(float t)
        {
            t = std::clamp(t, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        static float MainDrift(float z, float phase)
        {
            return 85.0f * std::sin(z * 0.0041f + phase) +
                42.0f * std::sin(z * 0.0093f + phase * 1.71f) +
                18.0f * std::sin(z * 0.0019f + phase * 0.42f);
        }

        static float MainDerivative(float z, float phase)
        {
            return 0.3485f * std::cos(z * 0.0041f + phase) +
                0.3906f * std::cos(z * 0.0093f + phase * 1.71f) +
                0.0342f * std::cos(z * 0.0019f + phase * 0.42f);
        }

        static void Consider(RiverSample& result, float signedDistance,
            float centerX, float halfWidth, float derivative, float speed,
            bool lake, bool tributary)
        {
            if (signedDistance <= result.signedDistance)
                return;
            const float inv = 1.0f / std::sqrt(1.0f + derivative * derivative);
            result.signedDistance = signedDistance;
            result.distance = std::max(0.0f, halfWidth - signedDistance);
            result.centerX = centerX;
            result.halfWidth = halfWidth;
            result.flowX = derivative * inv;
            result.flowZ = inv;
            result.speed = speed;
            result.isLake = lake;
            result.isTributary = tributary;
        }
    };
}
