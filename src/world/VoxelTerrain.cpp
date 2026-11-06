#include "industrial_frontier/world/VoxelTerrain.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <utility>

namespace industrial_frontier
{
    VoxelTerrain::VoxelTerrain(
        const TerrainSettings& settings,
        float chunkSize,
        float voxelSpacing)
        : m_generator(settings),
          m_chunkSize(std::max(chunkSize, 1.0f)),
          m_voxelSpacing(std::max(voxelSpacing, 0.1f))
    {
    }

    float VoxelTerrain::Clamp01(float value)
    {
        return std::clamp(value, 0.0f, 1.0f);
    }

    float VoxelTerrain::SmoothFalloff(float value)
    {
        const float t = Clamp01(value);
        return t * t * (3.0f - 2.0f * t);
    }

    float VoxelTerrain::DistanceSquared(
        float ax,
        float ay,
        float az,
        float bx,
        float by,
        float bz)
    {
        const float dx = ax - bx;
        const float dy = ay - by;
        const float dz = az - bz;
        return dx * dx + dy * dy + dz * dz;
    }

    float VoxelTerrain::SampleDensity(
        float x,
        float y,
        float z) const
    {
        float density = m_generator.GetHeight(x, z) - y;

        std::shared_lock<std::shared_mutex> lock(m_mutex);

        for (const VoxelEdit& edit : m_edits)
        {
            const float radius = edit.radius * edit.strength;
            if (radius <= 0.0f)
            {
                continue;
            }

            const float limit = radius + m_voxelSpacing * 3.0f;
            const float distanceSquared = DistanceSquared(
                x, y, z, edit.x, edit.y, edit.z
            );

            if (distanceSquared > limit * limit)
            {
                continue;
            }

            const float signedSphereDistance =
                std::sqrt(distanceSquared) - radius;

            if (edit.addMaterial)
            {
                density = std::max(density, -signedSphereDistance);
            }
            else
            {
                density = std::min(density, signedSphereDistance);
            }
        }

        return density;
    }

    bool VoxelTerrain::IsSolid(
        float x,
        float y,
        float z) const
    {
        return SampleDensity(x, y, z) > 0.0f;
    }

    bool VoxelTerrain::DigSphere(
        float x,
        float y,
        float z,
        float radius,
        float strength)
    {
        VoxelEdit edit{};
        edit.x = x;
        edit.y = y;
        edit.z = z;
        edit.radius = radius;
        edit.strength = strength;
        edit.addMaterial = false;
        return ApplyEdit(edit);
    }

    bool VoxelTerrain::FillSphere(
        float x,
        float y,
        float z,
        float radius,
        float strength)
    {
        VoxelEdit edit{};
        edit.x = x;
        edit.y = y;
        edit.z = z;
        edit.radius = radius;
        edit.strength = strength;
        edit.addMaterial = true;
        return ApplyEdit(edit);
    }

    bool VoxelTerrain::ApplyEdit(const VoxelEdit& edit)
    {
        if (!std::isfinite(edit.x) ||
            !std::isfinite(edit.y) ||
            !std::isfinite(edit.z) ||
            !std::isfinite(edit.radius) ||
            !std::isfinite(edit.strength) ||
            edit.radius <= 0.0f ||
            edit.strength <= 0.0f)
        {
            return false;
        }

        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_edits.push_back(edit);
        ++m_revision;
        return true;
    }

    VoxelChunkRange VoxelTerrain::GetAffectedChunks(
        const VoxelEdit& edit) const
    {
        const float radius =
            std::max(edit.radius * edit.strength, 0.0f);
        const float influenceRadius = radius + m_voxelSpacing;

        const auto toChunk = [this](float position)
        {
            return static_cast<int>(
                std::floor(position / m_chunkSize)
            );
        };

        VoxelChunkRange range{};
        range.minX = toChunk(edit.x - influenceRadius);
        range.maxX = toChunk(edit.x + influenceRadius);
        range.minZ = toChunk(edit.z - influenceRadius);
        range.maxZ = toChunk(edit.z + influenceRadius);
        return range;
    }

    std::vector<VoxelEdit> VoxelTerrain::GetEdits() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_edits;
    }

    void VoxelTerrain::SetEdits(
        const std::vector<VoxelEdit>& edits)
    {
        std::vector<VoxelEdit> validEdits;
        validEdits.reserve(edits.size());

        for (const VoxelEdit& edit : edits)
        {
            if (std::isfinite(edit.x) &&
                std::isfinite(edit.y) &&
                std::isfinite(edit.z) &&
                std::isfinite(edit.radius) &&
                std::isfinite(edit.strength) &&
                edit.radius > 0.0f &&
                edit.strength > 0.0f)
            {
                validEdits.push_back(edit);
            }
        }

        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_edits = std::move(validEdits);
        ++m_revision;
    }

    void VoxelTerrain::ClearEdits()
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);

        if (!m_edits.empty())
        {
            m_edits.clear();
            ++m_revision;
        }
    }

    float VoxelTerrain::GetChunkSize() const
    {
        return m_chunkSize;
    }

    float VoxelTerrain::GetVoxelSpacing() const
    {
        return m_voxelSpacing;
    }

    const TerrainGenerator& VoxelTerrain::GetGenerator() const
    {
        return m_generator;
    }

    std::uint64_t VoxelTerrain::GetRevision() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_revision;
    }
}
