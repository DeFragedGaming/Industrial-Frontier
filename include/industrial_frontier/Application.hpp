
#pragma once

#include "industrial_frontier/core/GameFlow.hpp"
#include "industrial_frontier/renderer/Renderer.hpp"
#include "industrial_frontier/ui/MainMenu.hpp"
#include "industrial_frontier/ui/UIRenderer.hpp"
#include "industrial_frontier/ui/GameHUD.hpp"
#include "industrial_frontier/ui/InventoryOverlay.hpp"
#include "industrial_frontier/ui/FieldGuideOverlay.hpp"
#include "industrial_frontier/ui/ProcessingOverlay.hpp"
#include "industrial_frontier/ui/SeedOverlay.hpp"
#include "industrial_frontier/ui/PrimitiveCraftingOverlay.hpp"
#include "industrial_frontier/ui/ItemWorkspaceOverlay.hpp"
#include "industrial_frontier/resources/FieldGuideCatalog.hpp"
#include "industrial_frontier/ui/SaveOverlay.hpp"
#include "industrial_frontier/save/SaveManager.hpp"
#include "industrial_frontier/resources/MineralDatabase.hpp"

#include <SDL3/SDL.h>

#include <string>
#include <vector>

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
        void HandleEvent(const SDL_Event& event);
        void HandleMenuAction(MenuAction action);
        void SetMouseCapture(bool captured);
        void UpdateCinematic(float deltaTime);
        void HandleSaveAction(const SaveUiResult& result);
        void RefreshSaveSlots();
        void CancelSavePanel();
        bool SaveAutomatic();
        bool LoadWorld(const std::string& id, int backupIndex);
        bool ContinueWorld();
        void DisplaySaveError(const std::string& message);
        void ToggleProcessing();
        void RunProcessing(ProcessingOperation operation);
        void HandleSeedAction(const SeedUiResult& result);
        bool BeginNewWorld(int seed);
        void CaptureStorageLayout(WorldSave& world);
        PlayerInventory& ActiveStorage();
        const PlayerInventory& ActiveStorage() const;
        void CommitActiveChest();
        ItemWidgetHit HitItemUI(float x, float y) const;
        void DropItem(const ItemWidgetHit& source, const ItemWidgetHit& target);
        std::string DragLabel(const ItemWidgetHit& hit,
            const InventoryState& inventory, const InventoryState& chest) const;

        bool m_initialized = false;
        bool m_running = false;
        bool m_mouseCaptured = false;
        bool m_jumpRequested = false;
        bool m_worldActive = false;
        bool m_worldPlayed = false;
        bool m_pausedForPanel = false;
        bool m_inventoryOpen = false;
        bool m_storageOpen = false;
        ItemWidgetHit m_dragStart{};
        float m_dragX = 0.0f;
        float m_dragY = 0.0f;
        bool m_guideOpen = false;
        bool m_processingOpen = false;
        bool m_craftingOpen = false;
        std::size_t m_craftingSelected = 0;
        std::string m_craftingMessage;
        std::size_t m_processingSelected = 0;
        std::string m_processingMessage;
        std::size_t m_inventorySelected = 0;
        float m_autosaveElapsed = 0.0f;

        SDL_Window* m_window = nullptr;
        SDL_GLContext m_glContext = nullptr;

        int m_viewportWidth = 0;
        int m_viewportHeight = 0;

        float m_mouseDeltaX = 0.0f;
        float m_mouseDeltaY = 0.0f;
        float m_mouseX = 0.0f;
        float m_mouseY = 0.0f;

        float m_menuCameraTime = 0.0f;
        float m_planetfallProgress = 0.0f;

        Uint64 m_lastFrameTime = 0;

        std::string m_applicationName = "Industrial Frontier";

        Renderer m_renderer;
        UIRenderer m_uiRenderer;
        GameHUD m_gameHUD;
        InventoryOverlay m_inventoryOverlay;
        ItemWorkspaceOverlay m_itemWorkspace;
        ItemLayout m_itemLayout;
        PlayerInventory m_storage;
        PlayerInventory m_chestStorage;
        std::uint64_t m_activeChestId = 0;
        ItemLayout::Grid m_starterStorageSlots{};
        FieldGuideOverlay m_fieldGuide;
        ProcessingOverlay m_processingOverlay;
        SeedOverlay m_seedOverlay;
        PrimitiveCraftingOverlay m_craftingOverlay;
        MainMenu m_mainMenu;
        GameFlow m_gameFlow;
        SaveManager m_saveManager;
        MineralDatabase m_mineralDatabase;
        FieldGuideCatalog m_fieldGuideCatalog;
        SaveOverlay m_saveOverlay;
        std::vector<SaveSlotInfo> m_saveSlots;
    };
}
