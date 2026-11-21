
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <cstdint>
#include "industrial_frontier/save/SaveManager.hpp"
#include "industrial_frontier/resources/OreProcessing.hpp"
#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/resources/StorageTransfer.hpp"

namespace industrial_frontier
{
    struct HUDWorldState
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        bool grounded = false;
        bool terrainUpdating = false;
        std::size_t loadedChunks = 0;
        bool hasTarget = false;
        float targetDistance = 0.0f;
        std::string targetMaterial = "NONE";
        bool targetIsOre = false;
        std::string targetAssay;
        bool miningActive = false;
        float miningProgress = 0.0f;
        int miningHits = 0;
        int miningHitsRequired = 0;
        std::string miningName;
        std::string miningFeedback;
    };

    class Renderer
    {
    public:
        Renderer();
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        bool Initialize();

        void UpdatePlayer(
            float deltaTime,
            float forwardInput,
            float rightInput,
            bool sprint,
            bool jumpPressed
        );

        void RotatePlayerCamera(
            float deltaX,
            float deltaY
        );

        void SetCinematicMode(bool enabled);

        void UpdateCinematic(
            float deltaTime,
            bool introduction,
            float progress
        );

        bool DigTerrain(float radius = 2.0f, float reach = 8.0f);
        void SetEquippedItem(const std::string& itemId);
        void SetMineralDatabase(const MineralDatabase* catalog);
        bool PlaceClayFurnace();
        bool PlaceOreCrusher();
        bool PlacePrimitiveMachine();
        bool HasPrimitiveMachineTarget() const;
        bool CyclePrimitiveMachineOre();
        bool LoadPrimitiveMachine();
        bool CollectPrimitiveMachine();
        bool DismantlePrimitiveMachine();
        void UpdatePrimitiveMachines(float deltaTime, bool crankHeld);
        bool HasOreCrusherTarget() const;
        bool CycleCrusherOre();
        bool LoadOreCrusher();
        bool CollectOreCrusher();
        bool DismantleOreCrusher();
        bool UpdateOreCrusherCrank(float deltaTime, bool crankHeld);
        bool PlaceBuildStructure();
        bool FindChestToOpen(std::uint64_t& id, InventoryState& contents) const;
        bool SaveChestContents(std::uint64_t id, const InventoryState& contents);
        bool DismantleBuildStructureAtCrosshair();
        bool TogglePlacementPreview();
        bool RotatePlacementPreview(int direction);
        bool CancelPlacementPreview();
        bool HasPlacementPreview() const;
        void ReportNoDismantleTarget();
        bool InteractClayFurnace();
        bool CycleFurnaceOre();
        bool DiscardFurnaceResidue();
        bool FuelClayFurnace();
        bool CollectCooledCopper();
        bool DismantleClayFurnaceAtCrosshair();
        void UpdateClayFurnaces(float deltaTime);
        bool BeginOreMining();
        bool BeginTreeHarvest();
        void UpdateTreeHarvest(float deltaTime);
        bool GatherTarget();
        bool PlaceCampfire();
        bool DismantleCampfireAtCrosshair();
        bool InteractCampfire();
        bool CollectCampfireCharcoal();
        void UpdateCampfires(float deltaTime);
        bool CraftPrimitive(const std::string& recipeId, std::string& message);
        void UpdateOreMining(float deltaTime);
        bool GetHUDWorldState(HUDWorldState& state) const;
        bool GetInventoryState(InventoryState& state) const;
        bool DiscardInventorySlot(std::size_t slot);
        bool TransferStorageItem(PlayerInventory& storage,
            std::size_t logicalSlot, bool fromStorage,
            StorageTransferMode mode = StorageTransferMode::All);
        bool ProcessInventorySlot(std::size_t slot, ProcessingOperation operation,
                                  std::string& message);
        bool SetCarryCapacityBonus(float bonusKg);
        bool CaptureWorld(WorldSave& save) const;
        bool RestoreWorld(const WorldSave& save);
        bool StartFreshWorld(int seed);

        void Render(int width, int height);
        void Shutdown();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
