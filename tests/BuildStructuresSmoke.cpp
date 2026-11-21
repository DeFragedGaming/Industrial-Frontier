#include "industrial_frontier/resources/BuildStructures.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

using namespace industrial_frontier;

namespace
{
    void Require(bool value, const char* label)
    {
        if (!value)
        {
            std::cerr << "FAIL: " << label << '\n';
            std::exit(1);
        }
    }
}

int main()
{
    Require(PrimitiveRecipes.size() >= 17, "building recipes registered");
    for (int kind = 0; kind <= static_cast<int>(BuildKind::Support); ++kind)
    {
        const auto next = static_cast<BuildKind>(kind);
        BuildKind resolved{};
        Require(BuildKitKind(BuildKitId(next), resolved) && resolved == next,
            "build kit mapping");
        Require(FindPrimitiveItem(BuildKitId(next)) != nullptr,
            "build kit item definition");
    }
    BuildWorld world;
    const auto floor = world.Place(BuildKind::Foundation,0.0f,10.0f,0.0f,0.0f);
    Require(floor > 0, "place foundation");
    Require(world.NearestFoundation(2.0f,0.0f) != nullptr,
        "find foundation for snapping");
    Require(world.Place(BuildKind::Foundation,0.0f,10.0f,0.0f,0.0f)==0,
        "reject duplicate foundation");
    const auto frame = world.Place(BuildKind::Doorframe,0.0f,10.0f,1.5f,0.0f);
    Require(frame>0,"place doorframe");
    Require(world.Place(BuildKind::Wall,0.0f,10.0f,1.5f,0.0f)==0,
        "doorframe protects socket");
    const auto chest = world.Place(BuildKind::Chest,4.0f,10.0f,0.0f,90.0f);
    Require(chest>0,"place chest");
    PlayerInventory inventory;
    Require(inventory.SetCarryCapacityBonus(900.0f),"chest capacity");
    Require(inventory.AddItem("log",105),"105 logs fit chest");
    Require(world.UpdateChest(chest,inventory.Capture()),"store contents");
    Require(!world.Remove(chest),"chest cannot dismantle with items");
    const auto state = world.Structures();
    Require(BuildWorld::Validate(state),"valid save data");
    BuildWorld loaded;
    Require(loaded.Restore(state),"restore builds");
    Require(loaded.Find(chest)->contents.items[0].quantity==105,
        "persistent 105 logs");
    Require(loaded.UpdateChest(chest,InventoryState{900.0f,{},{}}),
        "remove items safely");
    Require(loaded.Remove(chest),"empty chest dismantles");
    Require(loaded.Find(chest)==nullptr,"removed chest gone");
    std::cout << "Stage 10G construction, chest inventory, and save state passed\n";
}
