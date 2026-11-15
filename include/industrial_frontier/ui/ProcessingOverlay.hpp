#pragma once

#include "industrial_frontier/resources/OreProcessing.hpp"
#include "industrial_frontier/ui/UIRenderer.hpp"

#include <cstddef>
#include <string>

namespace industrial_frontier
{
    struct ProcessingClick
    {
        bool close = false;
        bool changedSelection = false;
        std::size_t selectedSlot = 0;
        bool run = false;
        ProcessingOperation operation = ProcessingOperation::Crush;
    };

    class ProcessingOverlay
    {
    public:
        void Render(UIRenderer& ui, const InventoryState& inventory,
                    int width, int height, std::size_t selectedSlot,
                    const std::string& message) const;
        ProcessingClick Click(int width, int height, float x, float y,
                              const InventoryState& inventory,
                              std::size_t selectedSlot) const;

    private:
        static float PanelWidth(int width);
        static float PanelX(int width);
        static float PanelY(int height);
    };
}
