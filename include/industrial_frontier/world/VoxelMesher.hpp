
#pragma once

#include "industrial_frontier/world/TerrainChunk.hpp"
#include "industrial_frontier/world/VoxelTerrain.hpp"

#include <vector>

namespace industrial_frontier
{
    struct VoxelMesh
    {
        std::vector<TerrainVertex> vertices;
        std::vector<unsigned int> indices;

        void Clear()
        {
            vertices.clear();
            indices.clear();
        }

        bool IsEmpty() const
        {
            return indices.empty();
        }
    };

    struct VoxelMesherSettings
    {
        float minimumY = -96.0f;
        float maximumY = 160.0f;
        float spacing = 2.0f;
    };

    class VoxelMesher
    {
    public:
        explicit VoxelMesher(
            const VoxelMesherSettings& settings = {}
        );

        bool GenerateChunk(
            const VoxelTerrain& terrain,
            int chunkX,
            int chunkZ,
            VoxelMesh& output
        ) const;

        const VoxelMesherSettings& GetSettings() const;

    private:
        VoxelMesherSettings m_settings;
    };
}
