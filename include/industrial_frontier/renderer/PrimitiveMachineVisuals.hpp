#pragma once

#include "industrial_frontier/resources/PrimitiveProcessingMachines.hpp"
#include "industrial_frontier/world/VoxelMesher.hpp"

namespace industrial_frontier
{
    class PrimitiveMachineVisuals
    {
    public:
        static void Build(const std::vector<PrimitiveMachineState>& machines,
            VoxelMesh& mesh);
    };
}
