#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/ui/ItemLayout.hpp"
#include <cassert>
#include <string>

int main()
{
    using namespace industrial_frontier;
    PlayerInventory player;
    PlayerInventory storage;
    assert(storage.SetCarryCapacityBonus(900.0f));
    assert(player.AddItem("stone_axe", 1));
    assert(player.AddItem("stone_pickaxe", 1));
    assert(player.AddItem("pebble", 3));
    ItemLayout layout;
    layout.Reconcile(player.Capture(), storage.Capture());
    assert(layout.Bind(0, "item:stone_axe", player.Capture()));
    layout.Reconcile(player.Capture(), storage.Capture());
    for (const auto& id : layout.bag)
        assert(id != "item:stone_axe");
    assert(layout.bar[0] == "stone_axe");
    assert(player.HasUsableTool("stone_axe"));
    bool broke = false;
    assert(player.UseTool("stone_axe", 17, broke));
    assert(!broke);
    InventoryState state = player.Capture();
    for (const auto& stack : state.items)
        if (stack.itemId == "stone_axe") assert(stack.durability == 83);
    assert(storage.AddItemStack(state.items[0]));
    assert(storage.Capture().items.front().durability == 83);
    PlayerInventory reloaded;
    assert(reloaded.Restore(state));
    assert(reloaded.UseTool("stone_axe", 83, broke));
    assert(broke);
    assert(!reloaded.HasUsableTool("stone_axe"));
    layout.Reconcile(reloaded.Capture(), storage.Capture());
    assert(layout.bar[0].empty());
    InventoryState legacy = state;
    for (auto& item : legacy.items)
        if (item.itemId == "stone_axe") item.durability = 0;
    assert(reloaded.Restore(legacy));
    assert(reloaded.HasUsableTool("stone_axe"));
    return 0;
}
