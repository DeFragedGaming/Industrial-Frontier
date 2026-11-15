#pragma once

#include "industrial_frontier/resources/BuildStructures.hpp"
#include "industrial_frontier/world/VoxelMesher.hpp"

#include <vector>

namespace industrial_frontier
{
    class BuildStructureVisuals
    {
    public:
        static void Build(const std::vector<BuildStructure>& structures,
            VoxelMesh& mesh);
    };
}
