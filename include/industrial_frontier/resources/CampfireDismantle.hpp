#pragma once

#include "industrial_frontier/resources/Campfire.hpp"
#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"

#include <cstdint>
#include <string>

namespace industrial_frontier
{
    enum class CampfireDismantleResult
    {
        Success,
        NotFound,
        StillBurning,
        StillCovered,
        CharcoalRemaining,
        InventoryFull
    };

    inline CampfireDismantleResult DismantleCampfire(
        CampfireWorld& world, PlayerInventory& inventory, std::uint64_t id)
    {
        const CampfireState* fire = world.Find(id);
        if (!fire)
            return CampfireDismantleResult::NotFound;
        if (fire->fuelSeconds > 0.0f)
            return CampfireDismantleResult::StillBurning;
        if (fire->charcoalReady > 0)
            return CampfireDismantleResult::CharcoalRemaining;
        if (fire->covered)
            return CampfireDismantleResult::StillCovered;

        PlayerInventory staged;
        if (!staged.Restore(inventory.Capture()))
            return CampfireDismantleResult::InventoryFull;

        bool hasRecipe = false;
        for (const auto& recipe : PrimitiveRecipes)
        {
            if (recipe.id != "campfire_kit_recipe")
                continue;
            hasRecipe = true;
            for (std::size_t i = 0; i < recipe.ingredientCount; ++i)
            {
                if (!staged.AddItem(std::string(recipe.ingredients[i].itemId),
                    recipe.ingredients[i].quantity))
                    return CampfireDismantleResult::InventoryFull;
            }
            break;
        }
        if (!hasRecipe)
            return CampfireDismantleResult::NotFound;
        if (!world.Remove(id))
            return CampfireDismantleResult::NotFound;
        inventory.Restore(staged.Capture());
        return CampfireDismantleResult::Success;
    }
}
