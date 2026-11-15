#pragma once

#include "industrial_frontier/player/StructureCollision.hpp"
#include "industrial_frontier/resources/BuildStructures.hpp"

#include <cmath>
#include <vector>

namespace industrial_frontier
{
    inline void AppendBuildColliders(
        std::vector<StructureCollider>& result, const BuildStructure& s)
    {
        const float radians = s.yawDegrees * 0.01745329251994329577f;
        const float cosine = std::cos(radians);
        const float sine = std::sin(radians);
        const auto box = [&](float lx, float lz, float lowY,
            float highY, float halfWidth, float halfDepth)
        {
            StructureCollider shape{};
            shape.x = s.x + cosine * lx - sine * lz;
            shape.y = s.y + lowY;
            shape.z = s.z + sine * lx + cosine * lz;
            shape.height = highY - lowY;
            shape.halfWidth = halfWidth;
            shape.halfDepth = halfDepth;
            shape.yawDegrees = s.yawDegrees;
            result.push_back(shape);
        };
        switch (s.kind)
        {
        case BuildKind::Chest:
            box(0.0f, 0.0f, 0.03f, 1.00f, 0.71f, 0.51f);
            break;
        case BuildKind::Foundation:
            box(0.0f, 0.0f, 0.06f, 0.48f, 1.70f, 1.63f);
            break;
        case BuildKind::Wall:
            box(0.0f, 0.0f, 0.20f, 3.05f, 1.67f, 0.18f);
            break;
        case BuildKind::Doorframe:
            box(-1.11f, 0.0f, 0.20f, 3.04f, 0.55f, 0.18f);
            box(1.11f, 0.0f, 0.20f, 3.04f, 0.55f, 0.18f);
            box(0.0f, 0.0f, 2.53f, 3.05f, 1.67f, 0.18f);
            break;
        case BuildKind::WindowWall:
            box(0.0f, 0.0f, 0.20f, 1.12f, 1.67f, 0.18f);
            box(0.0f, 0.0f, 2.22f, 3.05f, 1.67f, 0.18f);
            box(-1.11f, 0.0f, 1.12f, 2.22f, 0.55f, 0.18f);
            box(1.11f, 0.0f, 1.12f, 2.22f, 0.55f, 0.18f);
            break;
        case BuildKind::Roof:
            box(-0.82f, 0.0f, 3.00f, 4.12f, 0.86f, 1.76f);
            box(0.82f, 0.0f, 3.00f, 4.12f, 0.86f, 1.76f);
            break;
        case BuildKind::Support:
            box(0.0f, 0.0f, 0.05f, 3.40f, 0.22f, 0.22f);
            break;
        }
    }
}
