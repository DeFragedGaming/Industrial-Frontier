
#include "industrial_frontier/Application.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

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

        if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
            !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) ||
            !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                                 SDL_GL_CONTEXT_PROFILE_CORE) ||
            !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) ||
            !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24))
        {
            std::cerr << "OpenGL attribute configuration failed: "
                      << SDL_GetError() << '\n';

            SDL_Quit();
            return false;
        }

        m_window = SDL_CreateWindow(
            m_applicationName.c_str(),
            1280,
            720,
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
        );

        if (m_window == nullptr)
        {
            std::cerr << "Window creation failed: "
                      << SDL_GetError() << '\n';

            SDL_Quit();
            return false;
        }

        m_glContext = SDL_GL_CreateContext(m_window);

        if (m_glContext == nullptr)
        {
            std::cerr << "OpenGL context creation failed: "
                      << SDL_GetError() << '\n';

            SDL_DestroyWindow(m_window);
            m_window = nullptr;

            SDL_Quit();
            return false;
        }

        if (!SDL_GL_MakeCurrent(m_window, m_glContext))
        {
            std::cerr << "Could not activate OpenGL context: "
                      << SDL_GetError() << '\n';

            Shutdown();
            return false;
        }

        SDL_GL_SetSwapInterval(1);

        int majorVersion = 0;
        int minorVersion = 0;

        SDL_GL_GetAttribute(
            SDL_GL_CONTEXT_MAJOR_VERSION,
            &majorVersion
        );

        SDL_GL_GetAttribute(
            SDL_GL_CONTEXT_MINOR_VERSION,
            &minorVersion
        );

        const GLubyte* version = glGetString(GL_VERSION);
        const GLubyte* renderer = glGetString(GL_RENDERER);

        std::cout << "SDL3 initialized successfully.\n";
        std::cout << "OpenGL context created successfully.\n";
        std::cout << "Requested OpenGL: 3.3 Core Profile\n";
        std::cout << "OpenGL Context Version: "
                  << majorVersion << "." << minorVersion << '\n';

        if (version != nullptr)
        {
            std::cout << "OpenGL Driver Version: "
                      << reinterpret_cast<const char*>(version) << '\n';
        }

        if (renderer != nullptr)
        {
            std::cout << "Graphics Renderer: "
                      << reinterpret_cast<const char*>(renderer) << '\n';
        }

        SDL_GetWindowSizeInPixels(
            m_window,
            &m_viewportWidth,
            &m_viewportHeight
        );

        glViewport(
            0,
            0,
            m_viewportWidth,
            m_viewportHeight
        );

        glEnable(GL_DEPTH_TEST);

        glClearColor(
            0.025f,
            0.035f,
            0.055f,
            1.0f
        );

        m_initialized = true;

        std::cout << "Graphics initialization complete.\n";
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

            int width = 0;
            int height = 0;

            SDL_GetWindowSizeInPixels(
                m_window,
                &width,
                &height
            );

            if (width != m_viewportWidth ||
                height != m_viewportHeight)
            {
                m_viewportWidth = width;
                m_viewportHeight = height;

                glViewport(
                    0,
                    0,
                    m_viewportWidth,
                    m_viewportHeight
                );
            }

            glClear(
                GL_COLOR_BUFFER_BIT |
                GL_DEPTH_BUFFER_BIT
            );

            SDL_GL_SwapWindow(m_window);
        }
    }

    void Application::Shutdown()
    {
        if (m_glContext != nullptr)
        {
            SDL_GL_DestroyContext(m_glContext);
            m_glContext = nullptr;
        }

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
