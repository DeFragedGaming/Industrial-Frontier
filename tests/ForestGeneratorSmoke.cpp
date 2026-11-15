#include "industrial_frontier/forest/ForestGenerator.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <unordered_set>
#include <vector>

namespace
{
    bool Check(bool condition, const char* description)
    {
        if (!condition)
        {
            std::cerr << "ForestGenerator test failed: " << description << '\n';
        }
        return condition;
    }
}

int main()
{
    using namespace industrial_frontier;

    TerrainSettings settings{};
    VoxelTerrain terrain(settings);
    std::unordered_set<std::uint64_t> ids;
    std::size_t count = 0;
    std::size_t pines = 0;
    std::size_t broadleaf = 0;
    std::size_t birch = 0;
    bool valid = true;

    for (int z = -10; z <= 10; ++z)
    {
        for (int x = -10; x <= 10; ++x)
        {
            std::vector<ForestTree> a;
            std::vector<ForestTree> b;
            ForestGenerator::GenerateTrees(terrain, x, z, a);
            ForestGenerator::GenerateTrees(terrain, x, z, b);
            valid &= Check(a.size() == b.size(), "nondeterministic tree count");

            VoxelMesh mesh;
            ForestGenerator::BuildMesh(a, mesh);
            if (a.empty())
            {
                valid &= Check(mesh.IsEmpty(), "empty forest produced mesh");
            }
            else
            {
                valid &= Check(!mesh.IsEmpty(), "nonempty forest has no triangles");
                valid &= Check(mesh.indices.size() == mesh.vertices.size(), "index count mismatch");
                for (unsigned int index : mesh.indices)
                {
                    valid &= Check(index < mesh.vertices.size(), "invalid vertex index");
                }
            }

            for (std::size_t i = 0; i < a.size(); ++i)
            {
                const auto& tree = a[i];
                valid &= Check(tree.id == b[i].id &&
                    tree.x == b[i].x && tree.y == b[i].y &&
                    tree.z == b[i].z && tree.species == b[i].species,
                    "nondeterministic tree values");
                valid &= Check(tree.x >= x * 32.0f && tree.x < (x + 1) * 32.0f &&
                    tree.z >= z * 32.0f && tree.z < (z + 1) * 32.0f,
                    "tree owned by wrong chunk");
                valid &= Check(ids.insert(tree.id).second, "duplicate ID across chunks");
                valid &= Check(terrain.IsSolid(tree.x, tree.y - 0.5f, tree.z),
                    "tree not rooted in solid ground");
                valid &= Check(!terrain.IsSolid(tree.x, tree.y + 1.6f, tree.z),
                    "tree spawned inside rock");
                valid &= Check(terrain.GetEntranceRockWeight(tree.x, tree.z) < 0.075f,
                    "tree intersects cave entrance influence");
                const float surfaceY = terrain.GetSurfaceHeight(tree.x, tree.z);
                const float sx = (terrain.GetSurfaceHeight(tree.x + 2.25f, tree.z) -
                    terrain.GetSurfaceHeight(tree.x - 2.25f, tree.z)) / 4.5f;
                const float sz = (terrain.GetSurfaceHeight(tree.x, tree.z + 2.25f) -
                    terrain.GetSurfaceHeight(tree.x, tree.z - 2.25f)) / 4.5f;
                const float ny = 1.0f / std::sqrt(1.0f + sx * sx + sz * sz);
                valid &= Check(terrain.GetMaterialAt(tree.x, surfaceY - 0.12f,
                    tree.z, ny) == TerrainMaterial::Grass,
                    "tree spawned on rock or dirt rather than grass");
                for (int corner = 0; corner < 4; ++corner)
                {
                    const float px = tree.x + (corner & 1 ? 1.15f : -1.15f);
                    const float pz = tree.z + (corner & 2 ? 1.15f : -1.15f);
                    const float py = terrain.GetSurfaceHeight(px, pz);
                    valid &= Check(terrain.GetMaterialAt(px, py - 0.12f,
                        pz, ny) == TerrainMaterial::Grass,
                        "tree roots overlap rocky terrain");
                }
                ++count;
                if (tree.species == TreeSpecies::Pine) ++pines;
                if (tree.species == TreeSpecies::Broadleaf) ++broadleaf;
                if (tree.species == TreeSpecies::Birch) ++birch;
            }
        }
    }

    std::cout << "Forest smoke: " << count << " trees, " << pines
        << " pine, " << broadleaf << " broadleaf, " << birch << " birch.\n";
    TerrainSettings alternateSettings = settings;
    alternateSettings.seed = 482917;
    VoxelTerrain alternateTerrain(alternateSettings);
    valid &= Check(std::abs(terrain.GetSurfaceHeight(156.0f, -172.0f) -
        alternateTerrain.GetSurfaceHeight(156.0f, -172.0f)) > 0.01f,
        "different world seeds generated the same sampled terrain");
    std::vector<ForestTree> oldForest;
    std::vector<ForestTree> newForest;
    ForestGenerator::GenerateTrees(terrain, 4, 4, oldForest);
    ForestGenerator::GenerateTrees(alternateTerrain, 4, 4, newForest);
    valid &= Check(oldForest.size() != newForest.size() ||
        (!oldForest.empty() && !newForest.empty() &&
         (oldForest.front().x != newForest.front().x ||
          oldForest.front().z != newForest.front().z)) ||
        terrain.GetSurfaceHeight(156.0f, -172.0f) !=
          alternateTerrain.GetSurfaceHeight(156.0f, -172.0f),
        "world seed did not affect procedural generation");
    valid &= Check(count >= 10, "far too few trees in grassy forest regions");
    valid &= Check(pines > 0 && broadleaf > 0 && birch > 0,
        "missing a tree variant");
    return valid ? 0 : 1;
}
