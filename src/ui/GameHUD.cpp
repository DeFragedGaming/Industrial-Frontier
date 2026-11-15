#include "industrial_frontier/ui/GameHUD.hpp"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        constexpr UIColor Accent{0.95f, 0.49f, 0.13f, 1.0f};
        constexpr UIColor Text{0.94f, 0.95f, 0.97f, 1.0f};
        constexpr UIColor Secondary{0.68f, 0.74f, 0.78f, 1.0f};
        constexpr UIColor Backdrop{0.02f, 0.035f, 0.055f, 0.79f};
        constexpr UIColor Shadow{0.012f, 0.018f, 0.025f, 0.96f};
        constexpr UIColor Alert{0.95f, 0.74f, 0.32f, 1.0f};

        void DrawCentered(UIRenderer& ui, std::string_view value, float centerX,
            float y, float scale, const UIColor& color)
        {
            ui.DrawText(value, centerX - ui.MeasureText(value, scale) * 0.5f,
                y, scale, color);
        }

        std::vector<std::string> WrapAssay(UIRenderer& ui,
            const std::string& assay, float maximumWidth, float scale)
        {
            std::vector<std::string> lines;
            std::string normalized;
            normalized.reserve(assay.size() + 4);
            for (std::size_t i = 0; i < assay.size(); ++i)
            {
                if (i + 1 < assay.size() && assay[i] == ' ' && assay[i + 1] == ' ')
                {
                    normalized += '\n';
                    while (i + 1 < assay.size() && assay[i + 1] == ' ')
                    {
                        ++i;
                    }
                    continue;
                }
                normalized += assay[i];
            }

            std::size_t separator = 0;
            while ((separator = normalized.find(", ", separator)) != std::string::npos)
            {
                normalized.replace(separator, 2, "\n");
                ++separator;
            }

            const auto gangue = normalized.find("GANGUE:");
            if (gangue != std::string::npos && gangue > 0 &&
                normalized[gangue - 1] != '\n')
            {
                normalized.insert(gangue, "\n");
            }

            std::istringstream input(normalized);
            std::string section;
            while (std::getline(input, section))
            {
                std::istringstream words(section);
                std::string line;
                std::string word;
                while (words >> word)
                {
                    const std::string candidate = line.empty() ? word : line + " " + word;
                    if (!line.empty() && ui.MeasureText(candidate, scale) > maximumWidth)
                    {
                        lines.push_back(line);
                        line = word;
                    }
                    else
                    {
                        line = candidate;
                    }
                }
                if (!line.empty())
                {
                    lines.push_back(line);
                }
            }
            return lines;
        }

        std::string PositionString(const HUDWorldState& world)
        {
            std::ostringstream stream;
            stream << std::fixed << std::setprecision(1)
                   << "X " << world.x
                   << "   Y " << world.y
                   << "   Z " << world.z;
            return stream.str();
        }

        void DrawCrosshair(UIRenderer& ui, int width, int height, bool pulse)
        {
            const float cx = static_cast<float>(width) * 0.5f;
            const float cy = static_cast<float>(height) * 0.5f;
            const float scale = std::clamp(
                static_cast<float>(std::min(width, height)) / 720.0f,
                0.8f, 1.4f);
            const float gap = 5.0f * scale;
            const float length = 8.0f * scale;
            const float thickness = std::max(2.0f * scale, 1.6f);
            const UIColor color = pulse ? Alert : Accent;

            const auto segment = [&](float x, float y, float w, float h)
            {
                ui.DrawRectangle(x - 1.0f, y - 1.0f,
                    w + 2.0f, h + 2.0f, Shadow);
                ui.DrawRectangle(x, y, w, h, color);
            };

            segment(cx - gap - length, cy - thickness * 0.5f,
                length, thickness);
            segment(cx + gap, cy - thickness * 0.5f,
                length, thickness);
            segment(cx - thickness * 0.5f, cy - gap - length,
                thickness, length);
            segment(cx - thickness * 0.5f, cy + gap,
                thickness, length);
            ui.DrawRectangle(cx - 1.0f, cy - 1.0f, 2.0f, 2.0f, Text);
        }
    }

    void GameHUD::Reset()
    {
        m_feedbackRemaining = 0.0f;
        m_fpsAccumulated = 0.0f;
        m_frameCount = 0;
        m_displayFps = 0;
    }

    void GameHUD::Update(float deltaTime)
    {
        const float elapsed = std::clamp(deltaTime, 0.0f, 0.5f);
        m_feedbackRemaining = std::max(0.0f, m_feedbackRemaining - elapsed);
        m_fpsAccumulated += elapsed;
        ++m_frameCount;
        if (m_fpsAccumulated >= 0.5f)
        {
            m_displayFps = static_cast<int>(
                std::round(static_cast<float>(m_frameCount) / m_fpsAccumulated));
            m_fpsAccumulated = 0.0f;
            m_frameCount = 0;
        }
    }

    void GameHUD::ToggleDetails()
    {
        m_showDetails = !m_showDetails;
    }

    void GameHUD::ReportDig(bool success)
    {
        m_lastDigSucceeded = success;
        m_feedbackRemaining = success ? 0.55f : 0.35f;
    }

    void GameHUD::Render(
        UIRenderer& ui,
        const HUDWorldState& world,
        int width,
        int height) const
    {
        if (width <= 0 || height <= 0)
        {
            return;
        }

        DrawCrosshair(ui, width, height,
            m_lastDigSucceeded && m_feedbackRemaining > 0.0f);

        float targetPanelBottom = static_cast<float>(height) * 0.5f + 48.0f;
        if (m_showDetails && height >= 300 && world.hasTarget &&
            !world.targetMaterial.empty() &&
            std::string_view(world.targetMaterial) != "NONE")
        {
            const float centerX = static_cast<float>(width) * 0.5f;
            const float panelWidth = std::min(410.0f,
                static_cast<float>(width) - 28.0f);
            const float panelX = centerX - panelWidth * 0.5f;
            const float panelY = static_cast<float>(height) * 0.5f + 43.0f;
            const float titleScale = 1.12f;
            const float bodyScale = 0.98f;
            const float availableWidth = panelWidth - 28.0f;

            std::vector<std::string> detailLines;
            if (world.targetIsOre && !world.targetAssay.empty())
            {
                detailLines = WrapAssay(ui, world.targetAssay,
                    availableWidth, bodyScale);
                if (detailLines.size() > 3)
                {
                    detailLines.resize(3);
                    detailLines.back() = "MORE DETAILS: PRESS G FOR THE GUIDE";
                }
            }

            const float titleHeight = std::max(18.0f, ui.TextHeight(titleScale) + 4.0f);
            const float bodyHeight = std::max(18.0f, ui.TextHeight(bodyScale) + 4.0f);
            const float panelHeight = 13.0f + titleHeight +
                static_cast<float>(detailLines.size()) * bodyHeight + 9.0f;

            ui.DrawRectangle(panelX, panelY, panelWidth, panelHeight, Backdrop);
            ui.DrawRectangle(panelX, panelY, panelWidth, 2.0f, Accent);
            DrawCentered(ui, world.targetMaterial, centerX,
                panelY + 9.0f, titleScale, Text);

            float lineY = panelY + 9.0f + titleHeight;
            for (const std::string& line : detailLines)
            {
                DrawCentered(ui, line, centerX, lineY, bodyScale, Secondary);
                lineY += bodyHeight;
            }
            targetPanelBottom = panelY + panelHeight;
        }

        if (world.miningActive && height >= 280)
        {
            const float centerX = static_cast<float>(width) * 0.5f;
            const float centerY = static_cast<float>(height) * 0.5f;
            const float barWidth = std::min(310.0f,
                static_cast<float>(width) - 40.0f);
            const float barX = centerX - barWidth * 0.5f;
            const float barY = std::max(centerY + 81.0f,
                targetPanelBottom + 13.0f);
            const float progress = std::clamp(world.miningProgress, 0.0f, 1.0f);
            ui.DrawRectangle(barX - 3.0f, barY - 3.0f,
                barWidth + 6.0f, 20.0f, Shadow);
            ui.DrawRectangle(barX, barY, barWidth, 14.0f, Backdrop);
            ui.DrawRectangle(barX, barY, barWidth * progress, 14.0f, Accent);
            const bool chopping = world.miningName.find("TREE") != std::string::npos;
            std::string label = std::string(chopping ? "CHOPPING " : "MINING ") +
                world.miningName + "  " +
                std::to_string(world.miningHits) + "/" +
                std::to_string(world.miningHitsRequired) + " HITS";
            std::transform(label.begin(), label.end(), label.begin(),
                [](unsigned char c)
                {
                    return static_cast<char>(std::toupper(c));
                });
            const float textWidth = ui.MeasureText(label, 1.02f);
            ui.DrawText(label, centerX - textWidth * 0.5f,
                barY + 22.0f, 1.02f, Text);
        }
        else if (!world.miningFeedback.empty() && height >= 280)
        {
            const float textWidth = ui.MeasureText(world.miningFeedback, 0.95f);
            ui.DrawText(world.miningFeedback,
                (static_cast<float>(width) - textWidth) * 0.5f,
                static_cast<float>(height) * 0.5f + 82.0f,
                0.95f, Accent);
        }

        if (!world.miningActive && m_feedbackRemaining > 0.0f && height >= 280)
        {
            const std::string feedback = m_lastDigSucceeded
                ? "EXCAVATING" : "NOTHING IN REACH";
            const float scale = 1.05f;
            const float x = std::max(8.0f,
                (static_cast<float>(width) - ui.MeasureText(feedback, scale)) * 0.5f);
            ui.DrawText(feedback, x,
                static_cast<float>(height) * 0.5f + 79.0f,
                scale, m_lastDigSucceeded ? Accent : Secondary);
        }

        if (!m_showDetails || width < 420 || height < 300)
        {
            return;
        }

        const float margin = 18.0f;
        const float panelWidth = std::min(360.0f,
            static_cast<float>(width) - margin * 2.0f);
        const float panelHeight = 90.0f;
        const float panelY = static_cast<float>(height) - panelHeight - margin;
        ui.DrawRectangle(margin, panelY,
            panelWidth, panelHeight, Backdrop);
        ui.DrawRectangle(margin, panelY, 3.0f, panelHeight, Accent);
        ui.DrawText("LOCATION", margin + 15.0f, panelY + 10.0f,
            1.1f, Accent);
        ui.DrawText(PositionString(world), margin + 15.0f,
            panelY + 33.0f, 1.25f, Text);

        std::ostringstream details;
        details << "CHUNKS " << world.loadedChunks
                << "   FPS " << (m_displayFps > 0
                    ? std::to_string(m_displayFps) : "--")
                << "   " << (world.grounded ? "GROUNDED" : "AIRBORNE");
        ui.DrawText(details.str(), margin + 15.0f,
            panelY + 62.0f, 1.05f, Secondary);

        if (world.terrainUpdating && width >= 760)
        {
            const std::string message = "UPDATING TERRAIN";
            const float scale = 1.15f;
            const float messageWidth = ui.MeasureText(message, scale);
            const float x = static_cast<float>(width) - messageWidth - 24.0f;
            const float y = static_cast<float>(height) - 52.0f;
            ui.DrawText(message, x, y, scale, Alert);
        }

        if (width >= 950)
        {
            const std::string hints = "LMB DIG/CHOP    E GATHER    C CRAFT    I BAG    F5 SAVE    ESC PAUSE";
            const float scale = 1.0f;
            const float x = static_cast<float>(width) -
                ui.MeasureText(hints, scale) - 18.0f;
            ui.DrawText(hints, std::max(x, margin + panelWidth + 16.0f),
                static_cast<float>(height) - 19.0f, scale, Secondary);
        }
    }
}
