#pragma once

#include "industrial_frontier/resources/Campfire.hpp"
#include "industrial_frontier/world/VoxelMesher.hpp"

#include <vector>

namespace industrial_frontier
{
    class CampfireVisuals
    {
    public:
        static void BuildPit(const std::vector<CampfireState>& fires,
            VoxelMesh& output);
        static void BuildParticles(const std::vector<CampfireState>& fires,
            float time, VoxelMesh& flame, VoxelMesh& smoke);
    };
}
