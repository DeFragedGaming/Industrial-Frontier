#include "industrial_frontier/ui/ItemWorkspaceOverlay.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"
#include "industrial_frontier/resources/OreProcessing.hpp"
#include "industrial_frontier/ui/InventoryOverlay.hpp"
#include "industrial_frontier/ui/ItemIcons.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        constexpr UIColor Background{0.026f, 0.043f, 0.063f, 0.97f};
        constexpr UIColor Panel{0.10f, 0.13f, 0.16f, 0.99f};
        constexpr UIColor Slot{0.073f, 0.101f, 0.137f, 0.99f};
        constexpr UIColor Accent{0.91f, 0.61f, 0.27f, 1.0f};
        constexpr UIColor Text{0.95f, 0.96f, 0.98f, 1.0f};
        constexpr UIColor Muted{0.64f, 0.72f, 0.78f, 1.0f};
        constexpr float BarCell = 62.0f;
        constexpr float BarGap = 6.0f;
        constexpr float BarWidth = ItemLayout::BarSize * BarCell +
            (ItemLayout::BarSize - 1) * BarGap;

        struct ChestLayout
        {
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
            float leftX = 0.0f;
            float rightX = 0.0f;
            float gridY = 0.0f;
            float cellW = 0.0f;
            float cellH = 0.0f;
        };

        ChestLayout MakeChest(int width, int height)
        {
            ChestLayout result;
            result.width = std::min(1160.0f, static_cast<float>(width) - 32.0f);
            result.height = std::min(568.0f, static_cast<float>(height) - 118.0f);
            result.x = (static_cast<float>(width) - result.width) * 0.5f;
            result.y = (static_cast<float>(height) - 86.0f - result.height) * 0.5f;
            const float inner = (result.width - 58.0f) * 0.5f;
            result.leftX = result.x + 16.0f;
            result.rightX = result.x + result.width - 16.0f - inner;
            result.gridY = result.y + 95.0f;
            result.cellW = (inner - 30.0f) / 6.0f;
            result.cellH = std::clamp((result.height - 215.0f) / 5.0f, 38.0f, 70.0f);
            return result;
        }

        void Fit(UIRenderer& ui, std::string value, float x, float y,
            float width, float scale, UIColor color)
        {
            while (!value.empty() && ui.MeasureText(value, scale) > width)
                value.pop_back();
            ui.DrawText(value, x, y, scale, color);
        }

        std::string OneDecimal(float value)
        {
            std::ostringstream out;
            out << std::fixed << std::setprecision(1) << value;
            return out.str();
        }

        float TotalMass(const InventoryState& state)
        {
            float weight = 0.0f;
            for (const auto& ore : state.oreBatches)
                weight += ore.massKg;
            for (const auto& stack : state.items)
            {
                const auto* item = FindPrimitiveItem(stack.itemId);
                if (item)
                    weight += item->weightKg * stack.quantity;
            }
            return weight;
        }

        void DrawGrid(UIRenderer& ui, const InventoryState& inventory,
            const ItemLayout::Grid& grid, float gridX, float gridY,
            float cellW, float cellH)
        {
            for (int i = 0; i < 30; ++i)
            {
                const float x = gridX + (i % 6) * (cellW + 7.0f);
                const float y = gridY + (i / 6) * (cellH + 7.0f);
                ui.DrawRectangle(x, y, cellW, cellH, Slot);
                const int logical = ItemLayout::Resolve(inventory, grid[i]);
                if (logical < 0)
                    continue;

                const std::string icon = ItemIconId(inventory, grid[i]);
                const float size = std::max(24.0f,
                    std::min({54.0f, cellW - 8.0f, cellH - 9.0f}));
                if (!icon.empty())
                    ui.DrawIcon(icon, x + (cellW - size) * 0.5f,
                        y + (cellH - size) * 0.5f - 3.0f, size, size);
                else
                    ui.DrawText("ORE", x + 8.0f,
                        y + cellH * 0.5f - 7.0f, 0.82f, Muted);

                std::string amount;
                if (static_cast<std::size_t>(logical) < inventory.oreBatches.size())
                    amount = OneDecimal(inventory.oreBatches[logical].massKg) + "kg";
                else
                {
                    const auto& stack = inventory.items[static_cast<std::size_t>(logical) -
                        inventory.oreBatches.size()];
                    amount = "x" + std::to_string(stack.quantity);
                }
                const float tw = ui.MeasureText(amount, 0.79f);
                const float tx = x + cellW - tw - 5.0f;
                const float ty = y + cellH - 23.0f;
                ui.DrawRectangle(tx - 3.0f, ty - 2.0f,
                    tw + 6.0f, 17.0f, Panel);
                ui.DrawText(amount, tx, ty, 0.79f, Accent);
                DrawDurability(ui, x + 4.0f, y + cellH - 7.0f,
                    cellW - 8.0f, ItemDurabilityRatio(inventory, grid[i]));
            }
        }

    }

    ItemWidgetHit ItemWorkspaceOverlay::Hit(int width, int height,
        float x, float y, bool storageOpen) const
    {
        if (width < 760 || height < 540)
            return {};
        const float barX = (static_cast<float>(width) - BarWidth) * 0.5f;
        const float barY = static_cast<float>(height) - 76.0f;
        for (int i = 0; i < static_cast<int>(ItemLayout::BarSize); ++i)
        {
            const float sx = barX + i * (BarCell + BarGap);
            if (x >= sx && x < sx + BarCell && y >= barY && y < barY + BarCell)
                return {3, i};
        }
        if (!storageOpen)
            return {};
        const auto layout = MakeChest(width, height);
        const float buttonY = layout.y + layout.height - 73.0f;
        for (int index = 0; index < 5; ++index)
        {
            const float buttonX = layout.x + 183.0f + index * 92.0f;
            if (x >= buttonX && x < buttonX + 84.0f &&
                y >= buttonY && y < buttonY + 29.0f)
                return {4, index};
        }
        for (int zone = 1; zone <= 2; ++zone)
        {
            const float gridX = zone == 1 ? layout.leftX : layout.rightX;
            for (int i = 0; i < 30; ++i)
            {
                const float sx = gridX + (i % 6) * (layout.cellW + 7.0f);
                const float sy = layout.gridY + (i / 6) * (layout.cellH + 7.0f);
                if (x >= sx && x < sx + layout.cellW &&
                    y >= sy && y < sy + layout.cellH)
                    return {zone, i};
            }
        }
        return {};
    }

    void ItemWorkspaceOverlay::RenderBar(UIRenderer& ui,
        const InventoryState& inventory, const ItemLayout& layout,
        int width, int height) const
    {
        if (width < 680 || height < 230)
            return;
        const float startX = (static_cast<float>(width) - BarWidth) * 0.5f;
        const float y = static_cast<float>(height) - 76.0f;
        ui.DrawRectangle(startX - 9.0f, y - 9.0f,
            BarWidth + 18.0f, BarCell + 18.0f, Background);
        if (layout.selected < layout.bar.size())
        {
            const auto* def = FindPrimitiveItem(layout.bar[layout.selected]);
            if (def)
            {
                std::string label(def->name);
                const int logical = ItemLayout::Resolve(inventory,
                    ItemLayout::ItemKey(layout.bar[layout.selected]));
                if (logical >= static_cast<int>(inventory.oreBatches.size()))
                {
                    const auto& stack = inventory.items[static_cast<std::size_t>(logical) -
                        inventory.oreBatches.size()];
                    if (def->maximumDurability != 0)
                        label += "  |  " + std::to_string(stack.durability) +
                            "/" + std::to_string(def->maximumDurability);
                }
                const float labelWidth = ui.MeasureText(label, 0.94f);
                ui.DrawText(label, startX + (BarWidth - labelWidth) * 0.5f,
                    y - 30.0f, 0.94f, Text);
            }
        }
        for (std::size_t i = 0; i < ItemLayout::BarSize; ++i)
        {
            const float x = startX + static_cast<float>(i) * (BarCell + BarGap);
            const bool active = i == layout.selected;
            ui.DrawRectangle(x - (active ? 2.0f : 0.0f),
                y - (active ? 2.0f : 0.0f),
                BarCell + (active ? 4.0f : 0.0f),
                BarCell + (active ? 4.0f : 0.0f),
                active ? Accent : Panel);
            ui.DrawRectangle(x + 2.0f, y + 2.0f, BarCell - 4.0f,
                BarCell - 4.0f, Slot);
            const auto& id = layout.bar[i];
            if (!id.empty())
            {
                const auto* def = FindPrimitiveItem(id);
                if (def && ItemLayout::Resolve(inventory, ItemLayout::ItemKey(id)) >= 0)
                {
                    ui.DrawIcon(id, x + 8.0f, y + 8.0f, 46.0f, 46.0f);
                    DrawDurability(ui, x + 5.0f, y + BarCell - 8.0f,
                        BarCell - 10.0f,
                        ItemDurabilityRatio(inventory, ItemLayout::ItemKey(id)));
                    for (const auto& stack : inventory.items)
                    {
                        if (stack.itemId == id && stack.quantity > 1)
                        {
                            const std::string count = "x" + std::to_string(stack.quantity);
                            ui.DrawRectangle(x + BarCell - 35.0f, y + 37.0f,
                                30.0f, 15.0f, Panel);
                            Fit(ui, count, x + BarCell - 32.0f, y + 39.0f,
                                27.0f, 0.73f, Text);
                            break;
                        }
                    }
                }
            }
            ui.DrawRectangle(x + 3.0f, y + 3.0f,
                20.0f, 19.0f,
                UIColor{0.018f, 0.028f, 0.040f, 0.96f});
            ui.DrawText(std::to_string(i + 1),
                x + 9.0f, y + 5.0f, 0.84f,
                active ? Accent : Text);
        }
    }

    void ItemWorkspaceOverlay::RenderStorage(UIRenderer& ui,
        const InventoryState& bag, const InventoryState& storage,
        const ItemLayout& layout, int width, int height) const
    {
        if (width < 760 || height < 540)
            return;
        const auto rect = MakeChest(width, height);
        ui.DrawRectangle(rect.x, rect.y, rect.width, rect.height, Background);
        ui.DrawRectangle(rect.x, rect.y, rect.width, 55.0f, Panel);
        ui.DrawRectangle(rect.x, rect.y, 4.0f, rect.height, Accent);
        ui.DrawText("STARTER STORAGE  |  B / ESC CLOSE", rect.x + 21.0f,
            rect.y + 17.0f, 1.23f, Text);
        Fit(ui, "INVENTORY " + OneDecimal(TotalMass(bag)) + "/" +
            OneDecimal(PlayerInventory::BaseCarryCapacityKg + bag.carryCapacityBonusKg) + " KG",
            rect.leftX, rect.gridY - 27.0f, 340.0f, 0.96f, Accent);
        Fit(ui, "STORAGE " + OneDecimal(TotalMass(storage)) + "/" +
            OneDecimal(PlayerInventory::BaseCarryCapacityKg + storage.carryCapacityBonusKg) + " KG",
            rect.rightX, rect.gridY - 27.0f, 340.0f, 0.96f, Accent);
        DrawGrid(ui, bag, layout.bag, rect.leftX, rect.gridY,
            rect.cellW, rect.cellH);
        DrawGrid(ui, storage, layout.storage, rect.rightX, rect.gridY,
            rect.cellW, rect.cellH);
        ui.DrawText("TRANSFER:", rect.x + 21.0f,
            rect.y + rect.height - 66.0f, 0.94f, Accent);
        for (int index = 0; index < 5; ++index)
        {
            const auto mode = static_cast<StorageTransferMode>(index);
            const bool selected = m_transferMode == mode;
            const float x = rect.x + 183.0f + index * 92.0f;
            const float y = rect.y + rect.height - 73.0f;
            ui.DrawRectangle(x, y, 84.0f, 29.0f,
                selected ? Accent : Slot);
            ui.DrawText(StorageTransferModeName(mode), x + 10.0f,
                y + 7.0f, 0.87f, selected ? Background : Text);
        }
        ui.DrawText("CLICK AMOUNT, THEN DRAG BETWEEN PANELS  |  ORES: KG  |  ITEMS: COUNT",
            rect.x + 21.0f, rect.y + rect.height - 28.0f, 0.84f, Muted);
    }

    void ItemWorkspaceOverlay::RenderDrag(UIRenderer& ui,
        const std::string& label, float x, float y) const
    {
        if (label.empty())
            return;
        const float w = std::min(220.0f, ui.MeasureText(label, 0.93f) + 18.0f);
        ui.DrawRectangle(x + 12.0f, y + 13.0f, w, 31.0f, Panel);
        Fit(ui, label, x + 20.0f, y + 20.0f, w - 16.0f, 0.93f, Text);
    }
    void ItemWorkspaceOverlay::RenderHoverTooltip(UIRenderer& ui,
        const InventoryState& bag, const InventoryState& storage,
        const ItemLayout& layout, int width, int height,
        float mouseX, float mouseY, bool inventoryOpen, bool storageOpen) const
    {
        if (!inventoryOpen && !storageOpen)
            return;
        ItemWidgetHit hit{};
        if (storageOpen)
        {
            hit = Hit(width, height, mouseX, mouseY, true);
        }
        else
        {
            const int inventorySlot = InventoryOverlay{}.HitSlot(
                width, height, mouseX, mouseY);
            if (inventorySlot >= 0)
                hit = {1, inventorySlot};
            else
                hit = Hit(width, height, mouseX, mouseY, false);
        }

        const InventoryState* inventory = nullptr;
        std::string key;
        if (hit.zone == 3 && hit.slot >= 0 &&
            hit.slot < static_cast<int>(layout.bar.size()))
        {
            inventory = &bag;
            const auto& id = layout.bar[hit.slot];
            if (!id.empty())
                key = ItemLayout::ItemKey(id);
        }
        else if ((hit.zone == 1 || hit.zone == 2) &&
            hit.slot >= 0 && hit.slot < 30)
        {
            inventory = hit.zone == 1 ? &bag : &storage;
            key = (hit.zone == 1 ? layout.bag : layout.storage)[hit.slot];
        }
        if (!inventory || key.empty())
            return;
        const int logical = ItemLayout::Resolve(*inventory, key);
        if (logical < 0)
            return;

        std::string name;
        std::vector<std::string> details;
        float durability = -1.0f;
        if (static_cast<std::size_t>(logical) < inventory->oreBatches.size())
        {
            const auto& ore = inventory->oreBatches[logical];
            name = ore.displayName;
            details.push_back("Mass: " + OneDecimal(ore.massKg) + " kg");
            details.push_back("Stage: " +
                std::string(OreProcessing::StageName(ore.processingStage)));
        }
        else
        {
            const auto& stack = inventory->items[static_cast<std::size_t>(logical) -
                inventory->oreBatches.size()];
            const auto* def = FindPrimitiveItem(stack.itemId);
            name = def ? std::string(def->name) : stack.itemId;
            details.push_back("Quantity: " + std::to_string(stack.quantity));
            if (def)
            {
                details.push_back("Weight: " +
                    OneDecimal(def->weightKg * stack.quantity) + " kg");
                if (def->maximumDurability > 0)
                {
                    details.push_back("Durability: " +
                        std::to_string(stack.durability) + "/" +
                        std::to_string(def->maximumDurability));
                    durability = ItemDurabilityRatio(*inventory, key);
                }
            }
        }

        const float fontScale = 0.97f;
        float textW = ui.MeasureText(name, 1.06f);
        for (const std::string& line : details)
            textW = std::max(textW, ui.MeasureText(line, fontScale));
        const float tooltipW = std::clamp(textW + 28.0f, 145.0f,
            std::max(145.0f, static_cast<float>(width) - 18.0f));
        const float tooltipH = 39.0f + 21.0f * details.size() +
            (durability >= 0.0f ? 12.0f : 0.0f);
        const float tx = std::clamp(mouseX + 18.0f, 7.0f,
            std::max(7.0f, static_cast<float>(width) - tooltipW - 7.0f));
        const float ty = std::clamp(mouseY + 17.0f, 7.0f,
            std::max(7.0f, static_cast<float>(height) - tooltipH - 7.0f));
        ui.DrawRectangle(tx - 2.0f, ty - 2.0f, tooltipW + 4.0f,
            tooltipH + 4.0f, Accent);
        ui.DrawRectangle(tx, ty, tooltipW, tooltipH, Background);
        Fit(ui, name, tx + 13.0f, ty + 10.0f, tooltipW - 26.0f,
            1.06f, Text);
        float lineY = ty + 34.0f;
        for (const std::string& line : details)
        {
            Fit(ui, line, tx + 13.0f, lineY,
                tooltipW - 26.0f, fontScale, Muted);
            lineY += 21.0f;
        }
        if (durability >= 0.0f)
            DrawDurability(ui, tx + 13.0f, tooltipH + ty - 15.0f,
                tooltipW - 26.0f, durability);
    }

}
