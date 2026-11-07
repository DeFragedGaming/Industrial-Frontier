#pragma once

#include "industrial_frontier/ui/UIRenderer.hpp"
#include "industrial_frontier/renderer/Renderer.hpp"

namespace industrial_frontier
{
    class GameHUD
    {
    public:
        void Reset();
        void Update(float deltaTime);
        void ToggleDetails();
        void ReportDig(bool success);
        void Render(UIRenderer& ui, const HUDWorldState& world, int width, int height) const;

    private:
        bool m_showDetails = true;
        bool m_lastDigSucceeded = false;
        float m_feedbackRemaining = 0.0f;
        float m_fpsAccumulated = 0.0f;
        int m_frameCount = 0;
        int m_displayFps = 0;
    };
}
