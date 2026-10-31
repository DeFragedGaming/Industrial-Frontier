
#pragma once

#include <string>

struct SDL_Window;

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

        std::string m_applicationName = "Industrial Frontier";
    };
}
