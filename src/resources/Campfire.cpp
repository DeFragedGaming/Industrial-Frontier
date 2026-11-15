#include "industrial_frontier/resources/Campfire.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace industrial_frontier
{
    namespace
    {
        bool Finite(float v)
        {
            return std::isfinite(v) && std::abs(v) < 10000000.0f;
        }
    }

    void CampfireWorld::Clear()
    {
        m_fires.clear();
        m_nextId = 1;
    }

    bool CampfireWorld::Place(float x, float y, float z, float yawDegrees)
    {
        if (!Finite(x) || !Finite(y) || !Finite(z) ||
            !Finite(yawDegrees) || yawDegrees < 0.0f || yawDegrees >= 360.0f ||
            m_fires.size() >= MaxCampfires ||
            m_nextId == std::numeric_limits<std::uint64_t>::max())
            return false;
        for (const auto& fire : m_fires)
        {
            const float dx = fire.x - x;
            const float dz = fire.z - z;
            if (dx * dx + dz * dz < 3.24f &&
                std::abs(fire.y - y) < 2.0f)
                return false;
        }
        m_fires.push_back({m_nextId++, x, y, z, 0.0f, 0.0f, 0, false, yawDegrees});
        return true;
    }

    CampfireState* CampfireWorld::Find(std::uint64_t id)
    {
        for (auto& fire : m_fires)
            if (fire.id == id)
                return &fire;
        return nullptr;
    }

    const CampfireState* CampfireWorld::Find(std::uint64_t id) const
    {
        for (const auto& fire : m_fires)
            if (fire.id == id)
                return &fire;
        return nullptr;
    }

    bool CampfireWorld::AddLog(std::uint64_t id)
    {
        auto* fire = Find(id);
        if (!fire || fire->covered ||
            fire->fuelSeconds > 86400.0f - SecondsPerLog)
            return false;
        fire->fuelSeconds += SecondsPerLog;
        return true;
    }

    bool CampfireWorld::Cover(std::uint64_t id)
    {
        auto* fire = Find(id);
        if (!fire || fire->covered || fire->fuelSeconds <= 0.0f)
            return false;
        fire->covered = true;
        fire->burnProgress = 0.0f;
        fire->fuelSeconds = std::ceil(fire->fuelSeconds / SecondsPerLog) *
            SecondsPerLog;
        return true;
    }

    bool CampfireWorld::Uncover(std::uint64_t id)
    {
        auto* fire = Find(id);
        if (!fire || !fire->covered || fire->fuelSeconds > 0.0f)
            return false;
        fire->covered = false;
        return true;
    }

    bool CampfireWorld::Remove(std::uint64_t id)
    {
        for (auto it = m_fires.begin(); it != m_fires.end(); ++it)
        {
            if (it->id != id)
                continue;
            if (it->fuelSeconds > 0.0f || it->covered ||
                it->charcoalReady > 0)
                return false;
            m_fires.erase(it);
            return true;
        }
        return false;
    }

    void CampfireWorld::Update(float deltaTime)
    {
        if (!std::isfinite(deltaTime))
            return;
        const float dt = std::clamp(deltaTime, 0.0f, 0.25f);
        for (auto& fire : m_fires)
        {
            if (fire.fuelSeconds <= 0.0f)
                continue;
            const float burn = std::min(fire.fuelSeconds, dt);
            fire.fuelSeconds = std::max(0.0f, fire.fuelSeconds - burn);
            if (!fire.covered)
                continue;
            fire.burnProgress += burn;
            if (fire.burnProgress + 0.0001f >= SecondsPerLog)
            {
                fire.burnProgress = std::max(0.0f,
                    fire.burnProgress - SecondsPerLog);
                if (fire.charcoalReady < 999)
                    ++fire.charcoalReady;
            }
        }
    }

    bool CampfireWorld::Collect(std::uint64_t id, std::uint32_t& quantity)
    {
        auto* fire = Find(id);
        quantity = 0;
        if (!fire || fire->charcoalReady == 0)
            return false;
        quantity = fire->charcoalReady;
        fire->charcoalReady = 0;
        if (fire->fuelSeconds <= 0.0f)
            fire->covered = false;
        return true;
    }

    const std::vector<CampfireState>& CampfireWorld::Fires() const
    {
        return m_fires;
    }

    bool CampfireWorld::Validate(const std::vector<CampfireState>& states)
    {
        if (states.size() > MaxCampfires)
            return false;
        std::unordered_set<std::uint64_t> ids;
        for (const auto& fire : states)
        {
            if (!fire.id || !ids.insert(fire.id).second ||
                !Finite(fire.x) || !Finite(fire.y) || !Finite(fire.z) ||
                !Finite(fire.fuelSeconds) || !Finite(fire.burnProgress) ||
                !Finite(fire.yawDegrees) || fire.yawDegrees < 0.0f ||
                fire.yawDegrees >= 360.0f ||
                fire.fuelSeconds < 0.0f || fire.fuelSeconds > 86400.0f ||
                fire.burnProgress < 0.0f ||
                fire.burnProgress >= SecondsPerLog + 0.0001f ||
                fire.charcoalReady > 999)
                return false;
        }
        return true;
    }

    bool CampfireWorld::Restore(const std::vector<CampfireState>& states)
    {
        if (!Validate(states))
            return false;
        m_fires = states;
        std::uint64_t highest = 0;
        for (const auto& fire : m_fires)
            highest = std::max(highest, fire.id);
        if (highest == std::numeric_limits<std::uint64_t>::max())
        {
            Clear();
            return false;
        }
        m_nextId = highest + 1;
        return true;
    }
}
