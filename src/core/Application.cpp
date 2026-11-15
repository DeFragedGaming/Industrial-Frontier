
#include "industrial_frontier/Application.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
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

        std::string mineralError;
        const char* executableBasePath = SDL_GetBasePath();
        const std::filesystem::path sourceAssetPath =
            std::filesystem::path("assets") / "data" / "minerals.tsv";

        std::filesystem::path mineralPath = sourceAssetPath;
        if (executableBasePath != nullptr && *executableBasePath != '\0')
        {
            mineralPath = std::filesystem::path(executableBasePath) /
                sourceAssetPath;
        }

        if (!m_mineralDatabase.LoadFromFile(mineralPath, mineralError))
        {
            if (mineralPath == sourceAssetPath ||
                !m_mineralDatabase.LoadFromFile(sourceAssetPath, mineralError))
            {
                std::cerr << "Mineral database initialization failed: "
                          << mineralError << '\n';
                Shutdown();
                return false;
            }
        }

        m_renderer.SetMineralDatabase(&m_mineralDatabase);

        std::cout << "Stage 10A geological catalog loaded: "
                  << m_mineralDatabase.Count() << " resources.\n";

        const std::filesystem::path routeAssetPath =
            std::filesystem::path("assets") / "data" / "guide_processing.tsv";
        std::filesystem::path routePath = routeAssetPath;
        if (executableBasePath != nullptr && *executableBasePath != '\0')
        {
            routePath = std::filesystem::path(executableBasePath) / routeAssetPath;
        }

        std::string guideError;
        if (!m_fieldGuideCatalog.LoadFromFile(routePath,
                m_mineralDatabase, guideError))
        {
            if (routePath == routeAssetPath ||
                !m_fieldGuideCatalog.LoadFromFile(routeAssetPath,
                    m_mineralDatabase, guideError))
            {
                std::cerr << "Field Guide initialization failed: "
                          << guideError << '\n';
                Shutdown();
                return false;
            }
        }
        std::cout << "Stage 10D Field Guide loaded: "
                  << m_fieldGuideCatalog.Count() << " processing routes.\n";

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

        m_saveOverlay.SetTextRenderer(&m_uiRenderer);

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
            << "Escape: Pause / Resume\n"
            << "I: Inventory | G: Field Guide | P: Processing bench\n"
            << "C: Craft | F: Preview selected placement kit\n"
            << "E: Gather / Load selected full ore batch / Cast metal\n"
            << "V: Select furnace ore batch | R: Fuel | T: Collect | U: Purge residue\n"
            << "B: Open aimed chest or starter stash | R/Q: Rotate preview\n"
            << "F: Confirm | X: Dismantle or cancel preview\n";

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
        CommitActiveChest();
        if (!m_renderer.CaptureWorld(world))
            return false;
        CaptureStorageLayout(world);
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
        if (!m_storage.Restore(world.storage))
        {
            DisplaySaveError("Could not restore the world storage");
            return false;
        }
        m_itemLayout.bag = world.bagSlots;
        m_itemLayout.storage = world.storageSlots;
        m_starterStorageSlots = world.storageSlots;
        m_itemLayout.bar = world.quickbar;
        m_itemLayout.selected = world.selectedQuickbar;
        m_saveManager.MarkLastUsed(id);
        m_gameFlow.StartNewGame();
        m_gameFlow.SkipIntroduction();
        m_renderer.SetCinematicMode(false);
        m_worldActive = true;
        m_worldPlayed = true;
        m_autosaveElapsed = 0.0f;
        m_gameHUD.Reset();
        m_inventoryOpen = false;
        m_storageOpen = false;
        m_activeChestId = 0;
        m_chestStorage.Clear();
        m_dragStart = {};
        m_inventorySelected = 0;
        m_guideOpen = false;
        m_processingOpen = false;
        m_craftingOpen = false;
        m_fieldGuide.Reset();
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
                    m_craftingOpen = false;
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
                CommitActiveChest();
        if (!m_renderer.CaptureWorld(world))
                {
                    DisplaySaveError("Could not capture the current world");
                    break;
                }
                CaptureStorageLayout(world);
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

    bool Application::BeginNewWorld(int seed)
    {
        if (m_worldActive && m_worldPlayed)
        {
            SaveAutomatic();
        }
        if (!m_renderer.StartFreshWorld(seed))
        {
            DisplaySaveError("Could not initialize a new world");
            return false;
        }
        m_storage.Clear();
        m_storage.SetCarryCapacityBonus(900.0f);
        m_chestStorage.Clear();
        m_activeChestId = 0;
        m_starterStorageSlots = {};
        m_itemLayout = {};
        m_gameHUD.Reset();
        m_inventoryOpen = false;
        m_storageOpen = false;
        m_dragStart = {};
        m_inventorySelected = 0;
        m_guideOpen = false;
        m_processingOpen = false;
        m_craftingOpen = false;
        m_fieldGuide.Reset();
        m_worldActive = true;
        m_worldPlayed = false;
        m_autosaveElapsed = 0.0f;
        SetMouseCapture(false);
        m_gameFlow.StartNewGame();
        m_menuCameraTime = 0.0f;
        m_planetfallProgress = 0.0f;
        m_renderer.SetCinematicMode(true);
        std::cout << "New world seed: " << seed << '\n';
        return true;
    }

    void Application::HandleSeedAction(const SeedUiResult& result)
    {
        if (result.action == SeedUiAction::Cancel)
        {
            m_seedOverlay.Close();
            SDL_StopTextInput(m_window);
        }
        else if (result.action == SeedUiAction::Create)
        {
            if (BeginNewWorld(result.seed))
            {
                m_seedOverlay.Close();
                SDL_StopTextInput(m_window);
            }
        }
    }

    void Application::HandleMenuAction(MenuAction action)
    {
        switch (action)
        {
            case MenuAction::NewGame:
                if (!m_seedOverlay.IsOpen())
                {
                    m_seedOverlay.Open();
                    SetMouseCapture(false);
                    SDL_StartTextInput(m_window);
                }
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
                m_inventoryOpen = false;
                m_storageOpen = false;
                m_dragStart = {};
                m_guideOpen = false;
                m_processingOpen = false;
                m_craftingOpen = false;
                m_fieldGuide.Reset();
                SDL_StopTextInput(m_window);
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

    void Application::ToggleProcessing()
    {
        if (m_gameFlow.GetState() != GameState::Playing)
            return;
        m_processingOpen = !m_processingOpen;
        if (m_processingOpen)
        {
            m_inventoryOpen = false;
            m_storageOpen = false;
            m_dragStart = {};
            if (m_guideOpen)
            {
                m_guideOpen = false;
                m_fieldGuide.Reset();
                SDL_StopTextInput(m_window);
            }
            SetMouseCapture(false);
            m_processingMessage = "Select a batch and a processing operation";
        }
        else
        {
            SetMouseCapture(true);
        }
    }

    void Application::RunProcessing(ProcessingOperation operation)
    {
        if (!m_processingOpen) return;
        InventoryState inventory;
        if (!m_renderer.GetInventoryState(inventory) ||
            m_processingSelected >= inventory.oreBatches.size())
        {
            m_processingMessage = "Choose an occupied ore slot first";
            return;
        }
        std::string message;
        if (m_renderer.ProcessInventorySlot(m_processingSelected, operation, message))
        {
            InventoryState updated;
            if (m_renderer.GetInventoryState(updated) && !updated.oreBatches.empty())
                m_processingSelected = std::min(m_processingSelected,
                    updated.oreBatches.size() - 1);
            else
                m_processingSelected = 0;
        }
        m_processingMessage = std::move(message);
    }

    PlayerInventory& Application::ActiveStorage()
    {
        return m_activeChestId == 0 ? m_storage : m_chestStorage;
    }

    const PlayerInventory& Application::ActiveStorage() const
    {
        return m_activeChestId == 0 ? m_storage : m_chestStorage;
    }

    void Application::CommitActiveChest()
    {
        if (m_activeChestId != 0)
            m_renderer.SaveChestContents(m_activeChestId,m_chestStorage.Capture());
    }

    void Application::CaptureStorageLayout(WorldSave& world)
    {
        CommitActiveChest();
        InventoryState inventory;
        if (m_renderer.GetInventoryState(inventory))
        {
            if (m_activeChestId == 0)
                m_itemLayout.Reconcile(inventory, m_storage.Capture());
            else
            {
                ItemLayout layout = m_itemLayout;
                layout.storage = m_starterStorageSlots;
                layout.Reconcile(inventory, m_storage.Capture());
                m_starterStorageSlots = layout.storage;
            }
        }
        world.storage = m_storage.Capture();
        world.bagSlots = m_itemLayout.bag;
        world.storageSlots = m_activeChestId == 0 ?
            m_itemLayout.storage : m_starterStorageSlots;
        world.quickbar = m_itemLayout.bar;
        world.selectedQuickbar = m_itemLayout.selected;
    }

    ItemWidgetHit Application::HitItemUI(float x, float y) const
    {
        ItemWidgetHit hit = m_itemWorkspace.Hit(m_viewportWidth,
            m_viewportHeight, x, y, m_storageOpen);
        if (hit.zone != 0)
            return hit;
        if (m_inventoryOpen)
        {
            const int cell = m_inventoryOverlay.HitSlot(
                m_viewportWidth, m_viewportHeight, x, y);
            if (cell >= 0)
                return {1, cell};
        }
        return {};
    }

    std::string Application::DragLabel(const ItemWidgetHit& hit,
        const InventoryState& inventory, const InventoryState& chest) const
    {
        if (hit.zone == 3 && hit.slot >= 0 &&
            hit.slot < static_cast<int>(m_itemLayout.bar.size()))
        {
            const std::string& id = m_itemLayout.bar[hit.slot];
            const auto* def = FindPrimitiveItem(id);
            return def ? std::string(def->name) : id;
        }
        if (hit.slot < 0 || hit.slot >= 30 ||
            (hit.zone != 1 && hit.zone != 2))
            return {};
        const InventoryState& state = hit.zone == 1 ? inventory : chest;
        const ItemLayout::Grid& grid = hit.zone == 1 ?
            m_itemLayout.bag : m_itemLayout.storage;
        const int logical = ItemLayout::Resolve(state, grid[hit.slot]);
        if (logical < 0)
            return {};
        if (logical < static_cast<int>(state.oreBatches.size()))
            return state.oreBatches[logical].displayName;
        const auto& stack = state.items[logical -
            static_cast<int>(state.oreBatches.size())];
        const auto* def = FindPrimitiveItem(stack.itemId);
        return def ? std::string(def->name) : stack.itemId;
    }

    void Application::DropItem(const ItemWidgetHit& source,
        const ItemWidgetHit& target)
    {
        if (source.zone == 0 || target.zone == 0 ||
            source.slot < 0 || target.slot < 0)
            return;
        InventoryState inventory;
        if (!m_renderer.GetInventoryState(inventory))
            return;
        InventoryState chest = ActiveStorage().Capture();
        m_itemLayout.Reconcile(inventory, chest);
        if (source.zone == target.zone)
        {
            if (source.zone == 1)
                ItemLayout::Swap(m_itemLayout.bag, source.slot, target.slot);
            else if (source.zone == 2)
                ItemLayout::Swap(m_itemLayout.storage, source.slot, target.slot);
            else if (source.zone == 3 &&
                source.slot < static_cast<int>(ItemLayout::BarSize) &&
                target.slot < static_cast<int>(ItemLayout::BarSize))
                std::swap(m_itemLayout.bar[source.slot],
                    m_itemLayout.bar[target.slot]);
            return;
        }
        const std::string key = source.zone == 1 ?
            m_itemLayout.bag[source.slot] : source.zone == 2 ?
            m_itemLayout.storage[source.slot] :
            source.slot < static_cast<int>(ItemLayout::BarSize) &&
                !m_itemLayout.bar[source.slot].empty() ?
            ItemLayout::ItemKey(m_itemLayout.bar[source.slot]) : std::string{};
        if (key.empty())
            return;
        if (target.zone == 3)
        {
            if (source.zone == 2 || key.rfind("item:", 0) != 0 ||
                target.slot >= static_cast<int>(ItemLayout::BarSize))
                return;
            const std::string displaced = m_itemLayout.bar[target.slot];
            if (!m_itemLayout.Bind(target.slot, key, inventory))
                return;
            if (source.zone == 1)
            {
                m_itemLayout.bag[source.slot].clear();
                if (!displaced.empty() &&
                    displaced != key.substr(5))
                    m_itemLayout.bag[source.slot] =
                        ItemLayout::ItemKey(displaced);
            }
            m_itemLayout.Reconcile(inventory, chest);
            return;
        }
        if (source.zone == 3 && target.zone == 1)
        {
            m_itemLayout.bar[source.slot].clear();
            ItemLayout::Place(m_itemLayout.bag, key, target.slot);
            m_itemLayout.Reconcile(inventory, chest);
            return;
        }
        if (source.zone == 1 && target.zone == 2)
        {
            const int logical = ItemLayout::Resolve(inventory, key);
            if (logical < 0 || !m_renderer.TransferStorageItem(ActiveStorage(),
                static_cast<std::size_t>(logical), false,
                m_itemWorkspace.SelectedTransferMode()))
                return;
            InventoryState updated;
            if (m_renderer.GetInventoryState(updated))
                m_itemLayout.Reconcile(updated, ActiveStorage().Capture());
            ItemLayout::Place(m_itemLayout.storage, key, target.slot);
            CommitActiveChest();
        }
        else if (source.zone == 2 && target.zone == 1)
        {
            const int logical = ItemLayout::Resolve(chest, key);
            if (logical < 0 || !m_renderer.TransferStorageItem(ActiveStorage(),
                static_cast<std::size_t>(logical), true,
                m_itemWorkspace.SelectedTransferMode()))
                return;
            InventoryState updated;
            if (m_renderer.GetInventoryState(updated))
                m_itemLayout.Reconcile(updated, ActiveStorage().Capture());
            ItemLayout::Place(m_itemLayout.bag, key, target.slot);
            CommitActiveChest();
        }
        else if (source.zone == 3 && target.zone == 2)
        {
            const int logical = ItemLayout::Resolve(inventory, key);
            if (logical < 0 || !m_renderer.TransferStorageItem(ActiveStorage(),
                static_cast<std::size_t>(logical), false,
                m_itemWorkspace.SelectedTransferMode()))
                return;
            InventoryState updated;
            if (m_renderer.GetInventoryState(updated))
            {
                if (ItemLayout::Resolve(updated, key) < 0)
                    m_itemLayout.bar[source.slot].clear();
                m_itemLayout.Reconcile(updated, ActiveStorage().Capture());
            }
            ItemLayout::Place(m_itemLayout.storage, key, target.slot);
            CommitActiveChest();
        }
    }

    void Application::HandleEvent(const SDL_Event& event)
    {
        if (event.type == SDL_EVENT_TEXT_INPUT && m_seedOverlay.IsOpen())
        {
            m_seedOverlay.TextInput(event.text.text);
            return;
        }
        if (event.type == SDL_EVENT_TEXT_INPUT && m_saveOverlay.IsNaming())
        {
            m_saveOverlay.TextInput(event.text.text);
            return;
        }
        if (event.type == SDL_EVENT_TEXT_INPUT && m_guideOpen)
        {
            m_fieldGuide.TextInput(event.text.text);
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

                if (m_seedOverlay.IsOpen())
                {
                    HandleSeedAction(m_seedOverlay.KeyDown(key));
                    break;
                }

                if (m_saveOverlay.IsOpen())
                {
                    HandleSaveAction(m_saveOverlay.KeyDown(key));
                    break;
                }

                if (state == GameState::Playing && key == SDL_SCANCODE_C &&
                    !m_guideOpen && !m_processingOpen && !m_inventoryOpen &&
                    !m_storageOpen)
                {
                    m_craftingOpen = !m_craftingOpen;
                    m_craftingMessage.clear();
                    SetMouseCapture(!m_craftingOpen);
                    break;
                }
                if (m_craftingOpen && state == GameState::Playing)
                {
                    if (key == SDL_SCANCODE_ESCAPE)
                    {
                        m_craftingOpen = false;
                        SetMouseCapture(true);
                    }
                    else if (key == SDL_SCANCODE_LEFT)
                        m_craftingSelected = PrimitiveCraftingOverlay::SelectCategory(
                            m_craftingSelected, -1);
                    else if (key == SDL_SCANCODE_RIGHT)
                        m_craftingSelected = PrimitiveCraftingOverlay::SelectCategory(
                            m_craftingSelected, 1);
                    else if (key == SDL_SCANCODE_UP)
                        m_craftingSelected = PrimitiveCraftingOverlay::MoveSelection(
                            m_craftingSelected, -1);
                    else if (key == SDL_SCANCODE_DOWN)
                        m_craftingSelected = PrimitiveCraftingOverlay::MoveSelection(
                            m_craftingSelected, 1);
                    else if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_6)
                        m_craftingSelected = PrimitiveCraftingOverlay::SelectVisibleSlot(
                            m_craftingSelected, static_cast<std::size_t>(
                                key - SDL_SCANCODE_1));
                    else if (key == SDL_SCANCODE_PAGEDOWN)
                        m_craftingSelected = PrimitiveCraftingOverlay::MovePage(
                            m_craftingSelected, 1);
                    else if (key == SDL_SCANCODE_PAGEUP)
                        m_craftingSelected = PrimitiveCraftingOverlay::MovePage(
                            m_craftingSelected, -1);
                    else if (key == SDL_SCANCODE_RETURN ||
                        key == SDL_SCANCODE_KP_ENTER)
                        m_renderer.CraftPrimitive(
                            std::string(PrimitiveRecipes[m_craftingSelected].id),
                            m_craftingMessage);
                    break;
                }
                if (state == GameState::Playing && m_mouseCaptured &&
                    !m_inventoryOpen && !m_storageOpen && !m_guideOpen &&
                    !m_processingOpen && !m_craftingOpen)
                {
                    if (key == SDL_SCANCODE_F)
                    {
                        m_renderer.TogglePlacementPreview();
                        break;
                    }
                    if (m_renderer.HasPlacementPreview())
                    {
                        if (key == SDL_SCANCODE_R || key == SDL_SCANCODE_Q)
                        {
                            m_renderer.RotatePlacementPreview(
                                key == SDL_SCANCODE_R ? 1 : -1);
                            break;
                        }
                        if (key == SDL_SCANCODE_X || key == SDL_SCANCODE_ESCAPE)
                        {
                            m_renderer.CancelPlacementPreview();
                            break;
                        }
                    }
                    if (key == SDL_SCANCODE_R)
                    {
                        if (!m_renderer.FuelClayFurnace())
                            m_renderer.CollectCampfireCharcoal();
                        break;
                    }
                    if (key == SDL_SCANCODE_X)
                    {
                        if (!m_renderer.DismantleBuildStructureAtCrosshair() &&
                            !m_renderer.DismantleClayFurnaceAtCrosshair() &&
                            !m_renderer.DismantleCampfireAtCrosshair())
                            m_renderer.ReportNoDismantleTarget();
                        break;
                    }
                    if (key == SDL_SCANCODE_U)
                    {
                        m_renderer.DiscardFurnaceResidue();
                        break;
                    }
                    if (key == SDL_SCANCODE_V)
                    {
                        m_renderer.CycleFurnaceOre();
                        break;
                    }
                    if (key == SDL_SCANCODE_T)
                    {
                        m_renderer.CollectCooledCopper();
                        break;
                    }
                    if (key == SDL_SCANCODE_E)
                    {
                        if (!m_renderer.InteractClayFurnace() &&
                            !m_renderer.InteractCampfire())
                            m_renderer.GatherTarget();
                        break;
                    }
                }

                if (state == GameState::Playing && key == SDL_SCANCODE_P &&
                    !m_guideOpen)
                {
                    ToggleProcessing();
                    break;
                }
                if (m_processingOpen && state == GameState::Playing)
                {
                    if (key == SDL_SCANCODE_ESCAPE)
                        ToggleProcessing();
                    else if (key == SDL_SCANCODE_UP && m_processingSelected > 0)
                        --m_processingSelected;
                    else if (key == SDL_SCANCODE_DOWN)
                    {
                        InventoryState inventory;
                        if (m_renderer.GetInventoryState(inventory) &&
                            m_processingSelected + 1 < inventory.oreBatches.size())
                            ++m_processingSelected;
                    }
                    else if (key >= SDL_SCANCODE_1 && key <= SDL_SCANCODE_5)
                        RunProcessing(static_cast<ProcessingOperation>(key - SDL_SCANCODE_1));
                    break;
                }

                if (state == GameState::Playing && key == SDL_SCANCODE_G &&
                    (!m_guideOpen || !m_fieldGuide.SearchFocused()))
                {
                    m_guideOpen = !m_guideOpen;
                    if (m_guideOpen)
                    {
                        m_inventoryOpen = false;
                        m_storageOpen = false;
                        m_fieldGuide.Reset();
                        SetMouseCapture(false);
                        SDL_StartTextInput(m_window);
                    }
                    else
                    {
                        SDL_StopTextInput(m_window);
                        SetMouseCapture(true);
                    }
                    break;
                }

                if (m_guideOpen && state == GameState::Playing)
                {
                    if (key == SDL_SCANCODE_I &&
                        !m_fieldGuide.SearchFocused())
                    {
                        m_guideOpen = false;
                        m_fieldGuide.Reset();
                        SDL_StopTextInput(m_window);
                        m_inventoryOpen = true;
                        SetMouseCapture(false);
                    }
                    else if (key == SDL_SCANCODE_ESCAPE &&
                             !m_fieldGuide.SearchFocused())
                    {
                        m_guideOpen = false;
                        m_fieldGuide.Reset();
                        SDL_StopTextInput(m_window);
                        SetMouseCapture(true);
                    }
                    else
                    {
                        m_fieldGuide.KeyDown(key, m_mineralDatabase,
                            m_fieldGuideCatalog);
                    }
                    break;
                }

                if ((key == SDL_SCANCODE_I || key == SDL_SCANCODE_B) &&
                    state == GameState::Playing && !m_processingOpen &&
                    !m_craftingOpen && !m_guideOpen)
                {
                    if (m_storageOpen)
                    {
                        CommitActiveChest();
                        if (m_activeChestId != 0)
                            m_itemLayout.storage = m_starterStorageSlots;
                    }
                    const bool openingInventory = key == SDL_SCANCODE_I &&
                        !m_inventoryOpen;
                    const bool openingStorage = key == SDL_SCANCODE_B &&
                        !m_storageOpen;
                    m_inventoryOpen = openingInventory;
                    m_storageOpen = openingStorage;
                    m_activeChestId = 0;
                    if (openingStorage)
                    {
                        std::uint64_t chestId = 0;
                        InventoryState contents;
                        if (m_renderer.FindChestToOpen(chestId,contents) &&
                            m_chestStorage.Restore(contents))
                        {
                            m_activeChestId = chestId;
                            m_starterStorageSlots = m_itemLayout.storage;
                            m_itemLayout.storage = {};
                        }
                    }
                    m_dragStart = {};
                    SetMouseCapture(!m_inventoryOpen && !m_storageOpen);
                    break;
                }
                if ((m_inventoryOpen || m_storageOpen) &&
                    state == GameState::Playing)
                {
                    if (key == SDL_SCANCODE_ESCAPE)
                    {
                        CommitActiveChest();
                        if (m_activeChestId != 0)
                            m_itemLayout.storage = m_starterStorageSlots;
                        m_activeChestId = 0;
                        m_inventoryOpen = false;
                        m_storageOpen = false;
                        m_dragStart = {};
                        SetMouseCapture(true);
                    }
                    else if (key == SDL_SCANCODE_DELETE && m_inventoryOpen)
                    {
                        InventoryState current;
                        if (m_renderer.GetInventoryState(current) &&
                            m_inventorySelected < m_itemLayout.bag.size())
                        {
                            const int logical = ItemLayout::Resolve(current,
                                m_itemLayout.bag[m_inventorySelected]);
                            if (logical >= 0)
                                m_renderer.DiscardInventorySlot(logical);
                        }
                    }
                    break;
                }
                if (state == GameState::Playing && key >= SDL_SCANCODE_1 &&
                    key <= SDL_SCANCODE_8)
                {
                    m_itemLayout.selected = static_cast<std::size_t>(
                        key - SDL_SCANCODE_1);
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
                if (m_seedOverlay.IsOpen())
                {
                    if (event.button.button == SDL_BUTTON_LEFT)
                    {
                        int windowWidth = 1;
                        int windowHeight = 1;
                        SDL_GetWindowSize(m_window, &windowWidth, &windowHeight);
                        const float x = event.button.x *
                            static_cast<float>(m_viewportWidth) /
                            static_cast<float>(std::max(windowWidth, 1));
                        const float y = event.button.y *
                            static_cast<float>(m_viewportHeight) /
                            static_cast<float>(std::max(windowHeight, 1));
                        HandleSeedAction(m_seedOverlay.Click(
                            m_viewportWidth, m_viewportHeight, x, y));
                    }
                    break;
                }
                if (m_craftingOpen)
                {
                    if (event.button.button == SDL_BUTTON_LEFT)
                    {
                        int windowWidth = 1;
                        int windowHeight = 1;
                        SDL_GetWindowSize(m_window, &windowWidth, &windowHeight);
                        const float mouseX = event.button.x *
                            static_cast<float>(m_viewportWidth) /
                            static_cast<float>(std::max(windowWidth, 1));
                        const float mouseY = event.button.y *
                            static_cast<float>(m_viewportHeight) /
                            static_cast<float>(std::max(windowHeight, 1));
                        const CraftingUiHit hit = m_craftingOverlay.Click(
                            mouseX, mouseY, m_viewportWidth,
                            m_viewportHeight, m_craftingSelected);
                        if (hit.type == CraftingUiHit::Type::Close)
                        {
                            m_craftingOpen = false;
                            SetMouseCapture(true);
                        }
                        else if (hit.type == CraftingUiHit::Type::Select)
                        {
                            m_craftingSelected = hit.selected;
                            m_craftingMessage.clear();
                        }
                        else if (hit.type == CraftingUiHit::Type::Craft)
                        {
                            m_renderer.CraftPrimitive(
                                std::string(PrimitiveRecipes[m_craftingSelected].id),
                                m_craftingMessage);
                        }
                    }
                    break;
                }
                if (m_processingOpen)
                {
                    if (event.button.button == SDL_BUTTON_LEFT)
                    {
                        int windowWidth = 1;
                        int windowHeight = 1;
                        SDL_GetWindowSize(m_window, &windowWidth, &windowHeight);
                        const float x = event.button.x *
                            static_cast<float>(m_viewportWidth) /
                            static_cast<float>(std::max(windowWidth, 1));
                        const float y = event.button.y *
                            static_cast<float>(m_viewportHeight) /
                            static_cast<float>(std::max(windowHeight, 1));
                        InventoryState inventory;
                        if (m_renderer.GetInventoryState(inventory))
                        {
                            const auto hit = m_processingOverlay.Click(
                                m_viewportWidth, m_viewportHeight, x, y,
                                inventory, m_processingSelected);
                            if (hit.close) ToggleProcessing();
                            if (hit.changedSelection)
                                m_processingSelected = hit.selectedSlot;
                            if (hit.run) RunProcessing(hit.operation);
                        }
                    }
                    break;
                }
                if (m_guideOpen)
                {
                    if (event.button.button == SDL_BUTTON_LEFT)
                    {
                        int windowWidth = 1;
                        int windowHeight = 1;
                        SDL_GetWindowSize(m_window, &windowWidth, &windowHeight);
                        const float x = event.button.x *
                            static_cast<float>(m_viewportWidth) /
                            static_cast<float>(std::max(windowWidth, 1));
                        const float y = event.button.y *
                            static_cast<float>(m_viewportHeight) /
                            static_cast<float>(std::max(windowHeight, 1));
                        if (m_fieldGuide.Click(x, y, m_viewportWidth,
                            m_viewportHeight, m_mineralDatabase, m_fieldGuideCatalog))
                        {
                            m_guideOpen = false;
                            m_fieldGuide.Reset();
                            SDL_StopTextInput(m_window);
                            SetMouseCapture(true);
                        }
                    }
                    break;
                }
                if (m_inventoryOpen || m_storageOpen)
                {
                    if (event.button.button == SDL_BUTTON_LEFT)
                    {
                        int windowWidth = 1;
                        int windowHeight = 1;
                        SDL_GetWindowSize(m_window, &windowWidth, &windowHeight);
                        const float x = event.button.x *
                            static_cast<float>(m_viewportWidth) /
                            static_cast<float>(std::max(windowWidth, 1));
                        const float y = event.button.y *
                            static_cast<float>(m_viewportHeight) /
                            static_cast<float>(std::max(windowHeight, 1));
                        const ItemWidgetHit hit = HitItemUI(x, y);
                        if (hit.zone == 4 && hit.slot >= 0 && hit.slot < 5)
                        {
                            m_itemWorkspace.SetTransferMode(
                                static_cast<StorageTransferMode>(hit.slot));
                            m_dragStart = {};
                            break;
                        }
                        if (hit.zone == 1)
                            m_inventorySelected = static_cast<std::size_t>(hit.slot);
                        if (hit.zone == 3)
                            m_itemLayout.selected = static_cast<std::size_t>(hit.slot);
                        m_dragStart = hit;
                        m_dragX = x;
                        m_dragY = y;
                    }
                    break;
                }
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
                    m_renderer.SetEquippedItem(
                        m_itemLayout.bar[m_itemLayout.selected]);
                    if (!m_renderer.BeginTreeHarvest() &&
                        !m_renderer.BeginOreMining())
                    {
                        m_gameHUD.ReportDig(m_renderer.DigTerrain());
                    }
                }

                break;
            }

            case SDL_EVENT_MOUSE_BUTTON_UP:
            {
                if (event.button.button != SDL_BUTTON_LEFT ||
                    (!m_inventoryOpen && !m_storageOpen))
                    break;
                if (m_dragStart.zone != 0)
                {
                    int windowWidth = 1;
                    int windowHeight = 1;
                    SDL_GetWindowSize(m_window, &windowWidth, &windowHeight);
                    const float x = event.button.x *
                        static_cast<float>(m_viewportWidth) /
                        static_cast<float>(std::max(windowWidth, 1));
                    const float y = event.button.y *
                        static_cast<float>(m_viewportHeight) /
                        static_cast<float>(std::max(windowHeight, 1));
                    if (std::abs(x - m_dragX) > 5.0f ||
                        std::abs(y - m_dragY) > 5.0f)
                        DropItem(m_dragStart, HitItemUI(x, y));
                    m_dragStart = {};
                }
                break;
            }

            case SDL_EVENT_MOUSE_WHEEL:
                if (m_guideOpen)
                {
                    m_fieldGuide.Wheel(event.wheel.y, m_mineralDatabase,
                        m_fieldGuideCatalog);
                }
                else if (m_gameFlow.IsPlaying() && !m_inventoryOpen &&
                    !m_storageOpen && !m_processingOpen && !m_craftingOpen)
                {
                    const int change = event.wheel.y > 0 ? -1 : 1;
                    m_itemLayout.selected = static_cast<std::size_t>(
                        (static_cast<int>(m_itemLayout.selected) + change + 8) % 8);
                }
                break;

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
                InventoryState equippedInventory;
                if (m_renderer.GetInventoryState(equippedInventory))
                    m_itemLayout.Reconcile(equippedInventory, ActiveStorage().Capture());
                m_renderer.SetEquippedItem(m_itemLayout.bar[m_itemLayout.selected]);
                m_renderer.UpdateCampfires(deltaTime);
                m_renderer.UpdateClayFurnaces(deltaTime);
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

                if (!m_inventoryOpen && !m_storageOpen && !m_guideOpen &&
                    !m_processingOpen && !m_craftingOpen)
                {
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

                    m_renderer.UpdateOreMining(deltaTime);
                    m_renderer.UpdateTreeHarvest(deltaTime);
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
                InventoryState bag;
                if (m_renderer.GetInventoryState(bag))
                {
                    const InventoryState chest = ActiveStorage().Capture();
                    m_itemLayout.Reconcile(bag, chest);
                    m_itemWorkspace.RenderBar(m_uiRenderer, bag,
                        m_itemLayout, m_viewportWidth, m_viewportHeight);
                    if (m_inventoryOpen)
                        m_inventoryOverlay.Render(m_uiRenderer, bag,
                            m_viewportWidth, m_viewportHeight,
                            m_inventorySelected, m_itemLayout.bag);
                    if (m_storageOpen)
                        m_itemWorkspace.RenderStorage(m_uiRenderer, bag, chest,
                            m_itemLayout, m_viewportWidth, m_viewportHeight);
                    const bool dragging = m_dragStart.zone != 0 &&
                        (std::abs(m_mouseX - m_dragX) > 5.0f ||
                         std::abs(m_mouseY - m_dragY) > 5.0f);
                    if ((m_inventoryOpen || m_storageOpen) && !dragging)
                        m_itemWorkspace.RenderHoverTooltip(m_uiRenderer,
                            bag, chest, m_itemLayout, m_viewportWidth,
                            m_viewportHeight, m_mouseX, m_mouseY,
                            m_inventoryOpen, m_storageOpen);
                    if ((m_inventoryOpen || m_storageOpen) && dragging)
                        m_itemWorkspace.RenderDrag(m_uiRenderer,
                            DragLabel(m_dragStart, bag, chest),
                            m_mouseX, m_mouseY);
                }
                if (m_processingOpen)
                {
                    InventoryState inventory;
                    if (m_renderer.GetInventoryState(inventory))
                    {
                        m_processingOverlay.Render(m_uiRenderer, inventory,
                            m_viewportWidth, m_viewportHeight,
                            m_processingSelected, m_processingMessage);
                    }
                }
                if (m_craftingOpen)
                {
                    InventoryState inventory;
                    if (m_renderer.GetInventoryState(inventory))
                        m_craftingOverlay.Render(m_uiRenderer, inventory,
                            m_viewportWidth, m_viewportHeight,
                            m_craftingSelected, m_craftingMessage);
                }
                if (m_guideOpen)
                {
                    m_fieldGuide.Render(m_uiRenderer, m_mineralDatabase,
                        m_fieldGuideCatalog, m_viewportWidth, m_viewportHeight);
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
                if (m_seedOverlay.IsOpen())
                {
                    m_seedOverlay.Render(m_uiRenderer,
                        m_viewportWidth, m_viewportHeight);
                }
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
