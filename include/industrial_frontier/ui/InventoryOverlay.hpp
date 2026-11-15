#pragma once

#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/ui/ItemLayout.hpp"
#include "industrial_frontier/ui/UIRenderer.hpp"

#include <cstddef>

namespace industrial_frontier
{
    class InventoryOverlay
    {
    public:
        void Render(UIRenderer& ui, const InventoryState& inventory,
                    int width, int height, std::size_t selectedSlot,
                    const ItemLayout::Grid& slots) const;
        int HitSlot(int width, int height, float x, float y) const;

    private:
        static float PanelWidth(int width);
        static float PanelX(int width);
        static float PanelY(int height);
    };
}
