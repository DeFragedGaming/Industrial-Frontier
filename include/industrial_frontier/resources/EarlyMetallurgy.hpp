#pragma once

#include "industrial_frontier/resources/MineralDatabase.hpp"
#include "industrial_frontier/resources/PlayerInventory.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace industrial_frontier
{
    struct CopperFeedResult
    {
        bool supported = false;
        float copperFraction = 0.0f;
        float recoveryFraction = 0.0f;
        std::string description;
        std::string message;
    };

    CopperFeedResult EvaluateCopperFeed(const InventoryOreBatch& batch,
        const MineralDatabase& minerals);

    struct PrimitiveMetalFeed
    {
        bool supported = false;
        std::string metalId;
        float recoveredMetalFraction = 0.0f;
        std::string message;
    };

    PrimitiveMetalFeed EvaluatePrimitiveMetalFeed(const InventoryOreBatch& batch,
        const MineralDatabase& minerals);
    const char* PrimitiveMetalName(const std::string& metalId);
    const char* PrimitiveIngotId(const std::string& metalId);

    struct ClayFurnaceState
    {
        std::uint64_t id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float fuelSeconds = 0.0f;
        float smeltSeconds = 0.0f;
        float chargeCopperKg = 0.0f;
        float readyCopperKg = 0.0f;
        std::string chargeName;
        std::string metalId = "copper";
        std::uint32_t moldCapacity = 0;
        std::uint32_t castCount = 0;
        float castingCoolingSeconds = 0.0f;
        float yawDegrees = 0.0f;
    };

    class ClayFurnaceWorld
    {
    public:
        static constexpr std::size_t MaxFurnaces = 64;
        static constexpr float SecondsPerCharcoal = 45.0f;
        static constexpr float SmeltDurationSeconds = 30.0f;
        static constexpr float IngotMassKg = 0.25f;
        static constexpr float MaximumReadyCopperKg = 500.0f;
        static constexpr float MaximumSmeltSeconds = 600.0f;

        bool Place(float x, float y, float z, float yawDegrees = 0.0f);
        bool AddCharge(std::uint64_t id, float recoveredMetalKg,
            const std::string& name, const std::string& metalId = "copper",
            float durationSeconds = SmeltDurationSeconds);
        bool AddCharcoal(std::uint64_t id);
        bool DiscardResidue(std::uint64_t id);
        bool Pour(std::uint64_t id);
        bool InstallMold(std::uint64_t id, std::uint32_t capacity);
        std::uint32_t StartCasting(std::uint64_t id);
        std::uint32_t CollectCasting(std::uint64_t id);
        std::uint32_t UninstallMold(std::uint64_t id);
        bool Remove(std::uint64_t id);
        void Update(float seconds);
        void Clear();
        const ClayFurnaceState* Find(std::uint64_t id) const;
        const std::vector<ClayFurnaceState>& Furnaces() const;
        bool Restore(const std::vector<ClayFurnaceState>& states);
        static bool Validate(const std::vector<ClayFurnaceState>& states);

    private:
        ClayFurnaceState* FindMutable(std::uint64_t id);
        std::vector<ClayFurnaceState> m_furnaces;
        std::uint64_t m_nextId = 1;
    };
}
