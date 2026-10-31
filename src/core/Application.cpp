
#include "industrial_frontier/Application.hpp"

#include <iostream>

namespace industrial_frontier
{
    Application::Application() = default;

    Application::~Application()
    {
        Shutdown();
    }

    bool Application::Initialize()
    {
        if (m_initialized)
        {
            return true;
        }

        std::cout << "Initializing " << m_applicationName << "...\n";

        m_initialized = true;

        std::cout << "Application initialized successfully.\n";

        return true;
    }

    void Application::Run()
    {
        if (!m_initialized)
        {
            std::cerr << "Application has not been initialized.\n";
            return;
        }

        m_running = true;

        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "          INDUSTRIAL FRONTIER           \n";
        std::cout << "========================================\n";
        std::cout << "Native C++20 Application\n";
        std::cout << "Industrial Simulation Foundation\n";
        std::cout << "========================================\n";
        std::cout << "\n";

        std::cout << "Application is running.\n";

        m_running = false;
    }

    void Application::Shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        std::cout << "Shutting down " << m_applicationName << "...\n";

        m_initialized = false;
        m_running = false;
    }
}
