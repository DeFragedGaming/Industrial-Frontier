#pragma once

#include "industrial_frontier/world/TerrainGenerator.hpp"
#include "industrial_frontier/world/VoxelMesher.hpp"

namespace industrial_frontier
{
    class RiverVisuals
    {
    public:
        static void BuildChunk(const TerrainGenerator& terrain,
            int chunkX, int chunkZ, float chunkSize, VoxelMesh& mesh);
    };
}
