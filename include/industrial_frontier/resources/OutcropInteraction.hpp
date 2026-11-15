#pragma once

#include "industrial_frontier/resources/GeologyGenerator.hpp"
#include "industrial_frontier/resources/OreAssay.hpp"
#include "industrial_frontier/world/VoxelTerrain.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

namespace industrial_frontier
{
    struct OutcropCollisionShape
    {
        std::uint64_t id = 0;
        DepositKind kind = DepositKind::FiniteOutcrop;
        std::string mineralId;
        std::string mineralName;
        std::string assayLabel;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float radius = 1.0f;
        float height = 1.0f;
    };

    inline bool BuildOutcropCollisionShape(
        const VoxelTerrain& terrain,
        const GeologicalDeposit& deposit,
        OutcropCollisionShape& result)
    {
        const float groundY = terrain.GetSurfaceHeight(deposit.x, deposit.z);
        if (groundY < VoxelTerrain::BedrockSurfaceY + 3.0f ||
            groundY > 145.0f ||
            terrain.IsDepositDepleted(deposit.id))
        {
            return false;
        }
        result = {};
        result.id = deposit.id;
        result.kind = deposit.kind;
        result.mineralId = deposit.mineralId;
        result.x = deposit.x;
        result.y = groundY;
        result.z = deposit.z;
        result.radius = deposit.radius;
        result.height = deposit.radius *
            (deposit.continuousExtraction ? 0.28f : 0.85f);
        if (const GeologyGenerator* geology = terrain.GetGeology())
        {
            if (const MineralDefinition* mineral = geology->FindMineral(deposit.mineralId))
            {
                result.mineralName = mineral->name;
            }
        }
        if (const GeologyGenerator* geology = terrain.GetGeology())
        {
            result.assayLabel = FormatOreAssay(deposit, *geology);
        }
        if (result.mineralName.empty())
        {
            result.mineralName = deposit.mineralId;
        }
        return true;
    }

    inline constexpr int OutcropRingCount = 8;
    inline constexpr int OutcropSideCount = 16;

    inline float OutcropBottomY(const OutcropCollisionShape& shape)
    {
        const float buriedDepth = shape.kind == DepositKind::PermanentNode
            ? shape.radius * 0.68f : shape.radius * 1.25f;
        return std::max(VoxelTerrain::BedrockSurfaceY + 0.3f,
            shape.y - buriedDepth);
    }

    inline float OutcropRingY(
        const OutcropCollisionShape& shape, int ring)
    {
        const float bottom = OutcropBottomY(shape);
        const float buriedDepth = shape.y - bottom;
        if (shape.kind == DepositKind::PermanentNode)
        {
            switch (ring)
            {
                case 0: return bottom + buriedDepth * 0.035f;
                case 1: return bottom + buriedDepth * 0.32f;
                case 2: return bottom + buriedDepth * 0.76f;
                case 3: return shape.y - buriedDepth * 0.04f;
                case 4: return shape.y + shape.height * 0.24f;
                case 5: return shape.y + shape.height * 0.64f;
                case 6: return shape.y + shape.height * 0.78f;
                default: return shape.y + shape.height * 0.90f;
            }
        }
        switch (ring)
        {
            case 0: return bottom + buriedDepth * 0.035f;
            case 1: return bottom + buriedDepth * 0.25f;
            case 2: return bottom + buriedDepth * 0.61f;
            case 3: return shape.y - buriedDepth * 0.10f;
            case 4: return shape.y + shape.height * 0.20f;
            case 5: return shape.y + shape.height * 0.48f;
            case 6: return shape.y + shape.height * 0.76f;
            default: return shape.y + shape.height * 0.95f;
        }
    }

    inline float OutcropRingRadiusFactor(
        const OutcropCollisionShape& shape, int ring)
    {
        if (shape.kind == DepositKind::PermanentNode)
        {
            constexpr std::array<float, OutcropRingCount> permanentFactors = {
                0.21f, 0.60f, 0.86f, 0.95f,
                1.00f, 0.90f, 0.62f, 0.23f
            };
            return permanentFactors[static_cast<std::size_t>(
                std::clamp(ring, 0, OutcropRingCount - 1))];
        }
        constexpr std::array<float, OutcropRingCount> finiteFactors = {
            0.18f, 0.53f, 0.82f, 0.96f,
            0.80f, 0.62f, 0.38f, 0.12f
        };
        return finiteFactors[static_cast<std::size_t>(
            std::clamp(ring, 0, OutcropRingCount - 1))];
    }

    inline float OutcropTopY(const OutcropCollisionShape& shape)
    {
        return shape.kind == DepositKind::PermanentNode
            ? shape.y + shape.height * 0.93f
            : shape.y + shape.height;
    }

    inline float OutcropAngularVariation(
        const OutcropCollisionShape& shape, float angle)
    {
        const float offset = static_cast<float>(shape.id & 1023u) * 0.014f;
        if (shape.kind == DepositKind::PermanentNode)
        {
            return 0.95f + 0.035f * std::sin(angle * 5.0f + offset) +
                0.018f * std::sin(angle * 9.0f - offset * 0.7f);
        }
        return 0.89f + 0.085f * std::sin(angle * 3.0f + offset);
    }

    inline float OutcropRadiusAtHeight(
        const OutcropCollisionShape& shape, float y)
    {
        const float bottom = OutcropBottomY(shape);
        const float top = OutcropTopY(shape);
        if (y < bottom || y > top)
        {
            return 0.0f;
        }
        const float firstY = OutcropRingY(shape, 0);
        if (y < firstY)
        {
            return shape.radius * OutcropRingRadiusFactor(shape, 0) *
                (y - bottom) / std::max(0.0001f, firstY - bottom);
        }
        for (int ring = 0; ring + 1 < OutcropRingCount; ++ring)
        {
            const float low = OutcropRingY(shape, ring);
            const float high = OutcropRingY(shape, ring + 1);
            if (y <= high)
            {
                const float t = std::clamp((y - low) /
                    std::max(0.0001f, high - low), 0.0f, 1.0f);
                return shape.radius * (OutcropRingRadiusFactor(shape, ring) * (1.0f - t) +
                    OutcropRingRadiusFactor(shape, ring + 1) * t);
            }
        }
        const float lastY = OutcropRingY(shape, OutcropRingCount - 1);
        return shape.radius * OutcropRingRadiusFactor(shape, OutcropRingCount - 1) *
            (top - y) / std::max(0.0001f, top - lastY);
    }

    inline bool IsInsideOutcrop(
        const OutcropCollisionShape& shape,
        float x, float y, float z,
        float padding = 0.0f)
    {
        if (y < OutcropBottomY(shape) - padding ||
            y > OutcropTopY(shape) + padding)
        {
            return false;
        }
        const float dx = x - shape.x;
        const float dz = z - shape.z;
        const float angle = std::atan2(dz, dx);
        const float allowed = OutcropRadiusAtHeight(shape, y) *
            OutcropAngularVariation(shape, angle) + padding;
        return dx * dx + dz * dz <= allowed * allowed;
    }

    inline bool RaycastOutcrop(
        const OutcropCollisionShape& shape,
        float originX, float originY, float originZ,
        float directionX, float directionY, float directionZ,
        float maximumDistance, float& hitDistance)
    {
        const float extent = shape.radius + 0.4f;
        const float dx = shape.x - originX;
        const float dz = shape.z - originZ;
        const float flatDistance = std::sqrt(dx * dx + dz * dz);
        if (flatDistance > maximumDistance + extent)
        {
            return false;
        }
        float previous = 0.0f;
        for (float distance = 0.08f;
             distance <= maximumDistance + 0.0001f;
             distance += 0.08f)
        {
            if (IsInsideOutcrop(shape,
                originX + directionX * distance,
                originY + directionY * distance,
                originZ + directionZ * distance))
            {
                float start = previous;
                float end = distance;
                for (int i = 0; i < 5; ++i)
                {
                    const float midpoint = (start + end) * 0.5f;
                    if (IsInsideOutcrop(shape,
                        originX + directionX * midpoint,
                        originY + directionY * midpoint,
                        originZ + directionZ * midpoint))
                    {
                        end = midpoint;
                    }
                    else
                    {
                        start = midpoint;
                    }
                }
                hitDistance = end;
                return true;
            }
            previous = distance;
        }
        return false;
    }
}
