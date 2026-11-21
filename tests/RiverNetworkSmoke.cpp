#include "industrial_frontier/renderer/RiverVisuals.hpp"
#include "industrial_frontier/world/RiverSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace industrial_frontier;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }
}

int main()
{
    TerrainSettings settings;
    settings.seed = 16493;
    TerrainGenerator terrain(settings);
    bool sawMain = false;
    bool sawTributary = false;
    bool sawLake = false;
    bool sawDry = false;
    bool sawFlow = false;
    int confluences = 0;
    int waterPolygons = 0;
    float firstMainX = 0.0f;
    float lastMainX = 0.0f;
    int mainPositions = 0;
    for (int z = -1680; z <= 1680; z += 12)
    {
        const auto candidate = RiverSystem::Sample(0.0f, static_cast<float>(z), settings.seed);
        if (candidate.centerX != 0.0f)
        {
            if (mainPositions == 0)firstMainX = candidate.centerX;
            lastMainX = candidate.centerX;
            ++mainPositions;
        }
        for (int x = -300; x <= 300; x += 5)
        {
            const auto river = RiverSystem::Sample(static_cast<float>(x), static_cast<float>(z), settings.seed);
            if (!river.inWater)
            {
                sawDry = true;
                continue;
            }
            if (river.isLake)
            {
                Require(river.speed < 0.4f, "lakes are calm and cannot power sluices");
                const float neighborX = static_cast<float>(x) + 0.5f;
                const RiverSample neighbor = RiverSystem::Sample(neighborX,static_cast<float>(z),settings.seed);
                if (neighbor.isLake && neighbor.inWater)
                    Require(std::abs(terrain.GetRiverSurfaceHeight(static_cast<float>(x),static_cast<float>(z)) -
                        terrain.GetRiverSurfaceHeight(neighborX,static_cast<float>(z))) < 0.01f,
                        "lake surface level is uniform");
            }
            sawLake |= river.isLake;
            sawTributary |= river.isTributary;
            sawMain |= !river.isTributary && !river.isLake;
            sawFlow |= RiverSystem::HasFlow(static_cast<float>(x), static_cast<float>(z), settings.seed);
            if (!river.isLake && river.speed >= 0.4f)
            {
                float surface = 0.0f, fx = 0.0f, fz = 0.0f, speed = 0.0f;
                Require(terrain.GetRiverWater(static_cast<float>(x), static_cast<float>(z),surface,fx,fz,speed),
                    "flowing water reports surface");
                Require(std::isfinite(surface), "water heights are finite");
                Require(std::abs(fx * fx + fz * fz - 1.0f) < 0.002f,
                    "flow direction normalized");
            }
        }
    }
    Require(sawMain && sawTributary && sawLake && sawDry && sawFlow,
        "seed generates winding rivers tributaries lakes and dry land");
    Require(mainPositions > 50 && std::abs(firstMainX - lastMainX) > 10.0f,
        "main rivers change direction across landscape");

    for (int z = -500; z < 500; z += 20)
    {
        int previous = 0;
        for (int x = -220; x <= 220; x += 2)
        {
            const auto river = RiverSystem::Sample(static_cast<float>(x),static_cast<float>(z),settings.seed);
            const int kind = river.inWater ? (river.isTributary ? 2 : river.isLake ? 3 : 1) : 0;
            if (previous == 2 && kind == 1)
                ++confluences;
            previous = kind;
        }
    }
    VoxelMesh water;
    for (int cz = -7; cz < 7; ++cz)
    {
        for (int cx = -7; cx < 7; ++cx)
        {
            RiverVisuals::BuildChunk(terrain,cx,cz,32.0f,water);
            Require(water.indices.size() % 3 == 0, "triangles valid");
            for (const auto& vertex : water.vertices)
            {
                Require(std::isfinite(vertex.position[1]), "water vertex elevation finite");
                Require(vertex.normal[1] >= 0.0f, "current speed nonnegative");
            }
            waterPolygons += static_cast<int>(water.indices.size() / 3);
        }
    }
    Require(waterPolygons > 100, "rendered river network has surface geometry");
    const auto stable = RiverSystem::Sample(14.0f,116.0f,settings.seed);
    const auto stableAgain = RiverSystem::Sample(14.0f,116.0f,settings.seed);
    Require(stable.signedDistance == stableAgain.signedDistance &&
        stable.flowX == stableAgain.flowX, "same seed yields identical rivers");
    const auto different = RiverSystem::Sample(14.0f,116.0f,settings.seed + 1);
    Require(stable.centerX != different.centerX, "other seed changes river network");
    std::cout << "Stage 10H-2 river network: passed (" << waterPolygons << " water triangles)\n";
}
