#include "industrial_frontier/player/StructureCollision.hpp"
#include "industrial_frontier/resources/Campfire.hpp"
#include "industrial_frontier/resources/PrimitiveTier.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace industrial_frontier;

namespace
{
    void Require(bool condition, const char* message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        std::vector<StructureCollider> props{{2.0f, 1.0f, 3.0f, 0.89f, 2.08f},
            {-3.0f, 0.0f, 0.0f, 0.73f, 0.70f}};
        Require(StructureBlocksPoint(&props, 2.0f, 1.5f, 3.0f, 0.1f),
            "furnace center blocks movement");
        Require(StructureBlocksPoint(&props, 2.95f, 1.5f, 3.0f, 0.1f),
            "player capsule receives radius padding");
        Require(!StructureBlocksPoint(&props, 4.0f, 1.5f, 3.0f, 0.1f),
            "outside furnace does not block");
        Require(!StructureBlocksPoint(&props, 2.0f, 3.3f, 3.0f, 0.1f),
            "above furnace is clear");
        Require(StructureBlocksPoint(&props, -3.0f, 0.25f, 0.0f, 0.1f),
            "campfire ring blocks movement");
        Require(!StructureBlocksPoint(nullptr, 2.0f, 1.5f, 3.0f, 0.1f),
            "no collider list does not block");

        Require(!CanExcavateTerrain("",true) && !CanExcavateTerrain("",false),
            "bare hand never excavates");
        Require(!CanExcavateTerrain("stone_axe",true) &&
            !CanExcavateTerrain("stone_axe",false),
            "axe does not dig terrain");
        Require(CanExcavateTerrain("stone_pickaxe",true) &&
            !CanExcavateTerrain("stone_pickaxe",false),
            "stone pick mines rock only");
        Require(CanExcavateTerrain("copper_pickaxe",true) &&
            !CanExcavateTerrain("copper_pickaxe",false),
            "copper pick mines rock only");
        Require(CanExcavateTerrain("stone_shovel",false) &&
            !CanExcavateTerrain("stone_shovel",true),
            "stone shovel digs soil only");
        Require(CanExcavateTerrain("copper_shovel",false) &&
            !CanExcavateTerrain("copper_shovel",true),
            "copper shovel digs soil only");

        CampfireWorld fires;
        Require(fires.Place(0.0f,0.0f,0.0f,135.0f),
            "campfire placement with rotation");
        Require(fires.Fires().size() == 1 &&
            std::abs(fires.Fires()[0].yawDegrees - 135.0f) < 0.001f,
            "placed orientation retained");
        Require(!fires.Place(0.0f,0.0f,0.0f,45.0f),
            "overlapping placeables blocked");
        Require(!fires.Place(4.0f,0.0f,0.0f,360.0f),
            "invalid rotation rejected");
        CampfireWorld loaded;
        Require(loaded.Restore(fires.Fires()),
            "rotated campfire survives world restore");
        Require(loaded.Fires()[0].yawDegrees == 135.0f,
            "restored campfire has saved orientation");
        Require(loaded.Remove(loaded.Fires()[0].id),
            "cold empty campfire can be dismantled");
        std::cout << "Stage 10G structure collision and rotated campfire passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "Stage 10G structure test failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
