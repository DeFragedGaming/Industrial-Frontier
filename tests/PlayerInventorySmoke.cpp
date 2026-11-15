#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
using namespace industrial_frontier;
void Check(bool result, const char* message)
{
    if (!result) { std::cerr << "Player inventory test failed: " << message << '\n'; std::exit(1); }
}
int main()
{
    PlayerInventory bag;
    Check(bag.CarryCapacityKg() == 500.0f, "current player base capacity");
    Check(bag.UsedSlots() == 0, "empty start");
    Check(!bag.AddItem("not_an_item", 1), "reject unknown item");
    Check(bag.AddItem("log", 105), "accept full 105 logs");
    Check(bag.ItemCount("log") == 105, "count logs");
    Check(std::fabs(bag.TotalWeightKg() - 210.0f) < 0.01f, "log mass");
    Check(bag.RemoveItem("log", 5), "partial remove");
    Check(bag.ItemCount("log") == 100, "remaining logs");
    Check(!bag.AddItem("raw_stone", 300), "enforce 500 kg total");
    Check(bag.AddItem("copper_ingot", 7), "ingots exist");
    Check(bag.AddItem("stick", 6), "crafting handles");
    std::string feedback;
    Check(bag.Craft("copper_axe_recipe", feedback), "copper axe crafted");
    Check(bag.Craft("copper_pickaxe_recipe", feedback), "copper pickaxe crafted");
    Check(bag.Craft("copper_shovel_recipe", feedback), "copper shovel crafted");
    Check(bag.ItemCount("copper_ingot") == 1, "craft consumption accurate");
    Check(bag.ItemCount("stick") == 0, "stick consumption accurate");
    Check(bag.HasUsableTool("copper_pickaxe"), "usable copper tool");
    bool broke = false;
    Check(bag.UseTool("copper_pickaxe", 1, broke) && !broke, "tool durability");
    Check(PlayerInventory::IsValidState(bag.Capture()), "valid new items");
    PlayerInventory restored;
    Check(restored.Restore(bag.Capture()), "restore modern inventory");
    Check(restored.HasUsableTool("copper_pickaxe"), "tool survives restore");
    Check(restored.SetCarryCapacityBonus(100.0f), "capacity bonus");
    Check(restored.CarryCapacityKg() == 600.0f, "bonus applied");
    InventoryOreBatch batch;
    batch.depositId = 123;
    batch.primaryMineralId = "malachite";
    batch.displayName = "Malachite ore";
    batch.massKg = 3.0f;
    batch.oreMineralFraction = 1.0f;
    batch.components.push_back({"malachite", 1.0f, false});
    Check(restored.AddOre(batch), "real ore batch");
    Check(restored.RemoveOre(0, 1.0f), "ore partial removal");
    Check(restored.OreBatches()[0].massKg == 2.0f, "remaining ore mass");
    Check(PlayerInventory::IsValidState(restored.Capture()), "valid stored ore and metal tools");
    std::cout << "Player inventory weight, recipes, copper tools, ore, save restore passed\n";
}
