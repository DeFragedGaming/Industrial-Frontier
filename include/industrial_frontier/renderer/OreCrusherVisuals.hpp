#pragma once

#include "industrial_frontier/resources/OreCrusher.hpp"
#include "industrial_frontier/world/VoxelMesher.hpp"

#include <vector>

namespace industrial_frontier
{
    class OreCrusherVisuals
    {
    public:
        static void Build(const std::vector<OreCrusherState>& crushers, VoxelMesh& mesh);
    };
}
