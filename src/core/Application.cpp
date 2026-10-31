#include "industrial_frontier/Application.hpp"

#include <SDL3/SDL.h>

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

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            std::cerr << "SDL initialization failed: "
                      << SDL_GetError() << '\n';

            return false;
        }

        m_window = SDL_CreateWindow(
            m_applicationName.c_str(),
            1280,
            720,
            SDL_WINDOW_RESIZABLE
        );

        if (m_window == nullptr)
        {
            std::cerr << "Window creation failed: "
                      << SDL_GetError() << '\n';

            SDL_Quit();
            return false;
        }

        m_initialized = true;

        std::cout << "SDL3 initialized successfully.\n";
        std::cout << "Window created: 1280x720\n";

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

        std::cout << "Industrial Frontier is running.\n";

        SDL_Event event;

        while (m_running)
        {
            while (SDL_PollEvent(&event))
            {
                switch (event.type)
                {
                    case SDL_EVENT_QUIT:
                    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                        m_running = false;
                        break;

                    default:
                        break;
                }
            }

            SDL_Delay(1);
        }
    }

    void Application::Shutdown()
    {
        if (m_window != nullptr)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        if (m_initialized)
        {
            SDL_Quit();
            m_initialized = false;
        }

        m_running = false;

        std::cout << "Industrial Frontier shut down.\n";
    }
}