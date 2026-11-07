
#include "industrial_frontier/ui/MainMenu.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        constexpr UIColor Orange{
            0.95f, 0.49f, 0.13f, 1.0f
        };

        constexpr UIColor White{
            0.94f, 0.95f, 0.97f, 1.0f
        };

        constexpr UIColor Muted{
            0.62f, 0.68f, 0.74f, 1.0f
        };

        constexpr UIColor Panel{
            0.025f, 0.035f, 0.055f, 0.92f
        };

        constexpr UIColor Button{
            0.10f, 0.14f, 0.19f, 0.94f
        };

        constexpr UIColor Hover{
            0.27f, 0.19f, 0.12f, 0.98f
        };

        constexpr UIColor Disabled{
            0.38f, 0.40f, 0.44f, 0.7f
        };

        void DrawCenteredText(
            UIRenderer& ui,
            std::string_view text,
            float centerX,
            float y,
            float scale,
            const UIColor& color)
        {
            ui.DrawText(
                text,
                centerX - ui.MeasureText(text, scale) * 0.5f,
                y,
                scale,
                color
            );
        }

        std::vector<std::string> WrapText(
            std::string_view text,
            std::size_t maximumCharacters)
        {
            std::vector<std::string> lines;
            std::string line;
            std::string word;

            auto appendWord = [&]()
            {
                if (word.empty())
                {
                    return;
                }

                if (!line.empty() &&
                    line.size() + word.size() + 1 >
                        maximumCharacters)
                {
                    lines.push_back(line);
                    line.clear();
                }

                if (!line.empty())
                {
                    line += ' ';
                }

                line += word;
                word.clear();
            };

            for (const char c : text)
            {
                if (c == ' ' || c == '\n')
                {
                    appendWord();

                    if (c == '\n')
                    {
                        lines.push_back(line);
                        line.clear();
                    }
                }
                else
                {
                    word += c;
                }
            }

            appendWord();

            if (!line.empty())
            {
                lines.push_back(line);
            }

            return lines;
        }
    }

    bool MainMenu::Rect::Contains(
        float px,
        float py) const
    {
        return px >= x &&
               py >= y &&
               px < x + width &&
               py < y + height;
    }

    MainMenu::Rect MainMenu::GetPanel(
        int width,
        int height) const
    {
        const float panelWidth = std::min(
            430.0f,
            static_cast<float>(width) * 0.90f
        );

        const float panelHeight = std::min(
            590.0f,
            static_cast<float>(height) * 0.94f
        );

        return {
            24.0f,
            (static_cast<float>(height) - panelHeight) * 0.5f,
            panelWidth,
            panelHeight
        };
    }

    MainMenu::Rect MainMenu::GetButton(
        int width,
        int height,
        int index) const
    {
        const Rect panel = GetPanel(width, height);

        const float buttonHeight = std::clamp(
            panel.height * 0.078f,
            30.0f,
            46.0f
        );

        const float spacing = std::clamp(
            panel.height * 0.013f,
            5.0f,
            9.0f
        );

        return {
            panel.x + 30.0f,
            panel.y + panel.height * 0.32f +
                static_cast<float>(index) *
                    (buttonHeight + spacing),
            panel.width - 60.0f,
            buttonHeight
        };
    }

    void MainMenu::DrawButton(
        UIRenderer& ui,
        const Rect& rect,
        const char* label,
        bool hovered,
        bool enabled) const
    {
        ui.DrawRectangle(
            rect.x,
            rect.y,
            rect.width,
            rect.height,
            enabled ? (hovered ? Hover : Button) : Panel
        );

        ui.DrawRectangle(
            rect.x,
            rect.y,
            4.0f,
            rect.height,
            enabled ? Orange : Disabled
        );

        const float scale = std::clamp(
            rect.height / 20.0f,
            1.3f,
            2.1f
        );

        const float textY =
            rect.y + (rect.height - 7.0f * scale) * 0.5f;

        ui.DrawText(
            label,
            rect.x + 20.0f,
            textY,
            scale,
            enabled ? White : Disabled
        );
    }

    void MainMenu::DrawMainMenu(
        UIRenderer& ui,
        int width,
        int height,
        float mouseX,
        float mouseY,
        bool canContinue) const
    {
        const Rect panel = GetPanel(width, height);

        ui.DrawRectangle(
            panel.x,
            panel.y,
            panel.width,
            panel.height,
            Panel
        );

        ui.DrawRectangle(
            panel.x,
            panel.y,
            5.0f,
            panel.height,
            Orange
        );

        const float titleScale = std::clamp(
            panel.width / 200.0f,
            1.5f,
            2.4f
        );

        ui.DrawText(
            "INDUSTRIAL",
            panel.x + 30.0f,
            panel.y + panel.height * 0.09f,
            titleScale,
            White
        );

        ui.DrawText(
            "FRONTIER",
            panel.x + 30.0f,
            panel.y + panel.height * 0.09f +
                titleScale * 10.0f,
            titleScale,
            Orange
        );

        ui.DrawText(
            "BUILD. EXTRACT. AUTOMATE.",
            panel.x + 30.0f,
            panel.y + panel.height * 0.26f,
            1.25f,
            Muted
        );

        const char* labels[] =
        {
            "NEW GAME",
            "CONTINUE",
            "LOAD GAME",
            "SETTINGS",
            "CREDITS",
            "QUIT GAME"
        };

        for (int i = 0; i < 6; ++i)
        {
            const Rect rect = GetButton(width, height, i);

            DrawButton(
                ui,
                rect,
                labels[i],
                rect.Contains(mouseX, mouseY),
                i != 1 || canContinue
            );
        }

        ui.DrawText(
            "CYBERTRACE STUDIOS",
            panel.x + 30.0f,
            panel.y + panel.height - 27.0f,
            1.1f,
            Muted
        );
    }

    void MainMenu::DrawPauseMenu(
        UIRenderer& ui,
        int width,
        int height,
        float mouseX,
        float mouseY) const
    {
        const Rect panel = GetPanel(width, height);

        ui.DrawRectangle(
            panel.x,
            panel.y,
            panel.width,
            panel.height,
            Panel
        );

        ui.DrawRectangle(
            panel.x,
            panel.y,
            5.0f,
            panel.height,
            Orange
        );

        ui.DrawText(
            "GAME PAUSED",
            panel.x + 30.0f,
            panel.y + panel.height * 0.13f,
            2.4f,
            White
        );

        const char* labels[] =
        {
            "RESUME",
            "SAVE GAME",
            "LOAD GAME",
            "SETTINGS",
            "MAIN MENU",
            "QUIT GAME"
        };

        for (int i = 0; i < 6; ++i)
        {
            const Rect rect = GetButton(width, height, i);

            DrawButton(
                ui,
                rect,
                labels[i],
                rect.Contains(mouseX, mouseY)
            );
        }
    }

    void MainMenu::DrawStory(
        UIRenderer& ui,
        const GameFlow& flow,
        int width,
        int height) const
    {
        const StoryScene& scene =
            flow.GetCurrentStoryScene();

        const float screenWidth =
            static_cast<float>(width);

        const float screenHeight =
            static_cast<float>(height);

        const float panelX = screenWidth * 0.08f;
        const float panelWidth = screenWidth * 0.84f;
        const float panelY = screenHeight * 0.57f;
        const float panelHeight = screenHeight * 0.34f;

        ui.DrawRectangle(
            panelX,
            panelY,
            panelWidth,
            panelHeight,
            { 0.02f, 0.03f, 0.05f, 0.89f }
        );

        ui.DrawRectangle(
            panelX,
            panelY,
            panelWidth,
            3.0f,
            Orange
        );

        const float headingScale = std::clamp(
            screenWidth / 550.0f,
            1.4f,
            2.6f
        );

        ui.DrawText(
            scene.heading,
            panelX + 24.0f,
            panelY + 20.0f,
            headingScale,
            Orange
        );

        const float bodyScale = std::clamp(
            screenWidth / 900.0f,
            1.0f,
            1.7f
        );

        const std::size_t maximumCharacters =
            static_cast<std::size_t>(
                std::max(
                    18.0f,
                    (panelWidth - 48.0f) /
                        (bodyScale * 6.0f)
                )
            );

        const auto lines = WrapText(
            scene.narrative,
            maximumCharacters
        );

        float textY =
            panelY + 30.0f + headingScale * 9.0f;

        for (const std::string& line : lines)
        {
            ui.DrawText(
                line,
                panelX + 24.0f,
                textY,
                bodyScale,
                White
            );

            textY += bodyScale * 10.0f;
        }

        const float progress =
            flow.GetStorySceneProgress();

        ui.DrawRectangle(
            panelX + 24.0f,
            panelY + panelHeight - 39.0f,
            panelWidth - 48.0f,
            3.0f,
            Muted
        );

        ui.DrawRectangle(
            panelX + 24.0f,
            panelY + panelHeight - 39.0f,
            (panelWidth - 48.0f) * progress,
            3.0f,
            Orange
        );

        ui.DrawText(
            "SPACE: NEXT    ESC: SKIP",
            panelX + 24.0f,
            panelY + panelHeight - 24.0f,
            1.1f,
            Muted
        );
    }

    void MainMenu::DrawSubmenu(
        UIRenderer& ui,
        GameState state,
        int width,
        int height,
        float mouseX,
        float mouseY) const
    {
        const Rect panel = GetPanel(width, height);

        ui.DrawRectangle(
            panel.x,
            panel.y,
            panel.width,
            panel.height,
            Panel
        );

        ui.DrawRectangle(
            panel.x,
            panel.y,
            5.0f,
            panel.height,
            Orange
        );

        const bool settings =
            state == GameState::Settings;

        ui.DrawText(
            settings ? "SETTINGS" : "CREDITS",
            panel.x + 30.0f,
            panel.y + 50.0f,
            2.5f,
            Orange
        );

        if (settings)
        {
            ui.DrawText(
                "DISPLAY AND AUDIO OPTIONS",
                panel.x + 30.0f,
                panel.y + 120.0f,
                1.2f,
                White
            );

            ui.DrawText(
                "COMING IN A FUTURE UPDATE",
                panel.x + 30.0f,
                panel.y + 155.0f,
                1.1f,
                Muted
            );
        }
        else
        {
            ui.DrawText(
                "INDUSTRIAL FRONTIER",
                panel.x + 30.0f,
                panel.y + 120.0f,
                1.3f,
                White
            );

            ui.DrawText(
                "DEVELOPED BY",
                panel.x + 30.0f,
                panel.y + 160.0f,
                1.2f,
                Muted
            );

            ui.DrawText(
                "CYBERTRACE STUDIOS",
                panel.x + 30.0f,
                panel.y + 190.0f,
                1.3f,
                Orange
            );
        }

        const Rect back = GetButton(width, height, 3);

        DrawButton(
            ui,
            back,
            "BACK",
            back.Contains(mouseX, mouseY)
        );
    }

    void MainMenu::Render(
        UIRenderer& ui,
        const GameFlow& flow,
        int width,
        int height,
        float mouseX,
        float mouseY,
        bool canContinue) const
    {
        switch (flow.GetState())
        {
            case GameState::MainMenu:
                DrawMainMenu(
                    ui, width, height, mouseX, mouseY,
                    canContinue
                );
                break;

            case GameState::Paused:
                DrawPauseMenu(
                    ui, width, height, mouseX, mouseY
                );
                break;

            case GameState::NewGameIntro:
                DrawStory(ui, flow, width, height);
                break;

            case GameState::Settings:
            case GameState::Credits:
                DrawSubmenu(
                    ui,
                    flow.GetState(),
                    width,
                    height,
                    mouseX,
                    mouseY
                );
                break;

            case GameState::Playing:
                break;
        }
    }

    MenuAction MainMenu::HandleClick(
        const GameFlow& flow,
        int width,
        int height,
        float mouseX,
        float mouseY,
        bool canContinue) const
    {
        const GameState state = flow.GetState();

        if (state == GameState::MainMenu)
        {
            for (int i = 0; i < 6; ++i)
            {
                if (!GetButton(width, height, i)
                         .Contains(mouseX, mouseY))
                {
                    continue;
                }

                switch (i)
                {
                    case 0: return MenuAction::NewGame;
                    case 1: return canContinue ? MenuAction::Continue : MenuAction::None;
                    case 2: return MenuAction::LoadGame;
                    case 3: return MenuAction::Settings;
                    case 4: return MenuAction::Credits;
                    case 5: return MenuAction::Quit;
                }
            }
        }

        if (state == GameState::Paused)
        {
            for (int i = 0; i < 6; ++i)
            {
                if (!GetButton(width, height, i)
                         .Contains(mouseX, mouseY))
                {
                    continue;
                }

                switch (i)
                {
                    case 0: return MenuAction::Resume;
                    case 1: return MenuAction::SaveGame;
                    case 2: return MenuAction::LoadGame;
                    case 3: return MenuAction::Settings;
                    case 4: return MenuAction::MainMenu;
                    case 5: return MenuAction::Quit;
                }
            }
        }

        if (state == GameState::Settings ||
            state == GameState::Credits)
        {
            if (GetButton(width, height, 3)
                    .Contains(mouseX, mouseY))
            {
                return MenuAction::Back;
            }
        }

        return MenuAction::None;
    }
}
