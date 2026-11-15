#pragma once

#include "industrial_frontier/ui/ItemLayout.hpp"
#include "industrial_frontier/resources/StorageTransfer.hpp"
#include "industrial_frontier/ui/UIRenderer.hpp"

#include <cstddef>
#include <string>

namespace industrial_frontier
{
    struct ItemWidgetHit
    {
        int zone = 0;
        int slot = -1;
    };

    class ItemWorkspaceOverlay
    {
    public:
        StorageTransferMode SelectedTransferMode() const { return m_transferMode; }
        void SetTransferMode(StorageTransferMode mode) { m_transferMode = mode; }
        ItemWidgetHit Hit(int width, int height, float x, float y,
            bool storageOpen) const;
        void RenderBar(UIRenderer& ui, const InventoryState& bag,
            const ItemLayout& layout, int width, int height) const;
        void RenderStorage(UIRenderer& ui, const InventoryState& bag,
            const InventoryState& storage, const ItemLayout& layout,
            int width, int height) const;
        void RenderDrag(UIRenderer& ui, const std::string& label,
            float x, float y) const;
        void RenderHoverTooltip(UIRenderer& ui, const InventoryState& bag,
            const InventoryState& storage, const ItemLayout& layout,
            int width, int height, float mouseX, float mouseY,
            bool inventoryOpen, bool storageOpen) const;
    private:
        StorageTransferMode m_transferMode = StorageTransferMode::All;
    };
}
