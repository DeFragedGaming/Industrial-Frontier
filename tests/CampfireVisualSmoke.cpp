#include "industrial_frontier/renderer/CampfireVisuals.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace industrial_frontier;

namespace
{
    void Require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Campfire visuals test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    const std::vector<CampfireState> fires{{1, 3.0f, 5.0f, 8.0f, 45.0f, 0.0f, 0}};
    VoxelMesh pit, flame, smoke;
    CampfireVisuals::BuildPit(fires, pit);
    Require(!pit.IsEmpty() && pit.vertices.size() < 4000, "pit geometry");
    CampfireVisuals::BuildParticles(fires, 2.0f, flame, smoke);
    Require(!flame.IsEmpty() && !smoke.IsEmpty(), "open fire and smoke geometry");
    for (const auto* geometry : {&pit, &flame, &smoke})
    {
        for (const auto& vertex : geometry->vertices)
        {
            for (float p : vertex.position)
                Require(std::isfinite(p), "finite mesh coordinates");
        }
    }
    auto covered = fires;
    covered.front().covered = true;
    CampfireVisuals::BuildParticles(covered, 2.0f, flame, smoke);
    Require(flame.IsEmpty() && !smoke.IsEmpty(), "covered fire smokes without flames");
    CampfireVisuals::BuildPit(covered, pit);
    Require(!pit.IsEmpty(), "covered pit geometry");
    auto unlit = fires;
    unlit.front().fuelSeconds = 0.0f;
    CampfireVisuals::BuildParticles(unlit, 3.0f, flame, smoke);
    Require(flame.IsEmpty() && smoke.IsEmpty(), "extinguished fire has no particles");
    std::cout << "Campfire model, animated flames and smoke geometry passed\n";
    return EXIT_SUCCESS;
}
