
#pragma once

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

        std::string m_applicationName = "Industrial Frontier";
    };
}
