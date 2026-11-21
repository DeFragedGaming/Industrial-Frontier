#include "industrial_frontier/ui/PrimitiveCraftingOverlay.hpp"

#include <cstdlib>
#include <memory>
#include <string_view>
#include <vector>
#include <utility>
#include <iostream>

namespace
{
    struct DrawBounds { float x; float y; float w; float h; };
    std::vector<DrawBounds> drawn;
}

namespace industrial_frontier
{
    struct UIRenderer::Impl {};
    UIRenderer::UIRenderer() : m_impl(std::make_unique<Impl>()) {}
    UIRenderer::~UIRenderer() = default;
    void UIRenderer::DrawRectangle(float x, float y, float w, float h, UIColor)
    {
        drawn.push_back({x,y,w,h});
    }
    void UIRenderer::DrawIcon(std::string_view,float x,float y,float w,float h)
    {
        drawn.push_back({x,y,w,h});
    }
    void UIRenderer::DrawText(std::string_view value, float x, float y,
        float scale, UIColor)
    {
        drawn.push_back({x,y,MeasureText(value,scale),TextHeight(scale)});
    }
    float UIRenderer::MeasureText(std::string_view value, float scale)
    {
        return static_cast<float>(value.size()) * 7.0f * scale;
    }
    float UIRenderer::TextHeight(float scale) const
    {
        return 12.0f * scale;
    }
}

using namespace industrial_frontier;

static void Check(bool value, const char* label)
{
    if (!value)
    {
        std::cerr << "FAIL: " << label << '\n';
        std::exit(1);
    }
}

int main()
{
    Check(PrimitiveCraftingOverlay::CategoryEntries(0).size() == 4,
        "survival category has stone equipment and firemaking");
    Check(PrimitiveCraftingOverlay::CategoryEntries(1).size() == 6,
        "metallurgy category has furnace mold casting and copper tools");
    Check(PrimitiveCraftingOverlay::CategoryEntries(2).size() == 10,
        "construction category includes crushers mills and sluices");

    const std::size_t metallurgy = PrimitiveCraftingOverlay::SelectCategory(0, 1);
    Check(metallurgy == 4, "keyboard next category opens furnace");
    const std::size_t building = PrimitiveCraftingOverlay::SelectCategory(metallurgy, 1);
    Check(building == 10, "keyboard building opens chest");
    Check(PrimitiveCraftingOverlay::MoveSelection(building, -1) == building,
        "cannot select a recipe before first building kit");
    Check(PrimitiveCraftingOverlay::MoveSelection(building, 5) == 15,
        "select first six buildings");
    Check(PrimitiveCraftingOverlay::MovePage(building, 1) == 16,
        "second construction page displays support");
    Check(PrimitiveCraftingOverlay::MovePage(16, -1) == 10,
        "previous construction page works");
    Check(PrimitiveCraftingOverlay::SelectVisibleSlot(10, 5) == 15,
        "number six selects last recipe on first page");
    Check(PrimitiveCraftingOverlay::SelectVisibleSlot(16, 3) == 19,
        "last second-page tile selects the new sluice recipe");
    Check(PrimitiveCraftingOverlay::SelectVisibleSlot(16, 5) == 16,
        "empty second-page slots do not select non-existent recipes");
    Check(PrimitiveCraftingOverlay::SelectCategory(building, 1) == 0,
        "categories wrap around");

    PrimitiveCraftingOverlay overlay;
    for (const auto& [width, height] :
        {std::pair{614,386}, std::pair{1280,720}, std::pair{1920,1080}})
    {
        const float x = width * 0.5f;
        const float y = height * 0.5f;
        const auto click = overlay.Click(x - 150.0f, y - 70.0f,
            width, height, building);
        Check(click.type == CraftingUiHit::Type::Select ||
              click.type == CraftingUiHit::Type::None,
            "clicking crafting panel never opens gameplay");
    }

    UIRenderer renderer;
    for (const auto& [width, height] :
        {std::pair{614,386}, std::pair{1280,720}, std::pair{1920,1080}})
    {
        for (std::size_t recipe : {std::size_t{0}, std::size_t{4},
            std::size_t{10}, std::size_t{16}, std::size_t{19}})
        {
            drawn.clear();
            InventoryState inventory{};
            overlay.Render(renderer, inventory, width, height, recipe, "");
            Check(drawn.size() > 35, "all crafting elements render");
            for (const auto& r : drawn)
            {
                Check(r.w >= 0.0f && r.h >= 0.0f,
                    "UI elements have nonnegative extent");
                Check(r.x >= -0.2f && r.y >= -0.2f &&
                    r.x + r.w <= static_cast<float>(width) + 0.2f &&
                    r.y + r.h <= static_cast<float>(height) + 0.2f,
                    "crafting UI contents fit within viewport");
            }
        }
    }

    const auto tab = overlay.Click(105.0f, 77.0f, 614, 386, 0);
    Check(tab.type == CraftingUiHit::Type::Select && tab.selected == 0,
        "survival tab clickable on narrow viewport");
    const auto metalTab = overlay.Click(300.0f,77.0f,614,386,0);
    Check(metalTab.type == CraftingUiHit::Type::Select && metalTab.selected == 4,
        "metalwork tab clickable on narrow viewport");
    const auto buildTab = overlay.Click(505.0f,77.0f,614,386,0);
    Check(buildTab.type == CraftingUiHit::Type::Select && buildTab.selected == 10,
        "construction tab clickable on narrow viewport");
    const auto first = overlay.Click(50.0f,112.0f,614,386,10);
    Check(first.type == CraftingUiHit::Type::Select && first.selected == 10,
        "recipe tile selectable by mouse");
    const auto second = overlay.Click(204.0f,112.0f,614,386,10);
    Check(second.type == CraftingUiHit::Type::Select && second.selected == 11,
        "second recipe tile selectable by mouse");
    const auto craft = overlay.Click(490.0f,288.0f,614,386,10);
    Check(craft.type == CraftingUiHit::Type::Craft,
        "craft button clickable with small viewport");
    const auto close = overlay.Click(568.0f,25.0f,614,386,10);
    Check(close.type == CraftingUiHit::Type::Close,
        "close button clickable with small viewport");
    std::cout << "Crafting categories, six-tile pages, keyboard navigation and mouse layout passed\n";
}
