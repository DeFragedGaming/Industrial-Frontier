
#include "industrial_frontier/Application.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <iostream>
#include <utility>

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

        if (!m_saveManager.Initialize())
        {
            std::cerr << "Could not initialize save directory.\n";
            Shutdown();
            return false;
        }

        if (!m_saveOverlay.Initialize())
        {
            std::cerr << "Save menu initialization failed.\n";
            Shutdown();
            return false;
        }

        RefreshSaveSlots();
        std::cout << "Save directory: " << m_saveManager.GetDirectory() << '\n';

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

    void Application::RefreshSaveSlots()
    {
        m_saveSlots = m_saveManager.List();
    }

    void Application::DisplaySaveError(const std::string& message)
    {
        m_saveOverlay.SetMessage(message);
        std::cerr << "Save system: " << message << '\n';
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Industrial Frontier - Save System", message.c_str(), m_window);
    }

    bool Application::SaveAutomatic()
    {
        if (!m_worldActive || !m_worldPlayed)
            return false;
        WorldSave world;
        if (!m_renderer.CaptureWorld(world))
            return false;
        std::string error;
        if (!m_saveManager.SaveAutosave(world, error))
        {
            std::cerr << "Autosave failed: " << error << '\n';
            return false;
        }
        m_autosaveElapsed = 0.0f;
        RefreshSaveSlots();
        return true;
    }

    bool Application::LoadWorld(const std::string& id, int backupIndex)
    {
        WorldSave world;
        bool recovered = false;
        std::string error;
        if (!m_saveManager.Load(id, backupIndex, world, recovered, error))
        {
            DisplaySaveError(error);
            return false;
        }
        if (!m_renderer.RestoreWorld(world))
        {
            DisplaySaveError("Could not restore this world or its terrain seed");
            return false;
        }
        m_saveManager.MarkLastUsed(id);
        m_gameFlow.StartNewGame();
        m_gameFlow.SkipIntroduction();
        m_renderer.SetCinematicMode(false);
        m_worldActive = true;
        m_worldPlayed = true;
        m_autosaveElapsed = 0.0f;
        m_gameHUD.Reset();
        SDL_StopTextInput(m_window);
        m_saveOverlay.Close();
        m_pausedForPanel = false;
        SetMouseCapture(true);
        std::cout << (recovered ? "Restored backup: " : "Loaded save: ")
                  << world.name << '\n';
        return true;
    }

    bool Application::ContinueWorld()
    {
        WorldSave world;
        std::string id;
        std::string error;
        bool recovered = false;
        if (!m_saveManager.Continue(world, id, recovered, error))
        {
            DisplaySaveError(error);
            return false;
        }
        return LoadWorld(id, 0);
    }

    void Application::CancelSavePanel()
    {
        SDL_StopTextInput(m_window);
        m_saveOverlay.Close();
        if (m_pausedForPanel && m_gameFlow.GetState() == GameState::Paused)
        {
            m_gameFlow.ResumeGame();
            m_renderer.SetCinematicMode(false);
            SetMouseCapture(true);
        }
        m_pausedForPanel = false;
    }

    void Application::HandleSaveAction(const SaveUiResult& result)
    {
        switch (result.action)
        {
            case SaveUiAction::None:
                break;
            case SaveUiAction::OpenSave:
            case SaveUiAction::OpenLoad:
            {
                if (result.action == SaveUiAction::OpenSave &&
                    (!m_worldActive || !m_worldPlayed))
                    break;
                if (m_gameFlow.IsPlaying())
                {
                    m_gameFlow.PauseGame();
                    SetMouseCapture(false);
                    m_pausedForPanel = true;
                }
                else
                {
                    m_pausedForPanel = false;
                }
                RefreshSaveSlots();
                if (result.action == SaveUiAction::OpenSave)
                {
                    m_saveOverlay.OpenSave();
                    SDL_StartTextInput(m_window);
                }
                else
                {
                    m_saveOverlay.OpenLoad();
                }
                break;
            }
            case SaveUiAction::Continue:
                ContinueWorld();
                break;
            case SaveUiAction::LoadSlot:
                LoadWorld(result.slotId, result.backupIndex);
                break;
            case SaveUiAction::DeleteSlot:
            {
                const auto found = std::find_if(m_saveSlots.begin(), m_saveSlots.end(),
                    [&result](const SaveSlotInfo& slot)
                    {
                        return slot.id == result.slotId;
                    });
                if (found == m_saveSlots.end())
                {
                    DisplaySaveError("Selected save no longer exists");
                    RefreshSaveSlots();
                    break;
                }
                const std::string name = found->name;
                std::string error;
                if (!m_saveManager.DeleteSlot(result.slotId, error))
                {
                    RefreshSaveSlots();
                    DisplaySaveError(error);
                    break;
                }
                RefreshSaveSlots();
                m_saveOverlay.SetMessage("DELETED: " + name);
                std::cout << "Deleted save slot: " << name << '\n';
                break;
            }
            case SaveUiAction::SaveNamed:
            {
                if (!m_worldActive || !m_worldPlayed)
                    break;
                WorldSave world;
                if (!m_renderer.CaptureWorld(world))
                {
                    DisplaySaveError("Could not capture the current world");
                    break;
                }
                std::string id;
                std::string error;
                if (!m_saveManager.SaveNew(result.name, world, id, error))
                {
                    DisplaySaveError(error);
                    break;
                }
                RefreshSaveSlots();
                CancelSavePanel();
                m_saveOverlay.SetMessage("SAVED: " + result.name);
                std::cout << "Created named save: " << result.name << '\n';
                break;
            }
            case SaveUiAction::Close:
                CancelSavePanel();
                break;
        }
    }

    void Application::HandleMenuAction(MenuAction action)
    {
        switch (action)
        {
            case MenuAction::NewGame:
                m_gameHUD.Reset();
                if (m_worldActive && m_worldPlayed)
                    SaveAutomatic();
                if (!m_renderer.StartFreshWorld())
                {
                    DisplaySaveError("Could not initialize a new world");
                    break;
                }
                m_worldActive = true;
                m_worldPlayed = false;
                m_autosaveElapsed = 0.0f;
                SetMouseCapture(false);
                m_gameFlow.StartNewGame();
                m_menuCameraTime = 0.0f;
                m_planetfallProgress = 0.0f;
                m_renderer.SetCinematicMode(true);
                break;

            case MenuAction::Continue:
                ContinueWorld();
                break;

            case MenuAction::LoadGame:
                HandleSaveAction({SaveUiAction::OpenLoad});
                break;

            case MenuAction::SaveGame:
                HandleSaveAction({SaveUiAction::OpenSave});
                break;

            case MenuAction::Resume:
                m_gameFlow.ResumeGame();
                m_renderer.SetCinematicMode(false);
                SetMouseCapture(true);
                break;

            case MenuAction::MainMenu:
                SaveAutomatic();
                m_worldActive = false;
                m_worldPlayed = false;
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
        if (event.type == SDL_EVENT_TEXT_INPUT && m_saveOverlay.IsNaming())
        {
            m_saveOverlay.TextInput(event.text.text);
            return;
        }
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

                if (m_saveOverlay.IsOpen())
                {
                    HandleSaveAction(m_saveOverlay.KeyDown(key));
                    break;
                }

                if (key == SDL_SCANCODE_F3 && state == GameState::Playing)
                {
                    m_gameHUD.ToggleDetails();
                    break;
                }

                if (key == SDL_SCANCODE_F5 &&
                    (state == GameState::Playing || state == GameState::Paused))
                {
                    HandleSaveAction({SaveUiAction::OpenSave});
                    break;
                }
                if (key == SDL_SCANCODE_F9 &&
                    (state == GameState::Playing || state == GameState::Paused ||
                     state == GameState::MainMenu))
                {
                    HandleSaveAction({SaveUiAction::OpenLoad});
                    break;
                }
                if (key == SDL_SCANCODE_F10 && state == GameState::MainMenu)
                {
                    HandleSaveAction({SaveUiAction::Continue});
                    break;
                }

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

                if (m_saveOverlay.IsOpen() ||
                    m_gameFlow.GetState() == GameState::MainMenu ||
                    m_gameFlow.GetState() == GameState::Paused)
                {
                    int ww = 1;
                    int wh = 1;
                    SDL_GetWindowSize(m_window, &ww, &wh);
                    const float scaleX = static_cast<float>(m_viewportWidth) /
                        static_cast<float>(std::max(ww, 1));
                    const float scaleY = static_cast<float>(m_viewportHeight) /
                        static_cast<float>(std::max(wh, 1));
                    const bool modalWasOpen = m_saveOverlay.IsOpen();
                    const auto ui = m_saveOverlay.Click(m_viewportWidth,
                        m_viewportHeight, event.button.x * scaleX,
                        event.button.y * scaleY,
                        true, m_worldActive && m_worldPlayed,
                        !m_saveSlots.empty(), m_saveSlots);
                    if (ui.action != SaveUiAction::None)
                    {
                        HandleSaveAction(ui);
                        break;
                    }
                    if (modalWasOpen)
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
                            event.button.y * scaleY,
                            !m_saveSlots.empty()
                        );

                    HandleMenuAction(action);
                }
                else if (m_gameFlow.IsPlaying() &&
                         !m_mouseCaptured)
                {
                    SetMouseCapture(true);
                }
                else if (m_gameFlow.IsPlaying() && m_mouseCaptured)
                {
                    m_gameHUD.ReportDig(m_renderer.DigTerrain());
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
                m_gameHUD.Update(deltaTime);
                m_worldPlayed = true;
                m_autosaveElapsed += deltaTime;
                if (m_autosaveElapsed >= 1800.0f)
                    SaveAutomatic();
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

            m_uiRenderer.BeginFrame(
                m_viewportWidth,
                m_viewportHeight
            );

            if (m_gameFlow.IsPlaying())
            {
                HUDWorldState hudWorld{};
                if (m_renderer.GetHUDWorldState(hudWorld))
                {
                    m_gameHUD.Render(
                        m_uiRenderer,
                        hudWorld,
                        m_viewportWidth,
                        m_viewportHeight
                    );
                }
            }
            else
            {
                m_mainMenu.Render(
                    m_uiRenderer,
                    m_gameFlow,
                    m_viewportWidth,
                    m_viewportHeight,
                    m_mouseX,
                    m_mouseY,
                    !m_saveSlots.empty()
                );
            }

            m_uiRenderer.EndFrame();

            m_saveOverlay.Render(m_viewportWidth, m_viewportHeight,
                m_gameFlow.GetState() == GameState::MainMenu ||
                m_gameFlow.GetState() == GameState::Paused,
                m_worldActive && m_worldPlayed, !m_saveSlots.empty(), m_saveSlots);

            SDL_GL_SwapWindow(m_window);
        }
    }

    void Application::Shutdown()
    {
        if (m_initialized && m_worldActive && m_worldPlayed)
            SaveAutomatic();
        if (m_window != nullptr)
            SDL_StopTextInput(m_window);
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
            m_saveOverlay.Shutdown();
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
