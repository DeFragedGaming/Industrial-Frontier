
#include "industrial_frontier/Application.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
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

        std::cout << "Initializing "
                  << m_applicationName << "...\n";

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            std::cerr << "SDL initialization failed: "
                      << SDL_GetError() << '\n';
            return false;
        }

        if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
            !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3) ||
            !SDL_GL_SetAttribute(
                SDL_GL_CONTEXT_PROFILE_MASK,
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
        std::cout << "OpenGL Context Version: "
                  << majorVersion << "." << minorVersion << '\n';

        if (version != nullptr)
        {
            std::cout << "OpenGL Driver Version: "
                      << reinterpret_cast<const char*>(version)
                      << '\n';
        }

        if (renderer != nullptr)
        {
            std::cout << "Graphics Renderer: "
                      << reinterpret_cast<const char*>(renderer)
                      << '\n';
        }

        SDL_GetWindowSizeInPixels(
            m_window,
            &m_viewportWidth,
            &m_viewportHeight
        );

        glViewport(0, 0, m_viewportWidth, m_viewportHeight);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.025f, 0.035f, 0.055f, 1.0f);

        if (!m_renderer.Initialize())
        {
            std::cerr << "3D renderer initialization failed.\n";
            Shutdown();
            return false;
        }

        m_initialized = true;
        m_lastFrameTime = SDL_GetPerformanceCounter();

        std::cout << "Graphics initialization complete.\n";
        std::cout << "Player controller initialized.\n";
        std::cout << "WASD: Move | Mouse: Look | Shift: Sprint\n";
        std::cout << "Space: Jump\n";
        std::cout << "Escape: Release mouse | Left-click: Capture mouse\n";

        return true;
    }

    void Application::SetMouseCapture(bool captured)
    {
        if (m_window == nullptr || m_mouseCaptured == captured)
        {
            return;
        }

        if (!SDL_SetWindowRelativeMouseMode(m_window, captured))
        {
            std::cerr << "Mouse capture change failed: "
                      << SDL_GetError() << '\n';
            return;
        }

        m_mouseCaptured = captured;
        m_mouseDeltaX = 0.0f;
        m_mouseDeltaY = 0.0f;

        if (captured)
        {
            std::cout << "Mouse captured. Press Escape to release.\n";
        }
        else
        {
            std::cout << "Mouse released. Left-click to capture.\n";
        }
    }

    void Application::HandleEvent(const SDL_Event& event)
    {
        switch (event.type)
        {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                m_running = false;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (event.key.repeat)
                {
                    break;
                }

                if (event.key.scancode == SDL_SCANCODE_ESCAPE)
                {
                    if (m_mouseCaptured)
                    {
                        SetMouseCapture(false);
                    }
                    else
                    {
                        m_running = false;
                    }
                }
                else if (event.key.scancode == SDL_SCANCODE_SPACE)
                {
                    m_jumpRequested = true;
                }
                break;

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event.button.button == SDL_BUTTON_LEFT &&
                    !m_mouseCaptured)
                {
                    SetMouseCapture(true);
                }
                break;

            case SDL_EVENT_MOUSE_MOTION:
                if (m_mouseCaptured)
                {
                    m_mouseDeltaX += event.motion.xrel;
                    m_mouseDeltaY += event.motion.yrel;
                }
                break;

            case SDL_EVENT_WINDOW_FOCUS_LOST:
                if (m_mouseCaptured)
                {
                    SetMouseCapture(false);
                }
                break;

            default:
                break;
        }
    }

    void Application::Run()
    {
        if (!m_initialized)
        {
            std::cerr << "Application has not been initialized.\n";
            return;
        }

        m_running = true;

        SDL_Event event;

        const double performanceFrequency =
            static_cast<double>(SDL_GetPerformanceFrequency());

        while (m_running)
        {
            while (SDL_PollEvent(&event))
            {
                HandleEvent(event);
            }

            const Uint64 currentTime = SDL_GetPerformanceCounter();

            float deltaTime = static_cast<float>(
                static_cast<double>(currentTime - m_lastFrameTime) /
                performanceFrequency
            );

            m_lastFrameTime = currentTime;
            deltaTime = std::clamp(deltaTime, 0.0f, 0.1f);

            const bool* keyboardState = SDL_GetKeyboardState(nullptr);

            const float forwardInput =
                (keyboardState[SDL_SCANCODE_W] ? 1.0f : 0.0f) -
                (keyboardState[SDL_SCANCODE_S] ? 1.0f : 0.0f);

            const float rightInput =
                (keyboardState[SDL_SCANCODE_D] ? 1.0f : 0.0f) -
                (keyboardState[SDL_SCANCODE_A] ? 1.0f : 0.0f);

            const bool sprint =
                keyboardState[SDL_SCANCODE_LSHIFT] ||
                keyboardState[SDL_SCANCODE_RSHIFT];

            m_renderer.UpdatePlayer(
                deltaTime,
                forwardInput,
                rightInput,
                sprint,
                m_jumpRequested
            );

            m_jumpRequested = false;

            if (m_mouseCaptured)
            {
                m_renderer.RotatePlayerCamera(
                    m_mouseDeltaX,
                    m_mouseDeltaY
                );
            }

            m_mouseDeltaX = 0.0f;
            m_mouseDeltaY = 0.0f;

            SDL_GetWindowSizeInPixels(
                m_window,
                &m_viewportWidth,
                &m_viewportHeight
            );

            if (m_viewportWidth > 0 && m_viewportHeight > 0)
            {
                glViewport(
                    0,
                    0,
                    m_viewportWidth,
                    m_viewportHeight
                );

                glClear(
                    GL_COLOR_BUFFER_BIT |
                    GL_DEPTH_BUFFER_BIT
                );

                m_renderer.Render(
                    m_viewportWidth,
                    m_viewportHeight
                );

                SDL_GL_SwapWindow(m_window);
            }
        }
    }

    void Application::Shutdown()
    {
        if (m_window != nullptr && m_mouseCaptured)
        {
            SDL_SetWindowRelativeMouseMode(m_window, false);
            m_mouseCaptured = false;
        }

        if (m_glContext != nullptr)
        {
            m_renderer.Shutdown();

            SDL_GL_DestroyContext(m_glContext);
            m_glContext = nullptr;
        }

        if (m_window != nullptr)
        {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }

        if (SDL_WasInit(0) != 0)
        {
            SDL_Quit();
        }

        m_initialized = false;
        m_running = false;

        std::cout << "Industrial Frontier shut down.\n";
    }
}
