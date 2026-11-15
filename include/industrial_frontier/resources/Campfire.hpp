#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace industrial_frontier
{
    struct CampfireState
    {
        std::uint64_t id = 0;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float fuelSeconds = 0.0f;
        float burnProgress = 0.0f;
        std::uint32_t charcoalReady = 0;
        bool covered = false;
        float yawDegrees = 0.0f;
    };

    class CampfireWorld
    {
    public:
        static constexpr float SecondsPerLog = 45.0f;
        static constexpr std::size_t MaxCampfires = 128;

        void Clear();
        bool Place(float x, float y, float z, float yawDegrees = 0.0f);
        CampfireState* Find(std::uint64_t id);
        const CampfireState* Find(std::uint64_t id) const;
        bool AddLog(std::uint64_t id);
        bool Cover(std::uint64_t id);
        bool Uncover(std::uint64_t id);
        bool Remove(std::uint64_t id);
        void Update(float deltaTime);
        bool Collect(std::uint64_t id, std::uint32_t& quantity);
        const std::vector<CampfireState>& Fires() const;
        bool Restore(const std::vector<CampfireState>& states);
        static bool Validate(const std::vector<CampfireState>& states);

    private:
        std::vector<CampfireState> m_fires;
        std::uint64_t m_nextId = 1;
    };
}
