#include "industrial_frontier/ui/ItemLayout.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"
#include <cassert>
#include <string>

int main()
{
    using namespace industrial_frontier;
    PlayerInventory bag;
    PlayerInventory chest;
    assert(chest.SetCarryCapacityBonus(900.0f));
    assert(bag.AddItem("stick", 8));
    assert(bag.AddItem("pebble", 6));
    assert(bag.AddItem("stone_axe", 1));
    ItemLayout layout;
    layout.Reconcile(bag.Capture(), chest.Capture());
    assert(layout.bag[0] == "item:stick");
    assert(layout.bag[1] == "item:pebble");
    assert(layout.bag[2] == "item:stone_axe");
    ItemLayout::Swap(layout.bag, 0, 23);
    assert(layout.bag[23] == "item:stick");
    assert(layout.Bind(3, "item:stone_axe", bag.Capture()));
    assert(layout.bar[3] == "stone_axe");
    assert(layout.selected == 3);
    layout.Reconcile(bag.Capture(), chest.Capture());
    for (const auto& cell : layout.bag)
        assert(cell != "item:stone_axe");
    assert(!layout.Bind(4, "ore:invalid", bag.Capture()));
    assert(chest.AddItem("pebble", 6));
    assert(bag.RemoveItem("pebble", 6));
    layout.Reconcile(bag.Capture(), chest.Capture());
    assert(ItemLayout::Resolve(bag.Capture(), "item:pebble") == -1);
    assert(ItemLayout::Resolve(chest.Capture(), "item:pebble") == 0);
    assert(!layout.storage[0].empty());
    ItemLayout::Place(layout.storage, "item:pebble", 29);
    assert(layout.storage[29] == "item:pebble");
    layout.Reconcile(bag.Capture(), chest.Capture());
    assert(layout.storage[29] == "item:pebble");
    assert(layout.bag[23] == "item:stick");
    assert(bag.RemoveItem("stone_axe", 1));
    layout.Reconcile(bag.Capture(), chest.Capture());
    assert(layout.bar[3].empty());
    InventoryOreBatch ore;
    ore.depositId = 1001;
    ore.displayName = "Rich Copper Ore";
    ore.primaryMineralId = "chalcopyrite";
    ore.massKg = 3.0f;
    ore.oreMineralFraction = 0.8f;
    ore.components.push_back({"chalcopyrite", 0.8f, false});
    ore.components.push_back({"quartz", 0.2f, true});
    assert(bag.AddOre(ore));
    layout.Reconcile(bag.Capture(), chest.Capture());
    const auto key = ItemLayout::OreKey(ore);
    assert(ItemLayout::Resolve(bag.Capture(), key) == 0);
    assert(chest.AddOre(ore));
    assert(bag.RemoveOre(0, ore.massKg));
    layout.Reconcile(bag.Capture(), chest.Capture());
    assert(ItemLayout::Resolve(chest.Capture(), key) == 0);
}
