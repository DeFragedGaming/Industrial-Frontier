#pragma once

#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/resources/PrimitiveTier.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace industrial_frontier
{
    struct OreCrusherState
    {
        std::uint64_t id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yawDegrees = 0.0f;
        float workDone = 0.0f;
        float workRequired = 0.0f;
        float rotationRadians = 0.0f;
        InventoryOreBatch batch;
    };

    class OreCrusherWorld
    {
    public:
        static constexpr std::size_t MaxCrushers = 80;
        static constexpr float CapacityKg = 30.0f;

        static int HardnessTier(const InventoryOreBatch& batch)
        {
            if (IsStonePickaxeMineral(batch.primaryMineralId))
                return 1;
            if (IsCopperPickaxeMineral(batch.primaryMineralId))
                return 2;
            return 3;
        }

        static float RequiredWork(const InventoryOreBatch& batch)
        {
            const int tier = HardnessTier(batch);
            return std::ceil(3.0f + batch.massKg * (0.55f + 0.27f * tier));
        }

        static bool Validate(const std::vector<OreCrusherState>& states)
        {
            if (states.size() > MaxCrushers)
                return false;
            std::unordered_set<std::uint64_t> ids;
            for (const auto& s : states)
            {
                if (!s.id || !ids.insert(s.id).second ||
                    !std::isfinite(s.x) || !std::isfinite(s.y) ||
                    !std::isfinite(s.z) || !std::isfinite(s.yawDegrees) ||
                    !std::isfinite(s.rotationRadians) ||
                    !std::isfinite(s.workDone) || !std::isfinite(s.workRequired) ||
                    std::abs(s.x) > 100000.0f || std::abs(s.y) > 100000.0f ||
                    std::abs(s.z) > 100000.0f || s.yawDegrees < 0.0f ||
                    s.yawDegrees >= 360.001f || s.rotationRadians < 0.0f ||
                    s.rotationRadians > 1000000.0f || s.workDone < 0.0f ||
                    s.workRequired < 0.0f || s.workRequired > 1000.0f ||
                    s.workDone > s.workRequired + 0.001f)
                    return false;
                if (s.batch.massKg > 0.0f)
                {
                    InventoryState inventory;
                    inventory.oreBatches.push_back(s.batch);
                    if (!PlayerInventory::IsValidState(inventory) ||
                        s.batch.massKg > CapacityKg + 0.001f ||
                        (s.batch.processingStage != OreProcessingStage::Raw &&
                         s.batch.processingStage != OreProcessingStage::Crushed) ||
                        s.workRequired < 1.0f ||
                        (s.batch.processingStage == OreProcessingStage::Crushed &&
                         s.workDone + 0.001f < s.workRequired))
                        return false;
                }
                else if (s.batch.massKg != 0.0f ||
                         s.workDone != 0.0f || s.workRequired != 0.0f)
                    return false;
            }
            return true;
        }

        const std::vector<OreCrusherState>& Crushers() const { return m_crushers; }
        void Clear() { m_crushers.clear(); }

        bool Restore(const std::vector<OreCrusherState>& states)
        {
            if (!Validate(states))
                return false;
            m_crushers = states;
            return true;
        }

        OreCrusherState* Find(std::uint64_t id)
        {
            for (auto& crusher : m_crushers)
                if (crusher.id == id)
                    return &crusher;
            return nullptr;
        }

        const OreCrusherState* Find(std::uint64_t id) const
        {
            for (const auto& crusher : m_crushers)
                if (crusher.id == id)
                    return &crusher;
            return nullptr;
        }

        std::uint64_t Place(float x, float y, float z, float yawDegrees)
        {
            if (m_crushers.size() >= MaxCrushers || !std::isfinite(x) ||
                !std::isfinite(y) || !std::isfinite(z) ||
                !std::isfinite(yawDegrees) || yawDegrees < 0.0f ||
                yawDegrees >= 360.001f)
                return 0;
            for (const auto& s : m_crushers)
            {
                const float dx = s.x - x, dz = s.z - z;
                if (dx * dx + dz * dz < 2.8f * 2.8f &&
                    std::abs(s.y - y) < 2.0f)
                    return 0;
            }
            std::uint64_t next = 1;
            for (const auto& s : m_crushers)
                next = std::max(next, s.id + 1);
            OreCrusherState state;
            state.id = next;
            state.x = x;
            state.y = y;
            state.z = z;
            state.yawDegrees = yawDegrees;
            m_crushers.push_back(std::move(state));
            return next;
        }

        bool Load(std::uint64_t id, const InventoryOreBatch& batch)
        {
            auto* s = Find(id);
            if (!s || s->batch.massKg != 0.0f ||
                batch.processingStage != OreProcessingStage::Raw ||
                batch.massKg < 0.001f || batch.massKg > CapacityKg + 0.001f)
                return false;
            InventoryState state;
            state.oreBatches.push_back(batch);
            if (!PlayerInventory::IsValidState(state))
                return false;
            s->batch = batch;
            s->workDone = 0.0f;
            s->workRequired = RequiredWork(batch);
            return true;
        }

        bool Crank(std::uint64_t id)
        {
            auto* s = Find(id);
            if (!s || s->batch.massKg <= 0.0f ||
                s->batch.processingStage != OreProcessingStage::Raw)
                return false;
            s->workDone = std::min(s->workRequired, s->workDone + 1.0f);
            s->rotationRadians = std::fmod(s->rotationRadians + 0.34f, 6.28318530718f);
            if (s->workDone + 0.0001f >= s->workRequired)
            {
                s->batch.processingStage = OreProcessingStage::Crushed;
                s->batch.displayName = "Crushed " +
                    s->batch.displayName.substr(0,92);
            }
            return true;
        }

        bool Collect(std::uint64_t id, InventoryOreBatch& output)
        {
            auto* s = Find(id);
            if (!s || s->batch.processingStage != OreProcessingStage::Crushed)
                return false;
            output = s->batch;
            s->batch = {};
            s->workDone = 0.0f;
            s->workRequired = 0.0f;
            return true;
        }

        bool Remove(std::uint64_t id)
        {
            for (auto it = m_crushers.begin(); it != m_crushers.end(); ++it)
            {
                if (it->id != id)
                    continue;
                if (it->batch.massKg > 0.0f)
                    return false;
                m_crushers.erase(it);
                return true;
            }
            return false;
        }

    private:
        std::vector<OreCrusherState> m_crushers;
    };
}
