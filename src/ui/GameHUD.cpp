#include "industrial_frontier/ui/GameHUD.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

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

        if (m_showDetails && height >= 300 && world.hasTarget &&
            world.targetMaterial != nullptr &&
            world.targetMaterial[0] != '\0' &&
            std::string_view(world.targetMaterial) != "NONE")
        {
            const std::string target(world.targetMaterial);
            const float scale = 1.12f;
            const float labelWidth = ui.MeasureText(target, scale);
            const float x = (static_cast<float>(width) - labelWidth) * 0.5f;
            const float y = static_cast<float>(height) * 0.5f + 48.0f;
            ui.DrawRectangle(x - 12.0f, y - 7.0f,
                labelWidth + 24.0f, 24.0f, Backdrop);
            ui.DrawText(target, x, y, scale, Text);
        }

        if (m_feedbackRemaining > 0.0f && height >= 280)
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
            const std::string hints = "LMB DIG    F3 HUD    F5 SAVE    F9 LOAD    ESC PAUSE";
            const float scale = 1.0f;
            const float x = static_cast<float>(width) -
                ui.MeasureText(hints, scale) - 18.0f;
            ui.DrawText(hints, std::max(x, margin + panelWidth + 16.0f),
                static_cast<float>(height) - 19.0f, scale, Secondary);
        }
    }
}
