#pragma once

#include "industrial_frontier/ui/ItemLayout.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"
#include "industrial_frontier/ui/UIRenderer.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace industrial_frontier
{
    inline std::string ItemIconId(const InventoryState& inventory,
        const std::string& key)
    {
        const int logical = ItemLayout::Resolve(inventory, key);
        if (logical < 0)
            return {};
        if (static_cast<std::size_t>(logical) >= inventory.oreBatches.size())
        {
            return inventory.items[static_cast<std::size_t>(logical) -
                inventory.oreBatches.size()].itemId;
        }
        const auto& ore = inventory.oreBatches[logical];
        std::string mineral = ore.primaryMineralId + " " + ore.displayName;
        std::transform(mineral.begin(), mineral.end(), mineral.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (mineral.find("cassiterite") != std::string::npos ||
            mineral.find("tin") != std::string::npos)
            return "tin_ore";
        if (mineral.find("copper") != std::string::npos ||
            mineral.find("malachite") != std::string::npos ||
            mineral.find("azurite") != std::string::npos ||
            mineral.find("chalcopyrite") != std::string::npos ||
            mineral.find("bornite") != std::string::npos)
            return "copper_ore";
        return {};
    }

    inline float ItemDurabilityRatio(const InventoryState& inventory,
        const std::string& key)
    {
        const int logical = ItemLayout::Resolve(inventory, key);
        if (logical < 0 ||
            static_cast<std::size_t>(logical) < inventory.oreBatches.size())
            return -1.0f;
        const auto& stack = inventory.items[static_cast<std::size_t>(logical) -
            inventory.oreBatches.size()];
        const auto* def = FindPrimitiveItem(stack.itemId);
        if (!def || def->maximumDurability == 0)
            return -1.0f;
        return static_cast<float>(stack.durability) /
            static_cast<float>(def->maximumDurability);
    }

    inline void DrawDurability(UIRenderer& ui, float x, float y,
        float width, float ratio)
    {
        if (ratio < 0.0f)
            return;
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        ui.DrawRectangle(x, y, width, 5.0f,
            UIColor{0.05f, 0.06f, 0.07f, 1.0f});
        const UIColor color = ratio > 0.5f ? UIColor{0.33f, 0.82f, 0.44f, 1.0f} :
            ratio > 0.2f ? UIColor{0.95f, 0.72f, 0.22f, 1.0f} :
            UIColor{0.90f, 0.24f, 0.22f, 1.0f};
        ui.DrawRectangle(x, y, width * ratio, 5.0f, color);
    }
}
