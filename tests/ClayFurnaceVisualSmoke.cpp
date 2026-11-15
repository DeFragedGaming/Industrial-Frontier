#include "industrial_frontier/renderer/ClayFurnaceVisuals.hpp"

#include <iostream>
#include <vector>

using namespace industrial_frontier;

int main()
{
    std::vector<ClayFurnaceState> furnaces(1);
    furnaces[0].id=1;
    furnaces[0].y=5.0f;
    furnaces[0].fuelSeconds=35.0f;
    furnaces[0].readyCopperKg=0.50f;
    VoxelMesh body, fire, smoke, held;
    ClayFurnaceVisuals::BuildBodies(furnaces,body);
    ClayFurnaceVisuals::BuildEffects(furnaces,5.0f,fire,smoke);
    ClayFurnaceVisuals::BuildHeldMold(0,5,0,0,0,-1,true,held);
    if (body.IsEmpty() || fire.IsEmpty() || smoke.IsEmpty() || held.IsEmpty())
    {
        std::cerr << "Generated furnace or mold meshes were empty\n";
        return 1;
    }
    furnaces[0].fuelSeconds=0;
    ClayFurnaceVisuals::BuildEffects(furnaces,5.0f,fire,smoke);
    if (!fire.IsEmpty() || !smoke.IsEmpty())
    {
        std::cerr << "Extinguished furnace still has fire or smoke\n";
        return 1;
    }
    std::cout << "Procedural clay furnace, molten mold, fire and smoke passed\n";
    return 0;
}
