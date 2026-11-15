#pragma once

#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"
#include "industrial_frontier/ui/UIRenderer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace industrial_frontier
{
    struct CraftingUiHit
    {
        enum class Type { None, Select, Craft, Close };
        Type type = Type::None;
        std::size_t selected = 0;
    };

    class PrimitiveCraftingOverlay
    {
    public:
        static constexpr std::size_t PageSize = 6;
        static constexpr int CategoryCount = 3;

        static int CategoryOf(std::size_t selected)
        {
            if (selected >= PrimitiveRecipes.size())
                return 0;
            const std::string_view id = PrimitiveRecipes[selected].resultId;
            if (id == "wooden_chest_kit" || id.starts_with("log_"))
                return 2;
            if (id.starts_with("copper_") || id.starts_with("tin_") ||
                id.starts_with("lead_") || id == "multi_ingot_mold" ||
                id == "furnace_kit" || id == "ingot_mold")
                return 1;
            return 0;
        }

        static std::vector<std::size_t> CategoryEntries(int category)
        {
            std::vector<std::size_t> result;
            for (std::size_t i = 0; i < PrimitiveRecipes.size(); ++i)
                if (CategoryOf(i) == category)
                    result.push_back(i);
            return result;
        }

        static std::size_t SelectCategory(std::size_t selected, int direction)
        {
            const int previous = CategoryOf(selected);
            for (int step = 1; step <= CategoryCount; ++step)
            {
                const int next = (previous + direction * step +
                    CategoryCount * (CategoryCount + 1)) % CategoryCount;
                const auto entries = CategoryEntries(next);
                if (!entries.empty())
                    return entries.front();
            }
            return selected;
        }

        static std::size_t SelectCategoryDirect(int category)
        {
            const auto entries = CategoryEntries(category);
            return entries.empty() ? 0 : entries.front();
        }

        static std::size_t MoveSelection(std::size_t selected, int delta)
        {
            const auto entries = CategoryEntries(CategoryOf(selected));
            if (entries.empty())
                return selected;
            const auto it = std::find(entries.begin(), entries.end(), selected);
            const int position = it == entries.end() ? 0 :
                static_cast<int>(it - entries.begin());
            const int next = std::clamp(position + delta, 0,
                static_cast<int>(entries.size()) - 1);
            return entries[static_cast<std::size_t>(next)];
        }

        static std::size_t MovePage(std::size_t selected, int delta)
        {
            const auto entries = CategoryEntries(CategoryOf(selected));
            if (entries.empty())
                return selected;
            const auto it = std::find(entries.begin(), entries.end(), selected);
            const std::size_t position = it == entries.end() ? 0 :
                static_cast<std::size_t>(it - entries.begin());
            const std::size_t pageCount = (entries.size() + PageSize - 1) / PageSize;
            const int nextPage = std::clamp(
                static_cast<int>(position / PageSize) + delta, 0,
                static_cast<int>(pageCount) - 1);
            const std::size_t nextPosition = std::min(entries.size() - 1,
                static_cast<std::size_t>(nextPage) * PageSize + position % PageSize);
            return entries[nextPosition];
        }

        static std::size_t SelectVisibleSlot(std::size_t selected, std::size_t slot)
        {
            const auto entries = CategoryEntries(CategoryOf(selected));
            if (entries.empty() || slot >= PageSize)
                return selected;
            const auto it = std::find(entries.begin(), entries.end(), selected);
            const std::size_t position = it == entries.end() ? 0 :
                static_cast<std::size_t>(it - entries.begin());
            const std::size_t next = position / PageSize * PageSize + slot;
            return next < entries.size() ? entries[next] : selected;
        }

        CraftingUiHit Click(float mouseX, float mouseY,
            int viewportWidth, int viewportHeight,
            std::size_t selected) const
        {
            const Layout layout = CalculateLayout(viewportWidth, viewportHeight);
            if (layout.width <= 0.0f)
                return {};
            if (Inside(mouseX, mouseY, layout.x + layout.width - 70.0f,
                layout.y + 5.0f, 62.0f, 35.0f))
                return {CraftingUiHit::Type::Close, selected};
            for (int category = 0; category < CategoryCount; ++category)
            {
                const float tabWidth = (layout.width - 30.0f) / 3.0f;
                if (Inside(mouseX, mouseY, layout.x + 15.0f +
                    static_cast<float>(category) * tabWidth,
                    layout.y + 52.0f, tabWidth, 32.0f))
                    return {CraftingUiHit::Type::Select,
                        SelectCategoryDirect(category)};
            }
            const auto entries = CategoryEntries(CategoryOf(selected));
            const std::size_t current = PositionIn(entries, selected);
            const std::size_t first = current / PageSize * PageSize;
            for (std::size_t slot = 0; slot < PageSize &&
                first + slot < entries.size(); ++slot)
            {
                const float cardX = layout.leftX + static_cast<float>(slot % 2) *
                    (layout.cardWidth + 8.0f);
                const float cardY = layout.contentY + static_cast<float>(slot / 2) *
                    (layout.cardHeight + 8.0f);
                if (Inside(mouseX, mouseY, cardX, cardY,
                    layout.cardWidth, layout.cardHeight))
                    return {CraftingUiHit::Type::Select, entries[first + slot]};
            }
            if (Inside(mouseX, mouseY, layout.rightX + 6.0f,
                layout.contentY + layout.contentHeight - 45.0f,
                layout.rightWidth - 12.0f, 37.0f))
                return {CraftingUiHit::Type::Craft, selected};
            if (Inside(mouseX, mouseY, layout.leftX,
                layout.y + layout.height - 42.0f, 67.0f, 30.0f))
                return {CraftingUiHit::Type::Select, MovePage(selected, -1)};
            if (Inside(mouseX, mouseY,
                layout.leftX + layout.leftWidth - 67.0f,
                layout.y + layout.height - 42.0f, 67.0f, 30.0f))
                return {CraftingUiHit::Type::Select, MovePage(selected, 1)};
            return {};
        }

        void Render(UIRenderer& ui, const InventoryState& state,
            int width, int height, std::size_t selected,
            const std::string& feedback) const
        {
            constexpr UIColor background{0.035f, 0.055f, 0.075f, 0.98f};
            constexpr UIColor panel{0.078f, 0.105f, 0.13f, 1.0f};
            constexpr UIColor tile{0.105f, 0.135f, 0.17f, 1.0f};
            constexpr UIColor active{0.20f, 0.22f, 0.16f, 1.0f};
            constexpr UIColor accent{0.90f, 0.66f, 0.29f, 1.0f};
            constexpr UIColor white{0.92f, 0.94f, 0.96f, 1.0f};
            constexpr UIColor muted{0.61f, 0.68f, 0.74f, 1.0f};
            constexpr UIColor green{0.46f, 0.83f, 0.57f, 1.0f};
            constexpr UIColor missing{0.95f, 0.50f, 0.42f, 1.0f};
            const Layout l = CalculateLayout(width, height);
            if (l.width <= 0.0f || PrimitiveRecipes.empty())
                return;
            selected = std::min(selected, PrimitiveRecipes.size() - 1);
            const int category = CategoryOf(selected);
            const auto entries = CategoryEntries(category);
            const std::size_t position = PositionIn(entries, selected);
            const std::size_t page = position / PageSize;
            const std::size_t first = page * PageSize;
            const std::size_t pages = (entries.size() + PageSize - 1) / PageSize;
            const auto& recipe = PrimitiveRecipes[selected];

            ui.DrawRectangle(l.x, l.y, l.width, l.height, background);
            ui.DrawRectangle(l.x, l.y, l.width, 44.0f, panel);
            ui.DrawRectangle(l.x, l.y, 4.0f, l.height, accent);
            ui.DrawText("CRAFTING", l.x + 17.0f, l.y + 11.0f, 1.25f, white);
            ui.DrawText("C / ESC", l.x + l.width - 67.0f,
                l.y + 14.0f, 0.82f, muted);

            constexpr std::array<const char*, 3> groupTitles{
                "SURVIVAL", "METALWORK", "CONSTRUCTION"};
            const float tabWidth = (l.width - 30.0f) / 3.0f;
            for (int i = 0; i < CategoryCount; ++i)
            {
                const float tabX = l.x + 15.0f + tabWidth * i;
                ui.DrawRectangle(tabX, l.y + 52.0f, tabWidth - 3.0f, 32.0f,
                    i == category ? active : panel);
                FitText(ui, groupTitles[static_cast<std::size_t>(i)],
                    tabX + 9.0f, l.y + 60.0f, tabWidth - 20.0f,
                    0.95f, i == category ? accent : muted);
            }

            for (std::size_t slot = 0; slot < PageSize; ++slot)
            {
                const float cardX = l.leftX + static_cast<float>(slot % 2) *
                    (l.cardWidth + 8.0f);
                const float cardY = l.contentY + static_cast<float>(slot / 2) *
                    (l.cardHeight + 8.0f);
                ui.DrawRectangle(cardX, cardY, l.cardWidth, l.cardHeight, panel);
                if (first + slot >= entries.size())
                    continue;
                const std::size_t recipeIndex = entries[first + slot];
                const auto& option = PrimitiveRecipes[recipeIndex];
                if (recipeIndex == selected)
                {
                    ui.DrawRectangle(cardX + 1.0f, cardY + 1.0f,
                        l.cardWidth - 2.0f, l.cardHeight - 2.0f, active);
                    ui.DrawRectangle(cardX, cardY, 4.0f, l.cardHeight, accent);
                }
                const bool craftable = CanCraft(state, option);
                const float icon = std::clamp(l.cardHeight - 21.0f, 23.0f, 40.0f);
                ui.DrawIcon(option.resultId, cardX + 10.0f,
                    cardY + (l.cardHeight - icon) * 0.5f, icon, icon);
                const float nameX = cardX + icon + 17.0f;
                FitText(ui, option.title, nameX,
                    cardY + l.cardHeight * 0.5f - 15.0f,
                    l.cardWidth - icon - 25.0f, 0.92f, white);
                ui.DrawText(craftable ? "READY" : "NEEDS ITEMS", nameX,
                    cardY + l.cardHeight * 0.5f + 4.0f, 0.70f,
                    craftable ? green : muted);
                ui.DrawText(std::to_string(slot + 1),
                    cardX + l.cardWidth - 18.0f, cardY + 5.0f,
                    0.72f, muted);
            }

            ui.DrawRectangle(l.rightX, l.contentY, l.rightWidth,
                l.contentHeight, panel);
            const float rightTextX = l.rightX + 12.0f;
            FitText(ui, recipe.title, rightTextX, l.contentY + 11.0f,
                l.rightWidth - 24.0f, 1.05f, white);
            ui.DrawText("REQUIRED MATERIALS", rightTextX,
                l.contentY + 43.0f, 0.76f, accent);
            bool craftable = true;
            const float materialStart = l.contentY + 64.0f;
            const float ingredientSpacing = l.contentHeight < 225.0f ? 23.0f : 28.0f;
            for (std::size_t i = 0; i < recipe.ingredientCount; ++i)
            {
                const auto& ingredient = recipe.ingredients[i];
                const std::uint32_t have = CountItem(state, ingredient.itemId);
                const bool enough = have >= ingredient.quantity;
                craftable = craftable && enough;
                const auto* definition = FindPrimitiveItem(ingredient.itemId);
                const std::string name = std::string(definition
                    ? definition->name : ingredient.itemId);
                const float ingredientY = materialStart +
                    static_cast<float>(i) * ingredientSpacing;
                const std::string quantity = std::to_string(have) + "/" +
                    std::to_string(ingredient.quantity);
                const float quantityWidth = ui.MeasureText(quantity, 0.81f);
                FitText(ui, name, rightTextX, ingredientY,
                    l.rightWidth - quantityWidth - 31.0f,
                    0.83f, enough ? green : white);
                ui.DrawText(quantity,
                    l.rightX + l.rightWidth - quantityWidth - 12.0f,
                    ingredientY, 0.81f, enough ? green : missing);
            }
            if (!craftable && l.contentHeight > 250.0f)
                ui.DrawText("Missing materials shown in red", rightTextX,
                    l.contentY + 185.0f, 0.72f, muted);
            const float craftY = l.contentY + l.contentHeight - 45.0f;
            ui.DrawRectangle(l.rightX + 6.0f, craftY,
                l.rightWidth - 12.0f, 37.0f,
                craftable ? active : tile);
            FitText(ui, craftable ? "CRAFT [ENTER]" : "MISSING MATERIALS",
                l.rightX + 16.0f, craftY + 10.0f,
                l.rightWidth - 32.0f, 0.91f,
                craftable ? green : muted);

            ui.DrawRectangle(l.leftX, l.y + l.height - 42.0f,
                67.0f, 29.0f, tile);
            ui.DrawRectangle(l.leftX + l.leftWidth - 67.0f,
                l.y + l.height - 42.0f, 67.0f, 29.0f, tile);
            ui.DrawText("< PREV", l.leftX + 9.0f,
                l.y + l.height - 34.0f, 0.74f, muted);
            ui.DrawText("NEXT >", l.leftX + l.leftWidth - 60.0f,
                l.y + l.height - 34.0f, 0.74f, muted);
            const std::string count = std::to_string(page + 1) + "/" +
                std::to_string(pages);
            ui.DrawText(count, l.leftX + l.leftWidth * 0.45f,
                l.y + l.height - 34.0f, 0.80f, accent);
            if (!feedback.empty())
                FitText(ui, feedback, l.rightX + 8.0f,
                    l.y + l.height - 34.0f, l.rightWidth - 12.0f,
                    0.79f, white);
            else
                FitText(ui, "ARROWS: SELECT  |  LEFT/RIGHT: GROUP",
                    l.rightX + 8.0f, l.y + l.height - 34.0f,
                    l.rightWidth - 12.0f, 0.72f, muted);
        }

    private:
        struct Layout
        {
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
            float leftX = 0.0f;
            float leftWidth = 0.0f;
            float cardWidth = 0.0f;
            float cardHeight = 0.0f;
            float rightX = 0.0f;
            float rightWidth = 0.0f;
            float contentY = 0.0f;
            float contentHeight = 0.0f;
        };

        static Layout CalculateLayout(int width, int height)
        {
            Layout l;
            if (width < 520 || height < 360)
                return l;
            l.width = std::min(980.0f, static_cast<float>(width) - 16.0f);
            l.height = std::min(650.0f, static_cast<float>(height) - 16.0f);
            l.x = (static_cast<float>(width) - l.width) * 0.5f;
            l.y = (static_cast<float>(height) - l.height) * 0.5f;
            l.leftX = l.x + 15.0f;
            l.leftWidth = (l.width - 42.0f) * 0.55f;
            l.rightX = l.leftX + l.leftWidth + 12.0f;
            l.rightWidth = l.x + l.width - l.rightX - 15.0f;
            l.contentY = l.y + 94.0f;
            l.contentHeight = l.height - 146.0f;
            l.cardWidth = (l.leftWidth - 8.0f) * 0.5f;
            l.cardHeight = (l.contentHeight - 16.0f) / 3.0f;
            return l;
        }

        static bool Inside(float x, float y, float rx,
            float ry, float width, float height)
        {
            return x >= rx && y >= ry && x < rx + width && y < ry + height;
        }

        static std::size_t PositionIn(
            const std::vector<std::size_t>& entries, std::size_t selected)
        {
            const auto it = std::find(entries.begin(), entries.end(), selected);
            return it == entries.end() ? 0 :
                static_cast<std::size_t>(it - entries.begin());
        }

        static std::uint32_t CountItem(
            const InventoryState& state, std::string_view id)
        {
            std::uint32_t result = 0;
            for (const auto& item : state.items)
                if (item.itemId == id)
                    result += item.quantity;
            return result;
        }

        static bool CanCraft(
            const InventoryState& state, const PrimitiveRecipe& recipe)
        {
            for (std::size_t i = 0; i < recipe.ingredientCount; ++i)
                if (CountItem(state, recipe.ingredients[i].itemId) <
                    recipe.ingredients[i].quantity)
                    return false;
            return true;
        }

        static void FitText(UIRenderer& ui, std::string_view label,
            float x, float y, float availableWidth, float scale, UIColor color)
        {
            if (availableWidth < 15.0f)
                return;
            std::string value(label);
            while (!value.empty() && ui.MeasureText(value, scale) > availableWidth)
                value.pop_back();
            if (value.size() < label.size() && value.size() > 3)
            {
                value.resize(value.size() - 3);
                value += "...";
            }
            ui.DrawText(value, x, y, scale, color);
        }
    };
}
