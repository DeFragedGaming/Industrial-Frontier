#include "industrial_frontier/ui/InventoryOverlay.hpp"
#include "industrial_frontier/resources/OreProcessing.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"
#include "industrial_frontier/ui/ItemIcons.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        constexpr UIColor Backdrop{0.025f, 0.04f, 0.065f, 0.98f};
        constexpr UIColor Slot{0.085f, 0.11f, 0.145f, 0.98f};
        constexpr UIColor Selected{0.19f, 0.18f, 0.115f, 1.0f};
        constexpr UIColor Header{0.12f, 0.15f, 0.18f, 1.0f};
        constexpr UIColor DetailSurface{0.065f, 0.085f, 0.115f, 1.0f};
        constexpr UIColor Accent{0.85f, 0.61f, 0.27f, 1.0f};
        constexpr UIColor Text{0.94f, 0.95f, 0.97f, 1.0f};
        constexpr UIColor Muted{0.66f, 0.71f, 0.76f, 1.0f};
        constexpr UIColor Green{0.53f, 0.80f, 0.62f, 1.0f};
        constexpr int Columns = 6;
        constexpr int Rows = 5;
        constexpr float Gap = 8.0f;
        constexpr float Padding = 20.0f;
        constexpr float Gutter = 16.0f;

        struct InventoryLayout
        {
            float panelX = 0.0f;
            float panelY = 0.0f;
            float panelWidth = 0.0f;
            float panelHeight = 0.0f;
            float gridX = 0.0f;
            float gridY = 0.0f;
            float gridWidth = 0.0f;
            float cellWidth = 0.0f;
            float cellHeight = 0.0f;
            float detailsX = 0.0f;
            float detailsWidth = 0.0f;
        };

        float WindowHeight(int height)
        {
            return std::min(590.0f, std::max(0.0f,
                static_cast<float>(height) - 118.0f));
        }

        InventoryLayout MakeLayout(int width, int height)
        {
            InventoryLayout layout;
            layout.panelWidth = std::min(1120.0f,
                static_cast<float>(width) - 32.0f);
            layout.panelHeight = WindowHeight(height);
            layout.panelX = (static_cast<float>(width) - layout.panelWidth) * 0.5f;
            layout.panelY = (static_cast<float>(height) - 86.0f - layout.panelHeight) * 0.5f;
            layout.detailsWidth = std::clamp(layout.panelWidth * 0.35f,
                258.0f, 405.0f);
            layout.detailsX = layout.panelX + layout.panelWidth -
                Padding - layout.detailsWidth;
            layout.gridX = layout.panelX + Padding;
            layout.gridY = layout.panelY + 82.0f;
            layout.gridWidth = layout.detailsX - Gutter - layout.gridX;
            layout.cellWidth = (layout.gridWidth - Gap * (Columns - 1)) /
                static_cast<float>(Columns);
            layout.cellHeight = std::clamp(
                (layout.panelHeight - 235.0f) / static_cast<float>(Rows),
                47.0f, 86.0f);
            return layout;
        }

        std::string Amount(float value, int precision = 1)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(precision) << value;
            return stream.str();
        }

        std::string DisplayMineralId(std::string id)
        {
            bool capitalize = true;
            for (char& character : id)
            {
                if (character == '_' || character == '-')
                {
                    character = ' ';
                    capitalize = true;
                }
                else if (capitalize &&
                    std::isalpha(static_cast<unsigned char>(character)))
                {
                    character = static_cast<char>(std::toupper(
                        static_cast<unsigned char>(character)));
                    capitalize = false;
                }
                else if (character == ' ')
                {
                    capitalize = true;
                }
            }
            return id;
        }

        std::vector<std::string> WrapText(UIRenderer& ui,
            const std::string& value, float scale, float width)
        {
            std::vector<std::string> lines;
            std::istringstream words(value);
            std::string word;
            std::string line;
            const float limit = std::max(20.0f, width);

            auto appendWord = [&](const std::string& part)
            {
                const std::string candidate = line.empty()
                    ? part : line + " " + part;
                if (line.empty() || ui.MeasureText(candidate, scale) <= limit)
                {
                    line = candidate;
                    return;
                }
                lines.push_back(line);
                line = part;
            };

            while (words >> word)
            {
                if (ui.MeasureText(word, scale) <= limit)
                {
                    appendWord(word);
                    continue;
                }

                if (!line.empty())
                {
                    lines.push_back(line);
                    line.clear();
                }

                std::string fragment;
                for (char character : word)
                {
                    const std::string extended = fragment + character;
                    if (!fragment.empty() &&
                        ui.MeasureText(extended, scale) > limit)
                    {
                        lines.push_back(fragment);
                        fragment.clear();
                    }
                    fragment += character;
                }
                line = fragment;
            }
            if (!line.empty())
            {
                lines.push_back(line);
            }
            return lines;
        }

        float DrawWrapped(UIRenderer& ui, const std::string& text,
            float x, float y, float width, float scale, float lineHeight,
            float maximumY, UIColor color)
        {
            const auto lines = WrapText(ui, text, scale, width);
            for (const auto& line : lines)
            {
                if (y + lineHeight > maximumY)
                {
                    break;
                }
                ui.DrawText(line, x, y, scale, color);
                y += lineHeight;
            }
            return y;
        }

        void DrawFitted(UIRenderer& ui, const std::string& label,
            float x, float y, float width, float scale, UIColor color)
        {
            float adjustedScale = scale;
            while (adjustedScale > 0.70f &&
                ui.MeasureText(label, adjustedScale) > width)
            {
                adjustedScale -= 0.05f;
            }
            if (ui.MeasureText(label, adjustedScale) <= width)
            {
                ui.DrawText(label, x, y, adjustedScale, color);
                return;
            }

            std::string shortened = label;
            while (!shortened.empty() &&
                ui.MeasureText(shortened + "..", adjustedScale) > width)
            {
                shortened.pop_back();
            }
            ui.DrawText(shortened + "..", x, y, adjustedScale, color);
        }
    }

    float InventoryOverlay::PanelWidth(int width)
    {
        return std::min(1120.0f, static_cast<float>(width) - 32.0f);
    }

    float InventoryOverlay::PanelX(int width)
    {
        return (static_cast<float>(width) - PanelWidth(width)) * 0.5f;
    }

    float InventoryOverlay::PanelY(int height)
    {
        return (static_cast<float>(height) - 86.0f - WindowHeight(height)) * 0.5f;
    }

    int InventoryOverlay::HitSlot(int width, int height, float x, float y) const
    {
        if (width < 760 || height < 540)
        {
            return -1;
        }

        const InventoryLayout layout = MakeLayout(width, height);
        for (int index = 0; index < Columns * Rows; ++index)
        {
            const float sx = layout.gridX +
                static_cast<float>(index % Columns) * (layout.cellWidth + Gap);
            const float sy = layout.gridY +
                static_cast<float>(index / Columns) * (layout.cellHeight + Gap);
            if (x >= sx && x <= sx + layout.cellWidth &&
                y >= sy && y <= sy + layout.cellHeight)
            {
                return index;
            }
        }
        return -1;
    }

    void InventoryOverlay::Render(UIRenderer& ui,
        const InventoryState& inventory, int width, int height,
        std::size_t selectedSlot, const ItemLayout::Grid& slots) const
    {
        if (width < 760 || height < 540)
        {
            return;
        }

        const InventoryLayout layout = MakeLayout(width, height);
        const float bottom = layout.panelY + layout.panelHeight;
        const float contentBottom = bottom - 18.0f;
        const std::size_t occupied = inventory.oreBatches.size() +
            inventory.items.size();

        double totalWeight = 0.0;
        for (const auto& batch : inventory.oreBatches)
        {
            totalWeight += batch.massKg;
        }
        for (const auto& stack : inventory.items)
        {
            const auto* item = FindPrimitiveItem(stack.itemId);
            if (item)
                totalWeight += static_cast<double>(item->weightKg) * stack.quantity;
        }
        const float capacity = PlayerInventory::BaseCarryCapacityKg +
            inventory.carryCapacityBonusKg;

        ui.DrawRectangle(layout.panelX - 2.0f, layout.panelY - 2.0f,
            layout.panelWidth + 4.0f, layout.panelHeight + 4.0f, Header);
        ui.DrawRectangle(layout.panelX, layout.panelY,
            layout.panelWidth, layout.panelHeight, Backdrop);
        ui.DrawRectangle(layout.panelX, layout.panelY,
            layout.panelWidth, 64.0f, Header);
        ui.DrawRectangle(layout.panelX, layout.panelY,
            4.0f, layout.panelHeight, Accent);
        ui.DrawText("PLAYER INVENTORY", layout.panelX + Padding,
            layout.panelY + 17.0f, 1.45f, Text);
        const std::string closeHint = "I / ESC CLOSE";
        ui.DrawText(closeHint,
            layout.panelX + layout.panelWidth - Padding -
                ui.MeasureText(closeHint, 0.94f),
            layout.panelY + 22.0f, 0.94f, Muted);

        for (int index = 0; index < Columns * Rows; ++index)
        {
            const float sx = layout.gridX +
                static_cast<float>(index % Columns) * (layout.cellWidth + Gap);
            const float sy = layout.gridY +
                static_cast<float>(index / Columns) * (layout.cellHeight + Gap);
            ui.DrawRectangle(sx, sy, layout.cellWidth, layout.cellHeight,
                selectedSlot == static_cast<std::size_t>(index) ? Selected : Slot);
            const std::string& key = slots[index];
            const int logical = ItemLayout::Resolve(inventory, key);
            if (logical >= 0)
            {
                const std::string icon = ItemIconId(inventory, key);
                ui.DrawIcon(icon, sx + layout.cellWidth * 0.5f - 24.0f,
                    sy + 9.0f, 48.0f, 48.0f);
                DrawDurability(ui, sx + 5.0f,
                    sy + layout.cellHeight - 9.0f,
                    layout.cellWidth - 10.0f,
                    ItemDurabilityRatio(inventory, key));
            }
            if (logical >= 0 &&
                static_cast<std::size_t>(logical) < inventory.oreBatches.size())
            {
                const auto& batch = inventory.oreBatches[logical];
                DrawFitted(ui, batch.displayName, sx + 6.0f, sy + layout.cellHeight - 43.0f,
                    layout.cellWidth - 12.0f, 0.82f, Text);
                ui.DrawText(Amount(batch.massKg) + " kg", sx + 6.0f,
                    sy + layout.cellHeight - 23.0f, 0.97f, Accent);
            }
            else if (logical >= 0)
            {
                const auto& stack = inventory.items[static_cast<std::size_t>(logical) -
                    inventory.oreBatches.size()];
                const auto* item = FindPrimitiveItem(stack.itemId);
                DrawFitted(ui, std::string(item ? item->name : stack.itemId),
                    sx + 6.0f, sy + layout.cellHeight - 43.0f, layout.cellWidth - 12.0f, 0.82f, Text);
                ui.DrawText("x" + std::to_string(stack.quantity), sx + 6.0f,
                    sy + layout.cellHeight - 23.0f, 0.97f, Accent);
            }
            else
            {
                ui.DrawText("-", sx + layout.cellWidth * 0.5f - 3.0f,
                    sy + layout.cellHeight * 0.5f - 6.0f, 1.0f, Muted);
            }
        }

        const float gridBottom = layout.gridY +
            Rows * layout.cellHeight + (Rows - 1) * Gap;
        const float footerY = gridBottom + 19.0f;
        ui.DrawText("WEIGHT  " + Amount(static_cast<float>(totalWeight)) +
            " / " + Amount(capacity) + " KG", layout.gridX,
            footerY, 1.05f, Text);
        const std::string slotCountText = "SLOTS  " + std::to_string(occupied) +
            "/" + std::to_string(PlayerInventory::MaxSlots);
        ui.DrawText(slotCountText,
            layout.gridX + layout.gridWidth - ui.MeasureText(slotCountText, 0.98f),
            footerY, 0.98f, Muted);
        const float fill = capacity > 0.0f
            ? std::clamp(static_cast<float>(totalWeight) / capacity, 0.0f, 1.0f)
            : 0.0f;
        ui.DrawRectangle(layout.gridX, footerY + 27.0f,
            layout.gridWidth, 13.0f, Slot);
        ui.DrawRectangle(layout.gridX, footerY + 27.0f,
            layout.gridWidth * fill, 13.0f, Accent);
        ui.DrawText("DRAG SLOTS / HOTBAR  |  B STORAGE  |  DELETE DISCARD",
            layout.gridX, footerY + 54.0f, 0.86f, Muted);

        const float detailsY = layout.panelY + 82.0f;
        const float detailsHeight = contentBottom - detailsY;
        ui.DrawRectangle(layout.detailsX, detailsY,
            layout.detailsWidth, detailsHeight, DetailSurface);
        ui.DrawRectangle(layout.detailsX, detailsY,
            layout.detailsWidth, 3.0f, Accent);

        const float detailX = layout.detailsX + 17.0f;
        const float detailWidth = layout.detailsWidth - 34.0f;
        const float detailBottom = detailsY + detailsHeight - 15.0f;
        float cursorY = detailsY + 17.0f;
        ui.DrawText("SELECTED ITEM DETAILS", detailX, cursorY, 1.10f, Accent);
        cursorY += 32.0f;

        const int selectedLogical = selectedSlot < slots.size()
            ? ItemLayout::Resolve(inventory, slots[selectedSlot]) : -1;
        if (selectedLogical < 0)
        {
            DrawWrapped(ui, "Click a filled slot for item details. Equip stone tools in the hotbar to use them. Click an ore batch for assay information.",
                detailX, cursorY, detailWidth, 1.0f, 20.0f,
                detailBottom, Muted);
            return;
        }

        if (static_cast<std::size_t>(selectedLogical) >= inventory.oreBatches.size())
        {
            const auto& stack = inventory.items[static_cast<std::size_t>(selectedLogical) -
                inventory.oreBatches.size()];
            const auto* item = FindPrimitiveItem(stack.itemId);
            const float unitWeight = item ? item->weightKg : 0.0f;
            cursorY = DrawWrapped(ui,
                std::string(item ? item->name : stack.itemId),
                detailX, cursorY, detailWidth, 1.15f, 23.0f,
                detailBottom, Text);
            cursorY += 18.0f;
            ui.DrawText("QUANTITY: " + std::to_string(stack.quantity),
                detailX, cursorY, 1.0f, Accent);
            cursorY += 29.0f;
            ui.DrawText("WEIGHT: " + Amount(unitWeight * stack.quantity, 2) + " KG",
                detailX, cursorY, 1.0f, Muted);
            cursorY += 26.0f;
            const float condition = ItemDurabilityRatio(inventory,
                ItemLayout::ItemKey(stack.itemId));
            if (condition >= 0.0f)
            {
                ui.DrawText("DURABILITY " + std::to_string(stack.durability) +
                    " / " + std::to_string(item ? item->maximumDurability : 0),
                    detailX, cursorY, 0.94f, Accent);
                DrawDurability(ui, detailX, cursorY + 23.0f,
                    std::min(detailWidth, 190.0f), condition);
                cursorY += 48.0f;
            }
            std::string hint = "Harvest or gather resources in the world.";
            if (stack.itemId == "stick" || stack.itemId == "pebble")
                hint = "Used to craft primitive stone tools. Press C for recipes.";
            else if (stack.itemId == "stone_axe")
                hint = "Equipped stone axe: click a tree to chop logs.";
            else if (stack.itemId == "stone_pickaxe")
                hint = "Equipped stone pickaxe: stone excavation grants more Raw Stone.";
            else if (stack.itemId == "stone_shovel")
                hint = "Equipped stone shovel: dirt excavation grants more Clay Soil.";
            else if (stack.itemId == "copper_axe")
                hint = "Copper Axe: cut trees faster (3 strikes); 160 durability.";
            else if (stack.itemId == "copper_pickaxe")
                hint = "Copper Pickaxe: 230 durability; mines copper sulfides and moderate ores. See G > START HERE for caps.";
            else if (stack.itemId == "copper_shovel")
                hint = "Copper Shovel: 145 durability; dig soil and clay more efficiently.";
            else if (stack.itemId == "clay_soil")
                hint = "Excavated soil and clay for early fired-clay materials.";
            DrawWrapped(ui, hint, detailX, cursorY, detailWidth,
                0.95f, 21.0f, detailBottom, Muted);
            return;
        }

        const auto& batch = inventory.oreBatches[selectedLogical];
        cursorY = DrawWrapped(ui, batch.displayName,
            detailX, cursorY, detailWidth, 1.12f, 22.0f,
            detailBottom, Text);
        cursorY += 9.0f;
        ui.DrawText(OreProcessing::StageName(batch.processingStage),
            detailX, cursorY, 0.95f, Accent);
        cursorY += 26.0f;

        ui.DrawText("BATCH MASS", detailX, cursorY, 0.91f, Muted);
        cursorY += 18.0f;
        ui.DrawText(Amount(batch.massKg, 2) + " kg", detailX,
            cursorY, 1.20f, Text);
        cursorY += 31.0f;

        ui.DrawText("ORE-BEARING MINERALS", detailX, cursorY, 0.91f, Muted);
        cursorY += 20.0f;
        ui.DrawText(Amount(batch.oreMineralFraction * 100.0f) + "%",
            detailX, cursorY, 1.17f, Green);
        cursorY += 31.0f;

        ui.DrawText("GANGUE / OTHER MINERALS", detailX, cursorY, 0.91f, Muted);
        cursorY += 20.0f;
        ui.DrawText(Amount((1.0f - batch.oreMineralFraction) * 100.0f) + "%",
            detailX, cursorY, 1.17f, Text);
        cursorY += 33.0f;

        ui.DrawRectangle(detailX, cursorY, detailWidth, 1.0f, Header);
        cursorY += 14.0f;
        ui.DrawText("MINERAL COMPOSITION", detailX, cursorY, 0.97f, Accent);
        cursorY += 24.0f;

        std::size_t shown = 0;
        for (const auto& component : batch.components)
        {
            const std::string percent =
                Amount(component.massFraction * 100.0f) + "%";
            const float percentWidth = ui.MeasureText(percent, 0.92f);
            const float nameWidth = std::max(65.0f,
                detailWidth - percentWidth - 18.0f);
            const float rowsRemaining = detailBottom - cursorY;
            if (rowsRemaining < 20.0f)
            {
                break;
            }

            const auto labelLines = WrapText(ui,
                DisplayMineralId(component.mineralId), 0.92f, nameWidth);
            const float lineCount = static_cast<float>(labelLines.size());
            const float neededHeight = std::max(21.0f, lineCount * 18.0f + 3.0f);
            if (cursorY + neededHeight > detailBottom)
            {
                break;
            }

            const UIColor itemColor = component.gangue ? Muted : Text;
            for (const auto& line : labelLines)
            {
                ui.DrawText(line, detailX, cursorY, 0.92f, itemColor);
                cursorY += 18.0f;
            }
            ui.DrawText(percent,
                detailX + detailWidth - percentWidth,
                cursorY - static_cast<float>(labelLines.size()) * 18.0f,
                0.92f, component.gangue ? Muted : Green);
            cursorY += 3.0f;
            ++shown;
        }

        if (shown < batch.components.size())
        {
            const std::string remaining = "+ " +
                std::to_string(batch.components.size() - shown) +
                " MORE COMPONENTS";
            if (cursorY + 18.0f <= detailBottom)
            {
                ui.DrawText(remaining, detailX, cursorY, 0.82f, Accent);
            }
        }
    }
}
