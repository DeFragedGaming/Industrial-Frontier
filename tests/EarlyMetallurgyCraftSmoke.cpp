#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"

#include <iostream>
#include <string>

using namespace industrial_frontier;

int main()
{
    PlayerInventory inventory;
    std::string error;
    if (!inventory.AddItem("clay_soil", 28) ||
        !inventory.AddItem("raw_stone", 14) ||
        !inventory.AddItem("stick", 6) ||
        !inventory.AddItem("log", 4) ||
        !inventory.Craft("furnace_kit_recipe", error) ||
        inventory.ItemCount("furnace_kit") != 1 ||
        !inventory.Craft("ingot_mold_recipe", error) ||
        inventory.ItemCount("ingot_mold") != 1)
    {
        std::cerr << "Crafting furnace/mold failed: " << error << '\n';
        return 1;
    }
    if (!inventory.RemoveItem("ingot_mold",1) ||
        !inventory.AddItem("hot_copper_mold",1) ||
        !inventory.RemoveItem("hot_copper_mold",1) ||
        !inventory.AddItem("cooled_copper_mold",1) ||
        !inventory.RemoveItem("cooled_copper_mold",1) ||
        !inventory.AddItem("copper_ingot",1) ||
        !inventory.AddItem("ingot_mold",1) ||
        !PlayerInventory::IsValidState(inventory.Capture()))
    {
        std::cerr << "Mold transitions failed\n";
        return 1;
    }
    std::cout << "Furnace kit, mold and copper ingot inventory passed\n";
    return 0;
}
