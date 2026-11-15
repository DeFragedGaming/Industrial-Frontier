#include "industrial_frontier/resources/HotbarEquipment.hpp"

#include <iostream>
#include <string>

int main()
{
    using namespace industrial_frontier;
    PlayerInventory inventory;
    if (IsEquippableHotbarItem(inventory, "campfire_kit"))
        return 1;
    if (!inventory.AddItem("campfire_kit", 1))
        return 2;
    if (!IsEquippableHotbarItem(inventory, "campfire_kit"))
        return 3;
    if (!inventory.AddItem("stone_axe", 1))
        return 4;
    if (!IsEquippableHotbarItem(inventory, "stone_axe"))
        return 5;
    if (!inventory.AddItem("stick", 1))
        return 6;
    if (IsEquippableHotbarItem(inventory, "stick"))
        return 7;
    if (IsEquippableHotbarItem(inventory, "unknown"))
        return 8;
    if (!inventory.RemoveItem("campfire_kit", 1))
        return 9;
    if (IsEquippableHotbarItem(inventory, "campfire_kit"))
        return 10;
    bool broke = false;
    if (!inventory.UseTool("stone_axe", 100, broke) || !broke)
        return 11;
    if (IsEquippableHotbarItem(inventory, "stone_axe"))
        return 12;
    std::cout << "Hotbar equipment smoke test passed\n";
    return 0;
}
