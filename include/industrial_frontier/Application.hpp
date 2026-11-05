
#pragma once

#include "industrial_frontier/core/GameFlow.hpp"
#include "industrial_frontier/renderer/Renderer.hpp"
#include "industrial_frontier/ui/MainMenu.hpp"
#include "industrial_frontier/ui/UIRenderer.hpp"

#include <SDL3/SDL.h>

#include <string>

namespace industrial_frontier
{
    class Application
    {
    public:
        Application();
        ~Application();

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        bool Initialize();
        void Run();
        void Shutdown();

    private:
        void HandleEvent(const SDL_Event& event);
        void HandleMenuAction(MenuAction action);
        void SetMouseCapture(bool captured);
        void UpdateCinematic(float deltaTime);

        bool m_initialized = false;
        bool m_running = false;
        bool m_mouseCaptured = false;
        bool m_jumpRequested = false;

        SDL_Window* m_window = nullptr;
        SDL_GLContext m_glContext = nullptr;

        int m_viewportWidth = 0;
        int m_viewportHeight = 0;

        float m_mouseDeltaX = 0.0f;
        float m_mouseDeltaY = 0.0f;
        float m_mouseX = 0.0f;
        float m_mouseY = 0.0f;

        float m_menuCameraTime = 0.0f;
        float m_planetfallProgress = 0.0f;

        Uint64 m_lastFrameTime = 0;

        std::string m_applicationName = "Industrial Frontier";

        Renderer m_renderer;
        UIRenderer m_uiRenderer;
        MainMenu m_mainMenu;
        GameFlow m_gameFlow;
    };
}
