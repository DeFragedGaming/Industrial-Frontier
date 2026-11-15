#include "industrial_frontier/resources/EarlyMetallurgy.hpp"
#include "industrial_frontier/resources/OreAssay.hpp"
#include "industrial_frontier/resources/PrimitiveTier.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

namespace industrial_frontier
{
    namespace
    {
        bool IsSupportedMetal(const std::string& metal)
        {
            return metal == "copper" || metal == "tin" || metal == "lead";
        }
    }

    const char* PrimitiveMetalName(const std::string& id)
    {
        if (id == "tin") return "Tin";
        if (id == "lead") return "Lead";
        return "Copper";
    }

    const char* PrimitiveIngotId(const std::string& id)
    {
        if (id == "tin") return "tin_ingot";
        if (id == "lead") return "lead_ingot";
        return "copper_ingot";
    }

    CopperFeedResult EvaluateCopperFeed(const InventoryOreBatch& batch,
        const MineralDatabase& minerals)
    {
        CopperFeedResult result;
        if (!std::isfinite(batch.massKg) || batch.massKg <= 0.0f ||
            batch.processingStage == OreProcessingStage::Tailings)
        {
            result.message = "No usable copper feed in that batch";
            return result;
        }
        double available = 0.0;
        double weightedRecovery = 0.0;
        bool advanced = false;
        bool variable = false;
        bool hazardous = false;
        for (const auto& component : batch.components)
        {
            if (component.gangue || component.massFraction <= 0.0f)
                continue;
            const auto* mineral = minerals.FindById(component.mineralId);
            if (!mineral || mineral->commodity != "Copper")
                continue;
            if (mineral->id == "enargite")
            {
                hazardous = true;
                continue;
            }
            if (!IsPrimitiveCopperMineral(mineral->id))
            {
                advanced = true;
                continue;
            }
            double fraction = 0.0;
            if (!TheoreticalElementFraction(mineral->formula, "Cu", fraction))
            {
                variable = true;
                continue;
            }
            const float recovery = mineral->processingRouteId == "native_metal" ?
                0.92f : mineral->processingRouteId == "copper_oxide" ? 0.72f : 0.0f;
            if (recovery <= 0.0f)
                continue;
            const double copperPart = component.massFraction * fraction;
            available += copperPart;
            weightedRecovery += copperPart * recovery;
        }
        if (available <= 0.00001)
        {
            result.message = hazardous ?
                "Arsenic-bearing enargite needs industrial treatment" : advanced ?
                "Advanced copper mineral: requires a later furnace" : variable ?
                "Variable-composition copper ore needs advanced refining" :
                "No primitive-tier copper minerals in this batch";
            return result;
        }
        result.supported = true;
        result.copperFraction = static_cast<float>(available);
        result.recoveryFraction = static_cast<float>(weightedRecovery / available);
        result.description = batch.displayName;
        result.message = "Copper-bearing minerals recognized by chemical formula";
        return result;
    }

    PrimitiveMetalFeed EvaluatePrimitiveMetalFeed(const InventoryOreBatch& batch,
        const MineralDatabase& minerals)
    {
        PrimitiveMetalFeed result;
        if (!std::isfinite(batch.massKg) || batch.massKg <= 0.0f ||
            batch.processingStage == OreProcessingStage::Tailings)
        {
            result.message = "EMPTY OR UNUSABLE ORE BATCH";
            return result;
        }
        const CopperFeedResult copper = EvaluateCopperFeed(batch, minerals);
        double recoverableCopper = copper.supported ?
            copper.copperFraction * copper.recoveryFraction : 0.0;
        double recoverableTin = 0.0;
        double recoverableLead = 0.0;
        bool dangerous = false;
        for (const auto& component : batch.components)
        {
            if (component.gangue || component.massFraction <= 0.0f)
                continue;
            const auto* mineral = minerals.FindById(component.mineralId);
            if (!mineral)
                continue;
            if (mineral->id == "enargite")
                dangerous = true;
            double metalFraction = 0.0;
            if (mineral->id == "cassiterite" &&
                TheoreticalElementFraction(mineral->formula, "Sn", metalFraction))
                recoverableTin += component.massFraction * metalFraction * 0.68;
            else if (mineral->id == "cerussite" &&
                TheoreticalElementFraction(mineral->formula, "Pb", metalFraction))
                recoverableLead += component.massFraction * metalFraction * 0.75;
        }
        if (dangerous)
        {
            result.message = "HAZARDOUS ARSENIC-BEARING ORE NEEDS INDUSTRIAL TREATMENT";
            return result;
        }
        const auto winner = std::max({recoverableCopper, recoverableTin, recoverableLead});
        if (winner <= 0.00001)
        {
            result.message = "REQUIRES MORE ADVANCED ORE PROCESSING";
            return result;
        }
        result.supported = true;
        result.metalId = winner == recoverableCopper ? "copper" :
            winner == recoverableTin ? "tin" : "lead";
        result.recoveredMetalFraction = static_cast<float>(winner);
        result.message = "PRIMITIVE-TIER METAL FEED ACCEPTED";
        return result;
    }

    ClayFurnaceState* ClayFurnaceWorld::FindMutable(std::uint64_t id)
    {
        for (auto& furnace : m_furnaces)
            if (furnace.id == id)
                return &furnace;
        return nullptr;
    }

    const ClayFurnaceState* ClayFurnaceWorld::Find(std::uint64_t id) const
    {
        for (const auto& furnace : m_furnaces)
            if (furnace.id == id)
                return &furnace;
        return nullptr;
    }

    const std::vector<ClayFurnaceState>& ClayFurnaceWorld::Furnaces() const
    {
        return m_furnaces;
    }

    bool ClayFurnaceWorld::Place(float x, float y, float z, float yawDegrees)
    {
        if (m_furnaces.size() >= MaxFurnaces ||
            !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
            !std::isfinite(yawDegrees) || yawDegrees < 0.0f || yawDegrees >= 360.0f ||
            std::abs(x) > 100000.0f || std::abs(y) > 100000.0f ||
            std::abs(z) > 100000.0f || m_nextId == 0)
            return false;
        for (const auto& furnace : m_furnaces)
        {
            const float dx = furnace.x - x;
            const float dz = furnace.z - z;
            if (dx * dx + dz * dz < 6.25f)
                return false;
        }
        ClayFurnaceState state;
        state.id = m_nextId++;
        state.x = x;
        state.y = y;
        state.z = z;
        state.yawDegrees = yawDegrees;
        m_furnaces.push_back(std::move(state));
        return true;
    }

    bool ClayFurnaceWorld::AddCharge(std::uint64_t id, float metalKg,
        const std::string& name, const std::string& metalId, float duration)
    {
        auto* f = FindMutable(id);
        if (!f || !IsSupportedMetal(metalId) || !std::isfinite(metalKg) ||
            !std::isfinite(duration) || duration <= 0.0f ||
            duration > MaximumSmeltSeconds || metalKg <= 0.0f ||
            metalKg > MaximumReadyCopperKg || name.empty() || name.size() > 100 ||
            f->chargeCopperKg > 0.00001f ||
            f->readyCopperKg + metalKg > MaximumReadyCopperKg + 0.0001f ||
            (f->readyCopperKg > 0.00001f && f->metalId != metalId) ||
            (f->castCount > 0 && f->metalId != metalId))
            return false;
        f->metalId = metalId;
        f->chargeCopperKg = metalKg;
        f->smeltSeconds = duration;
        f->chargeName = name;
        return true;
    }

    bool ClayFurnaceWorld::AddCharcoal(std::uint64_t id)
    {
        auto* furnace = FindMutable(id);
        if (!furnace || furnace->fuelSeconds > 120.0f)
            return false;
        furnace->fuelSeconds += SecondsPerCharcoal;
        return true;
    }

    bool ClayFurnaceWorld::DiscardResidue(std::uint64_t id)
    {
        auto* f = FindMutable(id);
        if (!f || f->fuelSeconds > 0.0f || f->chargeCopperKg > 0.0f ||
            f->castCount != 0 || f->readyCopperKg <= 0.0f ||
            f->readyCopperKg + 0.0001f >= IngotMassKg)
            return false;
        f->readyCopperKg = 0.0f;
        return true;
    }

    bool ClayFurnaceWorld::Pour(std::uint64_t id)
    {
        auto* f = FindMutable(id);
        if (!f || f->readyCopperKg + 0.0001f < IngotMassKg)
            return false;
        f->readyCopperKg = std::max(0.0f, f->readyCopperKg - IngotMassKg);
        return true;
    }

    bool ClayFurnaceWorld::InstallMold(std::uint64_t id, std::uint32_t capacity)
    {
        auto* f = FindMutable(id);
        if (!f || f->moldCapacity != 0 || (capacity != 1 && capacity != 8))
            return false;
        f->moldCapacity = capacity;
        return true;
    }

    std::uint32_t ClayFurnaceWorld::StartCasting(std::uint64_t id)
    {
        auto* f = FindMutable(id);
        if (!f || f->moldCapacity == 0 || f->castCount != 0 ||
            f->readyCopperKg + 0.0001f < IngotMassKg)
            return 0;
        const auto available = static_cast<std::uint32_t>(
            std::floor((f->readyCopperKg + 0.0001f) / IngotMassKg));
        const auto count = std::min(available, f->moldCapacity);
        f->readyCopperKg = std::max(0.0f, f->readyCopperKg - count * IngotMassKg);
        f->castCount = count;
        f->castingCoolingSeconds = 30.0f;
        return count;
    }

    std::uint32_t ClayFurnaceWorld::CollectCasting(std::uint64_t id)
    {
        auto* f = FindMutable(id);
        if (!f || f->castCount == 0 || f->castingCoolingSeconds > 0.0f)
            return 0;
        const auto amount = f->castCount;
        f->castCount = 0;
        return amount;
    }

    std::uint32_t ClayFurnaceWorld::UninstallMold(std::uint64_t id)
    {
        auto* f = FindMutable(id);
        if (!f || f->castCount > 0 || f->moldCapacity == 0)
            return 0;
        const auto capacity = f->moldCapacity;
        f->moldCapacity = 0;
        return capacity;
    }

    bool ClayFurnaceWorld::Remove(std::uint64_t id)
    {
        auto it = std::find_if(m_furnaces.begin(), m_furnaces.end(),
            [&](const ClayFurnaceState& furnace) { return furnace.id == id; });
        if (it == m_furnaces.end() || it->fuelSeconds > 0.0f ||
            it->chargeCopperKg > 0.0f || it->moldCapacity > 0 ||
            it->castCount > 0 || it->readyCopperKg + 0.0001f >= IngotMassKg)
            return false;
        m_furnaces.erase(it);
        return true;
    }

    void ClayFurnaceWorld::Update(float seconds)
    {
        if (!std::isfinite(seconds) || seconds <= 0.0f)
            return;
        const float dt = std::min(seconds, 0.25f);
        for (auto& f : m_furnaces)
        {
            f.castingCoolingSeconds = std::max(0.0f, f.castingCoolingSeconds - dt);
            const float burning = std::min(dt, f.fuelSeconds);
            f.fuelSeconds = std::max(0.0f, f.fuelSeconds - burning);
            if (f.chargeCopperKg <= 0.0f || burning <= 0.0f)
                continue;
            f.smeltSeconds = std::max(0.0f, f.smeltSeconds - burning);
            if (f.smeltSeconds <= 0.0f)
            {
                f.readyCopperKg += f.chargeCopperKg;
                f.chargeCopperKg = 0.0f;
                f.chargeName.clear();
            }
        }
    }

    void ClayFurnaceWorld::Clear()
    {
        m_furnaces.clear();
        m_nextId = 1;
    }

    bool ClayFurnaceWorld::Validate(const std::vector<ClayFurnaceState>& states)
    {
        if (states.size() > MaxFurnaces)
            return false;
        std::unordered_set<std::uint64_t> ids;
        for (const auto& f : states)
        {
            if (f.id == 0 || !ids.insert(f.id).second ||
                !std::isfinite(f.x) || !std::isfinite(f.y) || !std::isfinite(f.z) ||
                !std::isfinite(f.fuelSeconds) || !std::isfinite(f.smeltSeconds) ||
                !std::isfinite(f.chargeCopperKg) || !std::isfinite(f.readyCopperKg) ||
                !std::isfinite(f.castingCoolingSeconds) ||
                !std::isfinite(f.yawDegrees) ||
                f.yawDegrees < 0.0f || f.yawDegrees >= 360.0f ||
                std::abs(f.x) > 100000.0f || std::abs(f.y) > 100000.0f ||
                std::abs(f.z) > 100000.0f || f.fuelSeconds < 0.0f ||
                f.fuelSeconds > 200.0f || f.smeltSeconds < 0.0f ||
                f.smeltSeconds > MaximumSmeltSeconds ||
                f.chargeCopperKg < 0.0f || f.chargeCopperKg > MaximumReadyCopperKg ||
                f.readyCopperKg < 0.0f || f.readyCopperKg > MaximumReadyCopperKg ||
                !IsSupportedMetal(f.metalId) || f.chargeName.size() > 100 ||
                (f.chargeCopperKg > 0.0f && f.chargeName.empty()) ||
                (f.moldCapacity != 0 && f.moldCapacity != 1 && f.moldCapacity != 8) ||
                f.castCount > f.moldCapacity ||
                f.castingCoolingSeconds < 0.0f || f.castingCoolingSeconds > 30.001f ||
                (f.castCount == 0 && f.castingCoolingSeconds > 0.0001f))
                return false;
        }
        return true;
    }

    bool ClayFurnaceWorld::Restore(const std::vector<ClayFurnaceState>& states)
    {
        if (!Validate(states))
            return false;
        m_furnaces = states;
        m_nextId = 1;
        for (const auto& furnace : states)
            m_nextId = std::max(m_nextId, furnace.id + 1);
        return true;
    }
}
