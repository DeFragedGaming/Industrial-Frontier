#pragma once

#include <cmath>
#include <vector>

namespace industrial_frontier
{
    struct StructureCollider
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float radius = 0.0f;
        float height = 0.0f;
        float halfWidth = 0.0f;
        float halfDepth = 0.0f;
        float yawDegrees = 0.0f;
    };

    inline bool StructureBlocksPoint(
        const std::vector<StructureCollider>* colliders,
        float x, float y, float z, float padding)
    {
        if (!colliders)
            return false;
        for (const StructureCollider& s : *colliders)
        {
            if (y < s.y - 0.01f || y > s.y + s.height)
                continue;
            const float dx = x - s.x;
            const float dz = z - s.z;
            if (s.halfWidth > 0.0f && s.halfDepth > 0.0f)
            {
                const float radians = s.yawDegrees * 0.01745329251994329577f;
                const float cosine = std::cos(radians);
                const float sine = std::sin(radians);
                const float localX = cosine * dx + sine * dz;
                const float localZ = -sine * dx + cosine * dz;
                if (std::abs(localX) <= s.halfWidth + padding &&
                    std::abs(localZ) <= s.halfDepth + padding)
                    return true;
            }
            else
            {
                const float radius = s.radius + padding;
                if (dx * dx + dz * dz <= radius * radius)
                    return true;
            }
        }
        return false;
    }
}
