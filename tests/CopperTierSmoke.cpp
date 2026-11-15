#include "industrial_frontier/resources/PrimitiveItems.hpp"
#include "industrial_frontier/resources/PrimitiveTier.hpp"
#include "industrial_frontier/resources/PlayerInventory.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <utility>
#include <string>
#include <string_view>

using namespace industrial_frontier;

void Require(bool ok, const char* label)
{
    if (!ok)
    {
        std::cerr << "Copper tier failed: " << label << '\n';
        std::exit(1);
    }
}

const PrimitiveRecipe* FindRecipe(std::string_view id)
{
    for (const auto& recipe : PrimitiveRecipes)
    {
        if (recipe.id == id)
            return &recipe;
    }
    return nullptr;
}

void RequireRecipe(std::string_view id, std::string_view output,
    const char* label)
{
    const auto* recipe = FindRecipe(id);
    Require(recipe != nullptr && recipe->resultId == output &&
        recipe->resultQuantity == 1, label);
}

int main()
{
    const std::array<std::pair<std::string_view, std::string_view>, 10> expectedRecipes{{
        {"stone_axe_recipe", "stone_axe"},
        {"stone_pickaxe_recipe", "stone_pickaxe"},
        {"stone_shovel_recipe", "stone_shovel"},
        {"campfire_kit_recipe", "campfire_kit"},
        {"furnace_kit_recipe", "furnace_kit"},
        {"ingot_mold_recipe", "ingot_mold"},
        {"copper_axe_recipe", "copper_axe"},
        {"copper_pickaxe_recipe", "copper_pickaxe"},
        {"copper_shovel_recipe", "copper_shovel"},
        {"multi_ingot_mold_recipe", "multi_ingot_mold"}
    }};

    Require(PrimitiveRecipes.size() >= expectedRecipes.size(),
        "baseline crafting recipes are present");
    for (const auto& [recipe, output] : expectedRecipes)
        RequireRecipe(recipe, output, "baseline recipe is valid");

    for (std::size_t i = 0; i < PrimitiveRecipes.size(); ++i)
    {
        for (std::size_t j = i + 1; j < PrimitiveRecipes.size(); ++j)
            Require(PrimitiveRecipes[i].id != PrimitiveRecipes[j].id,
                "crafting recipe IDs must be unique");
    }

    Require(FindPrimitiveItem("copper_axe") != nullptr &&
        FindPrimitiveItem("copper_axe")->maximumDurability == 160, "axe durability");
    Require(FindPrimitiveItem("copper_pickaxe") != nullptr &&
        FindPrimitiveItem("copper_pickaxe")->maximumDurability == 230, "pickaxe durability");
    Require(FindPrimitiveItem("copper_shovel") != nullptr &&
        FindPrimitiveItem("copper_shovel")->maximumDurability == 145, "shovel durability");

    Require(CanMineMineral("stone_pickaxe", "malachite"), "primitive copper oxide");
    Require(!CanMineMineral("stone_pickaxe", "chalcopyrite"), "stone tier cap");
    Require(CanMineMineral("copper_pickaxe", "chalcopyrite"), "copper sulfide mining");
    Require(CanMineMineral("copper_pickaxe", "chalcocite"), "chalcocite");
    Require(CanMineMineral("copper_pickaxe", "galena"), "galena");
    Require(CanMineMineral("copper_pickaxe", "sphalerite"), "sphalerite");
    Require(!CanMineMineral("copper_pickaxe", "hematite"), "iron ore restricted");
    Require(!CanMineMineral("copper_pickaxe", "cassiterite"), "hard tin restricted");
    Require(!CanMineMineral("copper_pickaxe", "quartz"), "hard stone restricted");
    Require(!CanMineMineral("copper_pickaxe", "uraninite"), "rare material restricted");
    Require(!CanMineMineral("copper_pickaxe", "enargite"), "arsenic-rich restricted");
    Require(!CanMineMineral("no_pickaxe", "malachite"), "equipped tool required");
    Require(IsUsableAxeId("copper_axe") && IsUsableShovelId("copper_shovel"),
        "copper tools categorized correctly");

    PlayerInventory inventory;
    std::string message;
    Require(inventory.AddItem("copper_ingot", 6), "ingots");
    Require(inventory.AddItem("stick", 6), "sticks");
    Require(inventory.Craft("copper_axe_recipe", message), "axe recipe");
    Require(inventory.Craft("copper_pickaxe_recipe", message), "pick recipe");
    Require(inventory.Craft("copper_shovel_recipe", message), "shovel recipe");
    Require(inventory.ItemCount("copper_ingot") == 0, "correct ingot consumption");
    Require(inventory.ItemCount("stick") == 0, "correct handle consumption");
    Require(inventory.HasUsableTool("copper_pickaxe"), "tools work");

    const auto data = inventory.Capture();
    Require(PlayerInventory::IsValidState(data), "save state valid");
    PlayerInventory restored;
    Require(restored.Restore(data) && restored.HasUsableTool("copper_axe"),
        "tool save roundtrip");
    Require(!inventory.Craft("copper_pickaxe_recipe", message), "no free crafting");

    PlayerInventory caster;
    Require(caster.AddItem("raw_stone", 12) &&
        caster.AddItem("clay_soil", 16) && caster.AddItem("stick", 2),
        "casting tray supplies");
    Require(caster.Craft("multi_ingot_mold_recipe", message), "casting tray recipe");
    Require(caster.ItemCount("multi_ingot_mold") == 1, "one casting tray");
    Require(PlayerInventory::IsValidState(caster.Capture()), "casting tray saves");

    RequireRecipe("wooden_chest_recipe", "wooden_chest_kit", "wooden chest recipe");
    RequireRecipe("log_foundation_recipe", "log_foundation_kit", "foundation recipe");
    RequireRecipe("log_wall_recipe", "log_wall_kit", "wall recipe");
    RequireRecipe("log_doorframe_recipe", "log_doorframe_kit", "doorframe recipe");
    RequireRecipe("log_window_recipe", "log_window_kit", "window recipe");
    RequireRecipe("log_roof_recipe", "log_roof_kit", "roof recipe");
    RequireRecipe("log_support_recipe", "log_support_kit", "support recipe");

    std::cout << "Copper crafting, mining caps, construction recipes and persistence passed\n";
}
