
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

        std::cout
            << "Initializing "
            << m_applicationName
            << "...\n";

        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        {
            std::cerr
                << "SDL initialization failed: "
                << SDL_GetError()
                << '\n';

            return false;
        }

        if (!SDL_GL_SetAttribute(
                SDL_GL_CONTEXT_MAJOR_VERSION, 3) ||
            !SDL_GL_SetAttribute(
                SDL_GL_CONTEXT_MINOR_VERSION, 3) ||
            !SDL_GL_SetAttribute(
                SDL_GL_CONTEXT_PROFILE_MASK,
                SDL_GL_CONTEXT_PROFILE_CORE) ||
            !SDL_GL_SetAttribute(
                SDL_GL_DOUBLEBUFFER, 1) ||
            !SDL_GL_SetAttribute(
                SDL_GL_DEPTH_SIZE, 24))
        {
            std::cerr
                << "OpenGL attribute configuration failed: "
                << SDL_GetError()
                << '\n';

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
            std::cerr
                << "Window creation failed: "
                << SDL_GetError()
                << '\n';

            SDL_Quit();
            return false;
        }

        m_glContext = SDL_GL_CreateContext(m_window);

        if (m_glContext == nullptr)
        {
            std::cerr
                << "OpenGL context creation failed: "
                << SDL_GetError()
                << '\n';

            Shutdown();
            return false;
        }

        if (!SDL_GL_MakeCurrent(m_window, m_glContext))
        {
            std::cerr
                << "Could not activate OpenGL context: "
                << SDL_GetError()
                << '\n';

            Shutdown();
            return false;
        }

        if (!SDL_GL_SetSwapInterval(1))
        {
            std::cerr
                << "VSync configuration warning: "
                << SDL_GetError()
                << '\n';
        }

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
        const GLubyte* graphicsRenderer = glGetString(GL_RENDERER);

        std::cout
            << "SDL3 initialized successfully.\n"
            << "OpenGL Context Version: "
            << majorVersion
            << "."
            << minorVersion
            << '\n';

        if (version != nullptr)
        {
            std::cout
                << "OpenGL Driver Version: "
                << reinterpret_cast<const char*>(version)
                << '\n';
        }

        if (graphicsRenderer != nullptr)
        {
            std::cout
                << "Graphics Renderer: "
                << reinterpret_cast<const char*>(graphicsRenderer)
                << '\n';
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

        if (!m_renderer.Initialize())
        {
            std::cerr
                << "3D renderer initialization failed.\n";

            Shutdown();
            return false;
        }

        if (!m_uiRenderer.Initialize())
        {
            std::cerr
                << "UI renderer initialization failed.\n";

            Shutdown();
            return false;
        }

        m_renderer.SetCinematicMode(true);
        m_renderer.UpdateCinematic(0.0f, false, 0.0f);

        m_initialized = true;
        m_lastFrameTime = SDL_GetPerformanceCounter();

        std::cout
            << "Industrial Frontier initialized.\n"
            << "Main menu ready.\n"
            << "Cinematic terrain camera enabled.\n"
            << "WASD: Move | Mouse: Look\n"
            << "Shift: Sprint | Space: Jump\n"
            << "Escape: Pause / Resume\n";

        return true;
    }

    void Application::SetMouseCapture(bool captured)
    {
        if (m_window == nullptr ||
            m_mouseCaptured == captured)
        {
            return;
        }

        if (!SDL_SetWindowRelativeMouseMode(
                m_window,
                captured))
        {
            std::cerr
                << "Mouse capture change failed: "
                << SDL_GetError()
                << '\n';

            return;
        }

        m_mouseCaptured = captured;
        m_mouseDeltaX = 0.0f;
        m_mouseDeltaY = 0.0f;
    }

    void Application::HandleMenuAction(MenuAction action)
    {
        switch (action)
        {
            case MenuAction::NewGame:
                SetMouseCapture(false);
                m_gameFlow.StartNewGame();
                m_menuCameraTime = 0.0f;
                m_planetfallProgress = 0.0f;
                m_renderer.SetCinematicMode(true);
                break;

            case MenuAction::Resume:
                m_gameFlow.ResumeGame();
                m_renderer.SetCinematicMode(false);
                SetMouseCapture(true);
                break;

            case MenuAction::MainMenu:
                SetMouseCapture(false);
                m_gameFlow.ReturnToMainMenu();
                m_menuCameraTime = 0.0f;
                m_renderer.SetCinematicMode(true);
                break;

            case MenuAction::Settings:
                m_gameFlow.OpenSettings();
                break;

            case MenuAction::Credits:
                m_gameFlow.OpenCredits();
                break;

            case MenuAction::Back:
                m_gameFlow.CloseSubmenu();
                break;

            case MenuAction::Quit:
                m_running = false;
                break;

            case MenuAction::SkipIntro:
                m_gameFlow.SkipIntroduction();
                m_renderer.SetCinematicMode(false);
                SetMouseCapture(true);
                break;

            case MenuAction::NextStory:
                m_gameFlow.AdvanceIntroduction();

                if (m_gameFlow.IsPlaying())
                {
                    m_renderer.SetCinematicMode(false);
                    SetMouseCapture(true);
                }
                break;

            case MenuAction::None:
                break;
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
            {
                if (event.key.repeat)
                {
                    break;
                }

                const SDL_Scancode key = event.key.scancode;
                const GameState state = m_gameFlow.GetState();

                if (key == SDL_SCANCODE_ESCAPE)
                {
                    if (state == GameState::NewGameIntro)
                    {
                        HandleMenuAction(MenuAction::SkipIntro);
                    }
                    else if (state == GameState::Playing)
                    {
                        SetMouseCapture(false);
                        m_gameFlow.PauseGame();
                    }
                    else if (state == GameState::Paused)
                    {
                        HandleMenuAction(MenuAction::Resume);
                    }
                    else if (state == GameState::Settings ||
                             state == GameState::Credits)
                    {
                        HandleMenuAction(MenuAction::Back);
                    }
                }
                else if (key == SDL_SCANCODE_SPACE)
                {
                    if (state == GameState::NewGameIntro)
                    {
                        HandleMenuAction(MenuAction::NextStory);
                    }
                    else if (state == GameState::Playing)
                    {
                        m_jumpRequested = true;
                    }
                }

                break;
            }

            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                if (event.button.button != SDL_BUTTON_LEFT)
                {
                    break;
                }

                if (m_gameFlow.IsMenuVisible())
                {
                    int windowWidth = 1;
                    int windowHeight = 1;

                    SDL_GetWindowSize(
                        m_window,
                        &windowWidth,
                        &windowHeight
                    );

                    const float scaleX =
                        static_cast<float>(m_viewportWidth) /
                        static_cast<float>(
                            std::max(windowWidth, 1)
                        );

                    const float scaleY =
                        static_cast<float>(m_viewportHeight) /
                        static_cast<float>(
                            std::max(windowHeight, 1)
                        );

                    const MenuAction action =
                        m_mainMenu.HandleClick(
                            m_gameFlow,
                            m_viewportWidth,
                            m_viewportHeight,
                            event.button.x * scaleX,
                            event.button.y * scaleY
                        );

                    HandleMenuAction(action);
                }
                else if (m_gameFlow.IsPlaying() &&
                         !m_mouseCaptured)
                {
                    SetMouseCapture(true);
                }

                break;
            }

            case SDL_EVENT_MOUSE_MOTION:
                if (m_mouseCaptured &&
                    m_gameFlow.IsPlaying())
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

    void Application::UpdateCinematic(float deltaTime)
    {
        if (m_gameFlow.IsPlaying())
        {
            m_renderer.SetCinematicMode(false);
            return;
        }

        const bool introduction =
            m_gameFlow.IsIntroductionActive();

        if (introduction)
        {
            m_planetfallProgress = std::clamp(
                m_planetfallProgress + deltaTime / 38.0f,
                0.0f,
                1.0f
            );
        }
        else if (m_gameFlow.GetState() == GameState::MainMenu)
        {
            m_planetfallProgress = 0.0f;
        }

        m_menuCameraTime += deltaTime;

        m_renderer.SetCinematicMode(true);
        m_renderer.UpdateCinematic(
            deltaTime,
            introduction,
            m_planetfallProgress
        );
    }

    void Application::Run()
    {
        if (!m_initialized)
        {
            std::cerr
                << "Application has not been initialized.\n";

            return;
        }

        m_running = true;

        SDL_Event event;

        const double performanceFrequency =
            static_cast<double>(
                SDL_GetPerformanceFrequency()
            );

        while (m_running)
        {
            while (SDL_PollEvent(&event))
            {
                HandleEvent(event);
            }

            if (!m_running)
            {
                break;
            }

            const Uint64 currentTime =
                SDL_GetPerformanceCounter();

            float deltaTime = static_cast<float>(
                static_cast<double>(
                    currentTime - m_lastFrameTime
                ) / performanceFrequency
            );

            m_lastFrameTime = currentTime;

            deltaTime = std::clamp(
                deltaTime,
                0.0f,
                0.1f
            );

            const GameState previousState =
                m_gameFlow.GetState();

            m_gameFlow.Update(deltaTime);

            if (previousState == GameState::NewGameIntro &&
                m_gameFlow.IsPlaying())
            {
                m_renderer.SetCinematicMode(false);
                SetMouseCapture(true);
            }

            if (m_gameFlow.IsPlaying())
            {
                const bool* keyboardState =
                    SDL_GetKeyboardState(nullptr);

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

                if (m_mouseCaptured)
                {
                    m_renderer.RotatePlayerCamera(
                        m_mouseDeltaX,
                        m_mouseDeltaY
                    );
                }
            }
            else
            {
                UpdateCinematic(deltaTime);
            }

            m_jumpRequested = false;
            m_mouseDeltaX = 0.0f;
            m_mouseDeltaY = 0.0f;

            SDL_GetWindowSizeInPixels(
                m_window,
                &m_viewportWidth,
                &m_viewportHeight
            );

            if (m_viewportWidth <= 0 ||
                m_viewportHeight <= 0)
            {
                continue;
            }

            int windowWidth = 1;
            int windowHeight = 1;

            SDL_GetWindowSize(
                m_window,
                &windowWidth,
                &windowHeight
            );

            const float scaleX =
                static_cast<float>(m_viewportWidth) /
                static_cast<float>(
                    std::max(windowWidth, 1)
                );

            const float scaleY =
                static_cast<float>(m_viewportHeight) /
                static_cast<float>(
                    std::max(windowHeight, 1)
                );

            SDL_GetMouseState(
                &m_mouseX,
                &m_mouseY
            );

            m_mouseX *= scaleX;
            m_mouseY *= scaleY;

            m_renderer.Render(
                m_viewportWidth,
                m_viewportHeight
            );

            if (!m_gameFlow.IsPlaying())
            {
                m_uiRenderer.BeginFrame(
                    m_viewportWidth,
                    m_viewportHeight
                );

                m_mainMenu.Render(
                    m_uiRenderer,
                    m_gameFlow,
                    m_viewportWidth,
                    m_viewportHeight,
                    m_mouseX,
                    m_mouseY
                );

                m_uiRenderer.EndFrame();
            }

            SDL_GL_SwapWindow(m_window);
        }
    }

    void Application::Shutdown()
    {
        if (m_window != nullptr && m_mouseCaptured)
        {
            SDL_SetWindowRelativeMouseMode(
                m_window,
                false
            );

            m_mouseCaptured = false;
        }

        if (m_glContext != nullptr)
        {
            m_uiRenderer.Shutdown();
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

        std::cout
            << "Industrial Frontier shut down.\n";
    }
}
