
#pragma once

#include "industrial_frontier/core/GameFlow.hpp"
#include "industrial_frontier/ui/UIRenderer.hpp"

namespace industrial_frontier
{
    enum class MenuAction
    {
        None,
        NewGame,
        Resume,
        MainMenu,
        Settings,
        Credits,
        Back,
        Quit,
        SkipIntro,
        NextStory
    };

    class MainMenu
    {
    public:
        void Render(
            UIRenderer& ui,
            const GameFlow& flow,
            int width,
            int height,
            float mouseX,
            float mouseY
        ) const;

        MenuAction HandleClick(
            const GameFlow& flow,
            int width,
            int height,
            float mouseX,
            float mouseY
        ) const;

    private:
        struct Rect
        {
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;

            bool Contains(float px, float py) const;
        };

        Rect GetPanel(int width, int height) const;
        Rect GetButton(int width, int height, int index) const;

        void DrawButton(
            UIRenderer& ui,
            const Rect& rect,
            const char* label,
            bool hovered,
            bool enabled = true
        ) const;

        void DrawMainMenu(
            UIRenderer& ui,
            int width,
            int height,
            float mouseX,
            float mouseY
        ) const;

        void DrawPauseMenu(
            UIRenderer& ui,
            int width,
            int height,
            float mouseX,
            float mouseY
        ) const;

        void DrawStory(
            UIRenderer& ui,
            const GameFlow& flow,
            int width,
            int height
        ) const;

        void DrawSubmenu(
            UIRenderer& ui,
            GameState state,
            int width,
            int height,
            float mouseX,
            float mouseY
        ) const;
    };
}
