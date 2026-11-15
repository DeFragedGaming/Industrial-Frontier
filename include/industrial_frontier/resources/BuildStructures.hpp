#pragma once

#include "industrial_frontier/resources/PlayerInventory.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace industrial_frontier
{
    enum class BuildKind : int
    {
        Chest = 0,
        Foundation = 1,
        Wall = 2,
        Doorframe = 3,
        WindowWall = 4,
        Roof = 5,
        Support = 6
    };

    struct BuildStructure
    {
        std::uint64_t id = 0;
        BuildKind kind = BuildKind::Chest;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yawDegrees = 0.0f;
        InventoryState contents{};
    };

    inline std::string_view BuildKitId(BuildKind kind)
    {
        switch (kind)
        {
        case BuildKind::Chest: return "wooden_chest_kit";
        case BuildKind::Foundation: return "log_foundation_kit";
        case BuildKind::Wall: return "log_wall_kit";
        case BuildKind::Doorframe: return "log_doorframe_kit";
        case BuildKind::WindowWall: return "log_window_kit";
        case BuildKind::Roof: return "log_roof_kit";
        case BuildKind::Support: return "log_support_kit";
        }
        return {};
    }

    inline std::string_view BuildPieceName(BuildKind kind)
    {
        switch (kind)
        {
        case BuildKind::Chest: return "WOODEN CHEST";
        case BuildKind::Foundation: return "LOG FOUNDATION";
        case BuildKind::Wall: return "LOG WALL";
        case BuildKind::Doorframe: return "LOG DOORFRAME";
        case BuildKind::WindowWall: return "LOG WINDOW WALL";
        case BuildKind::Roof: return "LOG ROOF";
        case BuildKind::Support: return "LOG SUPPORT";
        }
        return "STRUCTURE";
    }

    inline bool BuildKitKind(std::string_view item, BuildKind& kind)
    {
        for (int value = 0; value <= static_cast<int>(BuildKind::Support); ++value)
        {
            const auto next = static_cast<BuildKind>(value);
            if (BuildKitId(next) == item)
            {
                kind = next;
                return true;
            }
        }
        return false;
    }

    inline bool IsCabinPiece(BuildKind kind)
    {
        return kind != BuildKind::Chest;
    }

    class BuildWorld
    {
    public:
        static constexpr std::size_t MaxStructures = 640;

        static bool Validate(const std::vector<BuildStructure>& list)
        {
            if (list.size() > MaxStructures)
                return false;
            std::unordered_set<std::uint64_t> used;
            for (const auto& s : list)
            {
                if (s.id == 0 || !used.insert(s.id).second ||
                    static_cast<int>(s.kind) < 0 ||
                    static_cast<int>(s.kind) > static_cast<int>(BuildKind::Support) ||
                    !std::isfinite(s.x) || !std::isfinite(s.y) ||
                    !std::isfinite(s.z) || !std::isfinite(s.yawDegrees) ||
                    std::abs(s.x) > 100000.0f || std::abs(s.y) > 100000.0f ||
                    std::abs(s.z) > 100000.0f || s.yawDegrees < 0.0f ||
                    s.yawDegrees >= 360.001f ||
                    !PlayerInventory::IsValidState(s.contents) ||
                    (s.kind != BuildKind::Chest &&
                        (!s.contents.items.empty() || !s.contents.oreBatches.empty())))
                    return false;
            }
            return true;
        }

        const std::vector<BuildStructure>& Structures() const { return m_structures; }

        void Clear() { m_structures.clear(); }

        bool Restore(const std::vector<BuildStructure>& structures)
        {
            if (!Validate(structures))
                return false;
            m_structures = structures;
            return true;
        }

        const BuildStructure* Find(std::uint64_t id) const
        {
            for (const auto& s : m_structures)
                if (s.id == id)
                    return &s;
            return nullptr;
        }

        bool HasNearbyFoundation(float x, float z, float radius = 3.2f) const
        {
            for (const auto& s : m_structures)
            {
                if (s.kind != BuildKind::Foundation)
                    continue;
                const float dx = x - s.x;
                const float dz = z - s.z;
                if (dx * dx + dz * dz <= radius * radius)
                    return true;
            }
            return false;
        }

        const BuildStructure* NearestFoundation(float x, float z,
            float radius = 4.0f) const
        {
            const BuildStructure* nearest = nullptr;
            float best = radius * radius;
            for (const auto& s : m_structures)
            {
                if (s.kind != BuildKind::Foundation)
                    continue;
                const float dx = x - s.x;
                const float dz = z - s.z;
                const float d = dx * dx + dz * dz;
                if (d < best)
                {
                    best = d;
                    nearest = &s;
                }
            }
            return nearest;
        }

        bool CanPlace(BuildKind kind, float x, float y, float z,
            float yawDegrees) const
        {
            if (m_structures.size() >= MaxStructures ||
                !std::isfinite(x) || !std::isfinite(y) ||
                !std::isfinite(z) || !std::isfinite(yawDegrees))
                return false;
            const bool wall = kind == BuildKind::Wall ||
                kind == BuildKind::Doorframe || kind == BuildKind::WindowWall;
            for (const auto& current : m_structures)
            {
                const float dx = current.x - x;
                const float dz = current.z - z;
                const float distance2 = dx * dx + dz * dz;
                if (kind == BuildKind::Chest || current.kind == BuildKind::Chest)
                {
                    if (distance2 < 1.7f * 1.7f && std::abs(y - current.y) < 2.4f)
                        return false;
                    continue;
                }
                const bool currentWall = current.kind == BuildKind::Wall ||
                    current.kind == BuildKind::Doorframe ||
                    current.kind == BuildKind::WindowWall;
                if (kind == current.kind && distance2 < 0.35f * 0.35f &&
                    std::abs(y - current.y) < 0.45f &&
                    (kind != BuildKind::Support ||
                        std::abs(yawDegrees - current.yawDegrees) < 22.5f))
                    return false;
                if (wall && currentWall && distance2 < 0.3f * 0.3f &&
                    std::abs(y - current.y) < 0.45f)
                    return false;
            }
            return true;
        }

        std::uint64_t Place(BuildKind kind, float x, float y, float z, float yawDegrees)
        {
            if (!CanPlace(kind, x, y, z, yawDegrees))
                return 0;
            std::uint64_t next = 1;
            for (const auto& s : m_structures)
                next = std::max(next, s.id + 1);
            BuildStructure s;
            s.id = next;
            s.kind = kind;
            s.x = x;
            s.y = y;
            s.z = z;
            s.yawDegrees = yawDegrees;
            if (kind == BuildKind::Chest)
                s.contents.carryCapacityBonusKg = 900.0f;
            m_structures.push_back(s);
            return next;
        }

        bool UpdateChest(std::uint64_t id, const InventoryState& state)
        {
            if (!PlayerInventory::IsValidState(state))
                return false;
            for (auto& s : m_structures)
            {
                if (s.id == id && s.kind == BuildKind::Chest)
                {
                    s.contents = state;
                    return true;
                }
            }
            return false;
        }

        bool Remove(std::uint64_t id)
        {
            for (auto it = m_structures.begin(); it != m_structures.end(); ++it)
            {
                if (it->id == id)
                {
                    if (it->kind == BuildKind::Chest &&
                        (!it->contents.items.empty() || !it->contents.oreBatches.empty()))
                        return false;
                    m_structures.erase(it);
                    return true;
                }
            }
            return false;
        }

    private:
        std::vector<BuildStructure> m_structures;
    };
}
