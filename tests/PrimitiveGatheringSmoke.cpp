#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"

#include <cassert>
#include <string>

int main()
{
    using namespace industrial_frontier;
    PlayerInventory inventory;
    std::string message;
    assert(inventory.AddItem("stick", 6));
    assert(inventory.AddItem("pebble", 6));
    assert(inventory.UsedSlots() == 2);
    assert(inventory.Craft("stone_axe_recipe", message));
    assert(inventory.ItemCount("stone_axe") == 1);
    assert(inventory.ItemCount("stick") == 4);
    assert(inventory.Craft("stone_pickaxe_recipe", message));
    assert(inventory.ItemCount("stone_pickaxe") == 1);
    assert(inventory.ItemCount("pebble") == 1);
    assert(inventory.Craft("stone_shovel_recipe", message));
    assert(inventory.ItemCount("stone_shovel") == 1);
    assert(inventory.ItemCount("stick") == 0);
    assert(inventory.ItemCount("pebble") == 0);
    assert(!inventory.Craft("stone_axe_recipe", message));
    assert(PlayerInventory::IsValidState(inventory.Capture()));
    PlayerInventory restored;
    assert(restored.Restore(inventory.Capture()));
    assert(restored.ItemCount("stone_axe") == 1);
    assert(restored.TotalWeightKg() > 0.0f);
    assert(restored.RemoveItem("stone_axe", 1));
    assert(restored.ItemCount("stone_axe") == 0);
    return 0;
}
