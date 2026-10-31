
#pragma once

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
        bool m_initialized = false;
        bool m_running = false;

        SDL_Window* m_window = nullptr;
        SDL_GLContext m_glContext = nullptr;

        int m_viewportWidth = 0;
        int m_viewportHeight = 0;

        std::string m_applicationName = "Industrial Frontier";
    };
}
