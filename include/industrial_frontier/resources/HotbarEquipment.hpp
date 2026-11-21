#pragma once

#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/resources/BuildStructures.hpp"

#include <string>

namespace industrial_frontier
{
    inline bool IsEquippableHotbarItem(const PlayerInventory& inventory,
        const std::string& itemId)
    {
        BuildKind kind{};
        return inventory.HasUsableTool(itemId) ||
            (BuildKitKind(itemId, kind) && inventory.ItemCount(itemId) > 0) ||
            (itemId == "campfire_kit" && inventory.ItemCount(itemId) > 0) ||
            (itemId == "furnace_kit" && inventory.ItemCount(itemId) > 0) ||
            (itemId == "ore_crusher_kit" && inventory.ItemCount(itemId) > 0) ||
            (itemId == "hand_mill_kit" && inventory.ItemCount(itemId) > 0) ||
            (itemId == "sluice_kit" && inventory.ItemCount(itemId) > 0) ||
            (itemId == "hot_copper_mold" && inventory.ItemCount(itemId) > 0) ||
            (itemId == "cooled_copper_mold" && inventory.ItemCount(itemId) > 0) ||
            (itemId == "ingot_mold" && inventory.ItemCount(itemId) > 0);
    }
}
