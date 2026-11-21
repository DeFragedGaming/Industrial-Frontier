#pragma once

#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/world/RiverSystem.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace industrial_frontier
{
    enum class PrimitiveMachineKind : int { Mill = 0, Sluice = 1 };

    struct PrimitiveMachineState
    {
        std::uint64_t id = 0;
        PrimitiveMachineKind kind = PrimitiveMachineKind::Mill;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yawDegrees = 0.0f;
        float workDone = 0.0f;
        float workRequired = 0.0f;
        float rotationRadians = 0.0f;
        InventoryOreBatch feed;
        InventoryOreBatch tailings;
    };

    class PrimitiveProcessingMachines
    {
    public:
        static constexpr std::size_t MaxMachines = 96;
        static constexpr float MillCapacityKg = 25.0f;
        static constexpr float SluiceCapacityKg = 20.0f;

        static float Capacity(PrimitiveMachineKind kind)
        {
            return kind == PrimitiveMachineKind::Mill ? MillCapacityKg : SluiceCapacityKg;
        }

        static bool InputStage(PrimitiveMachineKind kind, OreProcessingStage stage)
        {
            return (kind == PrimitiveMachineKind::Mill && stage == OreProcessingStage::Crushed) ||
                (kind == PrimitiveMachineKind::Sluice && stage == OreProcessingStage::Ground);
        }

        static bool OutputStage(PrimitiveMachineKind kind, OreProcessingStage stage)
        {
            return (kind == PrimitiveMachineKind::Mill && stage == OreProcessingStage::Ground) ||
                (kind == PrimitiveMachineKind::Sluice && stage == OreProcessingStage::Concentrate);
        }

        static bool ValidBatch(const InventoryOreBatch& batch)
        {
            InventoryState state;
            state.oreBatches.push_back(batch);
            return PlayerInventory::IsValidState(state);
        }

        static bool Validate(const std::vector<PrimitiveMachineState>& states)
        {
            if (states.size() > MaxMachines) return false;
            std::unordered_set<std::uint64_t> ids;
            for (const auto& m : states)
            {
                if (!m.id || !ids.insert(m.id).second ||
                    (m.kind != PrimitiveMachineKind::Mill && m.kind != PrimitiveMachineKind::Sluice) ||
                    !std::isfinite(m.x) || !std::isfinite(m.y) || !std::isfinite(m.z) ||
                    !std::isfinite(m.yawDegrees) || !std::isfinite(m.workDone) ||
                    !std::isfinite(m.workRequired) || !std::isfinite(m.rotationRadians) ||
                    std::abs(m.x)>100000.0f || std::abs(m.y)>100000.0f ||
                    std::abs(m.z)>100000.0f || m.yawDegrees<0.0f ||
                    m.yawDegrees>360.001f || m.workDone<0.0f ||
                    m.workRequired<0.0f || m.workRequired>1000.0f ||
                    m.workDone>m.workRequired+0.001f || m.rotationRadians<0.0f ||
                    m.rotationRadians>1000000.0f)
                    return false;
                if (m.feed.massKg > 0.0f)
                {
                    if (!ValidBatch(m.feed) || m.feed.massKg > Capacity(m.kind)+0.001f ||
                        (!InputStage(m.kind,m.feed.processingStage) &&
                         !OutputStage(m.kind,m.feed.processingStage)) ||
                        m.workRequired < 1.0f)
                        return false;
                    const bool finished = OutputStage(m.kind,m.feed.processingStage);
                    if (finished != (m.workDone+0.001f>=m.workRequired))
                        return false;
                    if (m.tailings.massKg>0.0f &&
                        (m.kind!=PrimitiveMachineKind::Sluice || !finished ||
                         m.tailings.processingStage!=OreProcessingStage::Tailings ||
                         !ValidBatch(m.tailings)))
                        return false;
                }
                else if (m.feed.massKg != 0.0f || m.workDone!=0.0f ||
                    m.workRequired!=0.0f || m.tailings.massKg!=0.0f)
                    return false;
            }
            return true;
        }

        const std::vector<PrimitiveMachineState>& Machines() const { return m_machines; }
        void Clear() { m_machines.clear(); }

        bool Restore(const std::vector<PrimitiveMachineState>& states)
        {
            if (!Validate(states)) return false;
            m_machines = states;
            return true;
        }

        PrimitiveMachineState* Find(std::uint64_t id)
        {
            for (auto& m : m_machines) if (m.id==id) return &m;
            return nullptr;
        }

        const PrimitiveMachineState* Find(std::uint64_t id) const
        {
            for (const auto& m : m_machines) if (m.id==id) return &m;
            return nullptr;
        }

        std::uint64_t Place(PrimitiveMachineKind kind, float x,float y,float z,
            float yawDegrees, bool flowing)
        {
            if (m_machines.size()>=MaxMachines ||
                (kind!=PrimitiveMachineKind::Mill && kind!=PrimitiveMachineKind::Sluice) ||
                (kind==PrimitiveMachineKind::Sluice && !flowing) ||
                !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
                !std::isfinite(yawDegrees) || yawDegrees<0.0f || yawDegrees>=360.001f)
                return 0;
            for (const auto& m:m_machines)
            {
                const float dx=m.x-x,dz=m.z-z;
                if (dx*dx+dz*dz<3.5f*3.5f && std::abs(m.y-y)<2.5f)
                    return 0;
            }
            std::uint64_t id=1;
            for (const auto& m:m_machines) id=std::max(id,m.id+1);
            PrimitiveMachineState m;
            m.id=id;m.kind=kind;m.x=x;m.y=y;m.z=z;m.yawDegrees=yawDegrees;
            m_machines.push_back(m);
            return id;
        }

        bool Load(std::uint64_t id,const InventoryOreBatch& batch)
        {
            auto* m=Find(id);
            if (!m || m->feed.massKg>0.0f ||
                !InputStage(m->kind,batch.processingStage) ||
                !ValidBatch(batch) || batch.massKg>Capacity(m->kind)+0.001f)
                return false;
            m->feed=batch;
            m->tailings={};
            m->workDone=0.0f;
            m->workRequired=std::ceil(3.0f+batch.massKg*
                (m->kind==PrimitiveMachineKind::Mill?1.2f:1.75f));
            return true;
        }

        static float Capture(std::string_view id, bool gangue)
        {
            if (gangue) return 0.065f;
            if (id=="native_gold" || id=="gold" || id=="cassiterite" ||
                id=="galena" || id=="magnetite" || id=="native_copper") return 0.90f;
            if (id=="hematite" || id=="chromite" || id=="wolframite" ||
                id=="ilmenite" || id=="pyrite") return 0.83f;
            if (id=="malachite" || id=="azurite" || id=="cuprite" ||
                id=="chalcopyrite" || id=="bornite" || id=="chalcocite" ||
                id=="sphalerite" || id=="cerussite" || id=="siderite") return 0.65f;
            return 0.40f;
        }

        static InventoryOreBatch Split(const InventoryOreBatch& feed,
            bool concentrate)
        {
            InventoryOreBatch result=feed;
            result.components.clear();
            result.processingStage=concentrate?OreProcessingStage::Concentrate:
                OreProcessingStage::Tailings;
            result.displayName=(concentrate?"Sluice Concentrate ":"Sluice Tailings ")+
                feed.primaryMineralId;
            result.massKg=0.0f;
            result.oreMineralFraction=0.0f;
            double total=0.0;
            for (const auto& c:feed.components)
                total+=static_cast<double>(feed.massKg)*c.massFraction*
                    (concentrate?Capture(c.mineralId,c.gangue):1.0f-Capture(c.mineralId,c.gangue));
            if (total<0.000001) return {};
            result.massKg=static_cast<float>(total);
            double useful=0.0;
            for (const auto& c:feed.components)
            {
                const double part=static_cast<double>(feed.massKg)*c.massFraction*
                    (concentrate?Capture(c.mineralId,c.gangue):1.0f-Capture(c.mineralId,c.gangue));
                if (part <= 0.0) continue;
                auto component=c;
                component.massFraction=static_cast<float>(part/total);
                if (!component.gangue) useful+=part/total;
                result.components.push_back(std::move(component));
            }
            result.oreMineralFraction=static_cast<float>(useful);
            return result;
        }

        static void Finish(PrimitiveMachineState& m)
        {
            if (m.kind==PrimitiveMachineKind::Mill)
            {
                m.feed.processingStage=OreProcessingStage::Ground;
                m.feed.displayName="Ground "+m.feed.primaryMineralId+" ore";
            }
            else
            {
                const auto feed=m.feed;
                m.feed=Split(feed,true);
                m.tailings=Split(feed,false);
            }
            m.workDone=m.workRequired;
        }

        bool Crank(std::uint64_t id)
        {
            auto* m=Find(id);
            if (!m || m->kind!=PrimitiveMachineKind::Mill ||
                m->feed.massKg<=0.0f ||
                m->feed.processingStage!=OreProcessingStage::Crushed)
                return false;
            m->workDone=std::min(m->workRequired,m->workDone+1.0f);
            m->rotationRadians=std::fmod(m->rotationRadians+0.36f,6.28318530718f);
            if (m->workDone+0.0001f>=m->workRequired)
                Finish(*m);
            return true;
        }

        bool Flow(std::uint64_t id,float deltaSeconds,float speed)
        {
            auto* m=Find(id);
            if (!m || m->kind!=PrimitiveMachineKind::Sluice ||
                m->feed.massKg<=0.0f ||
                m->feed.processingStage!=OreProcessingStage::Ground ||
                !std::isfinite(deltaSeconds) || deltaSeconds<=0.0f ||
                !std::isfinite(speed) || speed<0.4f)
                return false;
            m->workDone=std::min(m->workRequired,m->workDone+
                std::min(deltaSeconds,0.25f)*speed);
            m->rotationRadians=std::fmod(m->rotationRadians+
                deltaSeconds*speed,6.28318530718f);
            if (m->workDone+0.0001f>=m->workRequired)
                Finish(*m);
            return true;
        }

        bool Collect(std::uint64_t id,InventoryOreBatch& product,
            InventoryOreBatch& tailings)
        {
            auto* m=Find(id);
            if (!m || !OutputStage(m->kind,m->feed.processingStage))
                return false;
            product=m->feed;
            tailings=m->tailings;
            m->feed={};m->tailings={};m->workDone=0.0f;m->workRequired=0.0f;
            return true;
        }

        bool Remove(std::uint64_t id)
        {
            for (auto it=m_machines.begin();it!=m_machines.end();++it)
            {
                if (it->id!=id) continue;
                if (it->feed.massKg>0.0f) return false;
                m_machines.erase(it);
                return true;
            }
            return false;
        }

    private:
        std::vector<PrimitiveMachineState> m_machines;
    };
}
