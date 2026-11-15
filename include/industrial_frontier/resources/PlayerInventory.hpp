#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace industrial_frontier
{
    struct InventoryMineralComponent
    {
        std::string mineralId;
        float massFraction = 0.0f;
        bool gangue = false;
    };

    enum class OreProcessingStage : int
    {
        Raw = 0,
        Crushed = 1,
        Ground = 2,
        Roasted = 3,
        Concentrate = 4,
        Tailings = 5
    };

    struct InventoryOreBatch
    {
        std::uint64_t depositId = 0;
        std::string primaryMineralId;
        std::string displayName;
        float massKg = 0.0f;
        float oreMineralFraction = 0.0f;
        OreProcessingStage processingStage = OreProcessingStage::Raw;
        std::vector<InventoryMineralComponent> components;
    };

    struct InventoryItemStack
    {
        std::string itemId;
        std::uint32_t quantity = 0;
        std::uint16_t durability = 0;
    };

    struct InventoryState
    {
        float carryCapacityBonusKg = 0.0f;
        std::vector<InventoryOreBatch> oreBatches;
        std::vector<InventoryItemStack> items;
    };

    class PlayerInventory
    {
    public:
        static constexpr std::size_t MaxSlots = 30;
        static constexpr float BaseCarryCapacityKg = 500.0f;
        static constexpr float MaximumCarryCapacityBonusKg = 900.0f;

        void Clear();
        bool CanAccept(const InventoryOreBatch& batch) const;
        bool AddOre(const InventoryOreBatch& batch);
        bool RemoveOre(std::size_t slot, float massKg);
        bool AddItem(const std::string& itemId, std::uint32_t quantity);
        bool AddItemStack(const InventoryItemStack& stack);
        bool UseTool(const std::string& itemId, std::uint16_t cost, bool& broke);
        bool HasUsableTool(const std::string& itemId) const;
        bool RemoveItem(const std::string& itemId, std::uint32_t quantity);
        bool RemoveItemSlot(std::size_t slot);
        bool CanAcceptItem(const std::string& itemId, std::uint32_t quantity) const;
        std::uint32_t ItemCount(const std::string& itemId) const;
        bool Craft(const std::string& recipeId, std::string& message);
        bool SetCarryCapacityBonus(float bonusKg);
        bool Restore(const InventoryState& state);
        InventoryState Capture() const;
        float TotalWeightKg() const;
        float CarryCapacityKg() const;
        std::size_t UsedSlots() const;
        const std::vector<InventoryOreBatch>& OreBatches() const;
        const std::vector<InventoryItemStack>& Items() const;
        static bool IsValidState(const InventoryState& state);

    private:
        static bool Compatible(const InventoryOreBatch& a, const InventoryOreBatch& b);
        InventoryState m_state;
    };
}
