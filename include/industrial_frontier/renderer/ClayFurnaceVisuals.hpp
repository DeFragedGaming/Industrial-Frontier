#pragma once

#include "industrial_frontier/resources/EarlyMetallurgy.hpp"
#include "industrial_frontier/world/VoxelMesher.hpp"

#include <vector>

namespace industrial_frontier
{
    class ClayFurnaceVisuals
    {
    public:
        static void BuildBodies(const std::vector<ClayFurnaceState>& furnaces,
            VoxelMesh& output);
        static void BuildEffects(const std::vector<ClayFurnaceState>& furnaces,
            float time, VoxelMesh& flames, VoxelMesh& smoke);
        static void BuildHeldMold(float x, float y, float z,
            float forwardX, float forwardY, float forwardZ,
            bool hot, VoxelMesh& mesh);
    };
}
