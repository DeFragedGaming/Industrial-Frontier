#include "industrial_frontier/ui/ProcessingOverlay.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <string>

namespace industrial_frontier
{
    namespace
    {
        constexpr UIColor Backdrop{0.025f, 0.040f, 0.065f, 0.98f};
        constexpr UIColor Header{0.11f, 0.14f, 0.19f, 1.0f};
        constexpr UIColor Accent{0.85f, 0.61f, 0.27f, 1.0f};
        constexpr UIColor Text{0.94f, 0.95f, 0.98f, 1.0f};
        constexpr UIColor Muted{0.68f, 0.73f, 0.78f, 1.0f};
        constexpr UIColor Button{0.15f, 0.22f, 0.27f, 1.0f};
        constexpr UIColor Selected{0.22f, 0.18f, 0.10f, 1.0f};
        constexpr float PanelHeight = 610.0f;
        constexpr std::array<ProcessingOperation, 5> Operations{{
            ProcessingOperation::Crush, ProcessingOperation::Grind,
            ProcessingOperation::Roast, ProcessingOperation::MagneticSeparation,
            ProcessingOperation::Flotation
        }};

        bool Within(float px, float py, float x, float y, float w, float h)
        {
            return px >= x && py >= y && px <= x + w && py <= y + h;
        }

        std::string Format(float value, int decimals = 1)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(decimals) << value;
            return stream.str();
        }

        std::string Fit(UIRenderer& ui, std::string text, float width, float scale)
        {
            while (text.size() > 4 && ui.MeasureText(text, scale) > width)
            {
                text.pop_back();
            }
            return text;
        }
    }

    float ProcessingOverlay::PanelWidth(int width)
    {
        return std::max(0.0f, std::min(1030.0f, static_cast<float>(width) - 26.0f));
    }

    float ProcessingOverlay::PanelX(int width)
    {
        return (static_cast<float>(width) - PanelWidth(width)) * 0.5f;
    }

    float ProcessingOverlay::PanelY(int height)
    {
        return std::max(8.0f, (static_cast<float>(height) - PanelHeight) * 0.5f);
    }

    ProcessingClick ProcessingOverlay::Click(int width, int height, float x, float y,
        const InventoryState& inventory, std::size_t selectedSlot) const
    {
        ProcessingClick result;
        if (width < 650 || height < 610) return result;
        const float pw = PanelWidth(width);
        const float left = PanelX(width);
        const float top = PanelY(height);
        if (Within(x, y, left + pw - 110.0f, top + 18.0f, 88.0f, 38.0f))
        {
            result.close = true;
            return result;
        }
        const float listX = left + 22.0f;
        const float listY = top + 112.0f;
        const float listWidth = pw * 0.39f - 28.0f;
        const std::size_t page = selectedSlot / 9;
        for (std::size_t row = 0; row < 9; ++row)
        {
            const std::size_t index = page * 9 + row;
            if (index < inventory.oreBatches.size() &&
                Within(x, y, listX, listY + 42.0f * row, listWidth, 38.0f))
            {
                result.changedSelection = true;
                result.selectedSlot = index;
                return result;
            }
        }
        if (Within(x, y, listX, top + 515.0f, 110.0f, 34.0f) && page > 0)
        {
            result.changedSelection = true;
            result.selectedSlot = (page - 1) * 9;
            return result;
        }
        if (Within(x, y, listX + 120.0f, top + 515.0f, 110.0f, 34.0f) &&
            (page + 1) * 9 < inventory.oreBatches.size())
        {
            result.changedSelection = true;
            result.selectedSlot = (page + 1) * 9;
            return result;
        }
        const float actionX = left + pw * 0.43f;
        const float actionWidth = pw * 0.52f;
        for (std::size_t i = 0; i < Operations.size(); ++i)
        {
            if (Within(x, y, actionX, top + 278.0f + 48.0f * i,
                       actionWidth, 40.0f))
            {
                result.run = true;
                result.operation = Operations[i];
                return result;
            }
        }
        return result;
    }

    void ProcessingOverlay::Render(UIRenderer& ui, const InventoryState& inventory,
        int width, int height, std::size_t selectedSlot, const std::string& message) const
    {
        if (width < 650 || height < 610) return;
        const float pw = PanelWidth(width);
        const float left = PanelX(width);
        const float top = PanelY(height);
        ui.DrawRectangle(0, 0, static_cast<float>(width),
                         static_cast<float>(height), {0, 0, 0, 0.57f});
        ui.DrawRectangle(left, top, pw, PanelHeight, Backdrop);
        ui.DrawRectangle(left, top, 5.0f, PanelHeight, Accent);
        ui.DrawRectangle(left, top, pw, 78.0f, Header);
        ui.DrawText("MINERAL PROCESSING", left + 24.0f, top + 20.0f, 1.85f, Text);
        ui.DrawText("STAGE 10E  |  DEVELOPMENT PROCESSING BENCH", left + 24.0f,
                    top + 52.0f, 0.85f, Accent);
        const float closeX = left + pw - 110.0f;
        ui.DrawRectangle(closeX, top + 18.0f, 88.0f, 38.0f, Button);
        ui.DrawText("CLOSE", closeX + 13.0f, top + 27.0f, 1.1f, Text);

        const float listX = left + 22.0f;
        const float listY = top + 112.0f;
        const float listWidth = pw * 0.39f - 28.0f;
        ui.DrawText("STORED BATCHES", listX, top + 87.0f, 1.1f, Accent);
        const std::size_t page = selectedSlot / 9;
        for (std::size_t row = 0; row < 9; ++row)
        {
            const std::size_t index = page * 9 + row;
            if (index >= inventory.oreBatches.size()) break;
            const auto& batch = inventory.oreBatches[index];
            const float ry = listY + 42.0f * row;
            ui.DrawRectangle(listX, ry, listWidth, 38.0f,
                index == selectedSlot ? Selected : Header);
            ui.DrawText(Fit(ui, batch.displayName, listWidth - 105.0f, 0.91f),
                listX + 8.0f, ry + 9.0f, 0.91f, Text);
            const std::string mass = Format(batch.massKg) + " kg";
            ui.DrawText(mass, listX + listWidth - ui.MeasureText(mass, 0.84f) - 9.0f,
                ry + 11.0f, 0.84f, Accent);
        }
        ui.DrawRectangle(listX, top + 515.0f, 110.0f, 34.0f, Button);
        ui.DrawText("PREV PAGE", listX + 10.0f, top + 523.0f, 0.85f, Text);
        ui.DrawRectangle(listX + 120.0f, top + 515.0f, 110.0f, 34.0f, Button);
        ui.DrawText("NEXT PAGE", listX + 130.0f, top + 523.0f, 0.85f, Text);
        ui.DrawText("SELECT BATCH OR USE ARROWS", listX, top + 569.0f, 0.84f, Muted);

        const float actionX = left + pw * 0.43f;
        const float actionWidth = pw * 0.52f;
        ui.DrawText("SELECTED MATERIAL", actionX, top + 87.0f, 1.1f, Accent);
        if (selectedSlot < inventory.oreBatches.size())
        {
            const auto& batch = inventory.oreBatches[selectedSlot];
            ui.DrawText(Fit(ui, batch.displayName, actionWidth, 1.15f),
                actionX, top + 119.0f, 1.15f, Text);
            ui.DrawText(Format(batch.massKg, 2) + " KG  |  " +
                OreProcessing::StageName(batch.processingStage), actionX,
                top + 151.0f, 1.01f, Muted);
            ui.DrawText("ORE MINERALS: " + Format(batch.oreMineralFraction * 100.0f) + "%",
                actionX, top + 185.0f, 0.98f, Accent);
            ui.DrawText("GANGUE: " + Format((1.0f - batch.oreMineralFraction) * 100.0f) + "%",
                actionX, top + 207.0f, 0.98f, Muted);
        }
        else
        {
            ui.DrawText("Mine ore and choose a batch on the left",
                actionX, top + 124.0f, 0.95f, Muted);
        }
        for (std::size_t i = 0; i < Operations.size(); ++i)
        {
            const float ry = top + 278.0f + 48.0f * i;
            ui.DrawRectangle(actionX, ry, actionWidth, 40.0f, Button);
            const std::string label = std::to_string(i + 1) + "  " +
                OreProcessing::OperationName(Operations[i]);
            ui.DrawText(label, actionX + 14.0f, ry + 10.0f, 1.05f, Text);
        }
        ui.DrawText("Crush > Grind > Roast (iron, optional) > Separate",
            actionX, top + 533.0f, 0.83f, Muted);
        const std::string displayMessage = message.empty() ?
            "Select a batch. Outputs remain in your saved inventory." : message;
        ui.DrawText(Fit(ui, displayMessage, actionWidth, 0.82f),
            actionX, top + 567.0f, 0.82f, Accent);
    }
}
