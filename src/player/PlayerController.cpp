#include "industrial_frontier/player/PlayerController.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float DegreesToRadians = Pi / 180.0f;
        constexpr float CollisionSkin = 0.025f;
        constexpr float HorizontalSubstep = 0.16f;
        constexpr float VerticalSubstep = 0.065f;
        constexpr float GroundProbeStep = 0.05f;

        struct CollisionRing
        {
            float height;
            float radiusFactor;
        };

        constexpr std::array<CollisionRing, 5> CollisionRings = {{
            {0.10f, 0.50f},
            {0.36f, 0.96f},
            {0.93f, 0.96f},
            {1.50f, 0.86f},
            {1.78f, 0.48f}
        }};
    }

    void PlayerController::Initialize(const TerrainGenerator& terrain)
    {
        m_x = 0.0f;
        m_z = 0.0f;

        const float radius = m_collisionRadius;
        float height = terrain.GetHeight(m_x, m_z);
        height = std::max(height, terrain.GetHeight(m_x + radius, m_z));
        height = std::max(height, terrain.GetHeight(m_x - radius, m_z));
        height = std::max(height, terrain.GetHeight(m_x, m_z + radius));
        height = std::max(height, terrain.GetHeight(m_x, m_z - radius));

        m_y = height + m_groundOffset + 0.06f;
        m_verticalVelocity = 0.0f;
        m_grounded = true;
        m_yaw = -90.0f;
        m_pitch = 0.0f;
        m_initialized = true;
    }

    bool PlayerController::IsCapsuleClear(
        float x,
        float y,
        float z,
        const VoxelTerrain& terrain) const
    {
        for (std::size_t ringIndex = 0;
             ringIndex < CollisionRings.size();
             ++ringIndex)
        {
            const CollisionRing ring = CollisionRings[ringIndex];
            const float sampleY = y + ring.height;
            const float radius = m_collisionRadius * ring.radiusFactor;

            if (terrain.SampleDensity(x, sampleY, z) > CollisionSkin ||
                terrain.SampleDensity(x + radius, sampleY, z) > CollisionSkin ||
                terrain.SampleDensity(x - radius, sampleY, z) > CollisionSkin ||
                terrain.SampleDensity(x, sampleY, z + radius) > CollisionSkin ||
                terrain.SampleDensity(x, sampleY, z - radius) > CollisionSkin)
            {
                return false;
            }

            if (ringIndex == 1 || ringIndex == 2)
            {
                const float diagonal = radius * 0.70710678118f;

                if (terrain.SampleDensity(x + diagonal, sampleY, z + diagonal) > CollisionSkin ||
                    terrain.SampleDensity(x + diagonal, sampleY, z - diagonal) > CollisionSkin ||
                    terrain.SampleDensity(x - diagonal, sampleY, z + diagonal) > CollisionSkin ||
                    terrain.SampleDensity(x - diagonal, sampleY, z - diagonal) > CollisionSkin)
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool PlayerController::TryHorizontalStep(
        float deltaX,
        float deltaZ,
        const VoxelTerrain& terrain)
    {
        if (deltaX == 0.0f && deltaZ == 0.0f)
        {
            return true;
        }

        const float candidateX = m_x + deltaX;
        const float candidateZ = m_z + deltaZ;

        if (IsCapsuleClear(candidateX, m_y, candidateZ, terrain))
        {
            m_x = candidateX;
            m_z = candidateZ;
            return true;
        }

        if (!m_grounded)
        {
            return false;
        }

        for (float rise = 0.10f;
             rise <= m_maxStepHeight + 0.0001f;
             rise += 0.10f)
        {
            if (!IsCapsuleClear(m_x, m_y + rise, m_z, terrain))
            {
                continue;
            }

            if (IsCapsuleClear(candidateX, m_y + rise, candidateZ, terrain))
            {
                m_y += rise;
                m_x = candidateX;
                m_z = candidateZ;
                return true;
            }
        }

        if (IsCapsuleClear(m_x, m_y + m_maxStepHeight, m_z, terrain) &&
            IsCapsuleClear(candidateX, m_y + m_maxStepHeight, candidateZ, terrain))
        {
            m_y += m_maxStepHeight;
            m_x = candidateX;
            m_z = candidateZ;
            return true;
        }

        return false;
    }

    bool PlayerController::SnapToGround(
        float maximumDrop,
        const VoxelTerrain& terrain)
    {
        const float startY = m_y;

        for (float distance = GroundProbeStep;
             distance <= maximumDrop + GroundProbeStep * 0.5f;
             distance += GroundProbeStep)
        {
            const float candidateY = startY - distance;

            if (!IsCapsuleClear(m_x, candidateY, m_z, terrain))
            {
                m_grounded = true;
                m_verticalVelocity = 0.0f;
                return true;
            }

            m_y = candidateY;
        }

        m_grounded = false;
        return false;
    }

    void PlayerController::ApplyVerticalMovement(
        float deltaTime,
        const VoxelTerrain& terrain)
    {
        m_verticalVelocity += m_gravity * deltaTime;

        const float deltaY = m_verticalVelocity * deltaTime;

        const int steps = std::max(
            1,
            static_cast<int>(
                std::ceil(std::abs(deltaY) / VerticalSubstep)
            )
        );

        const float stepY = deltaY / static_cast<float>(steps);

        for (int step = 0; step < steps; ++step)
        {
            if (IsCapsuleClear(m_x, m_y + stepY, m_z, terrain))
            {
                m_y += stepY;
                continue;
            }

            if (stepY <= 0.0f)
            {
                m_grounded = true;
            }

            m_verticalVelocity = 0.0f;
            return;
        }

        m_grounded = false;

        if (m_verticalVelocity <= 0.0f)
        {
            SnapToGround(0.09f, terrain);
        }
    }

    void PlayerController::Update(
        float deltaTime,
        const PlayerInput& input,
        const VoxelTerrain& terrain)
    {
        if (!m_initialized || deltaTime <= 0.0f)
        {
            return;
        }

        deltaTime = std::min(deltaTime, 0.05f);

        if (!IsCapsuleClear(m_x, m_y, m_z, terrain))
        {
            for (float rise = 0.05f; rise <= 1.50f; rise += 0.05f)
            {
                if (IsCapsuleClear(m_x, m_y + rise, m_z, terrain))
                {
                    m_y += rise;
                    m_grounded = false;
                    m_verticalVelocity = 0.0f;
                    break;
                }
            }
        }

        const float yaw = m_yaw * DegreesToRadians;
        const float forwardX = std::cos(yaw);
        const float forwardZ = std::sin(yaw);
        const float rightX = -forwardZ;
        const float rightZ = forwardX;

        float moveX = forwardX * input.forward + rightX * input.right;
        float moveZ = forwardZ * input.forward + rightZ * input.right;

        const float length = std::sqrt(moveX * moveX + moveZ * moveZ);

        if (length > 1.0f)
        {
            moveX /= length;
            moveZ /= length;
        }

        const float speed = input.sprint ? m_sprintSpeed : m_walkSpeed;
        const float moveDistance = speed * deltaTime;
        const float totalDistance = std::sqrt(moveX * moveX + moveZ * moveZ) * moveDistance;

        const int steps = std::max(
            1,
            static_cast<int>(std::ceil(totalDistance / HorizontalSubstep))
        );

        const float stepX = moveX * moveDistance / static_cast<float>(steps);
        const float stepZ = moveZ * moveDistance / static_cast<float>(steps);

        for (int step = 0; step < steps; ++step)
        {
            if (TryHorizontalStep(stepX, stepZ, terrain))
            {
                continue;
            }

            TryHorizontalStep(stepX, 0.0f, terrain);
            TryHorizontalStep(0.0f, stepZ, terrain);
        }

        if (input.jumpPressed && m_grounded)
        {
            m_verticalVelocity = m_jumpVelocity;
            m_grounded = false;
        }

        if (m_grounded)
        {
            SnapToGround(0.45f, terrain);
        }

        if (!m_grounded)
        {
            ApplyVerticalMovement(deltaTime, terrain);
        }
    }

    void PlayerController::Rotate(float deltaX, float deltaY)
    {
        if (!m_initialized)
        {
            return;
        }

        m_yaw += deltaX * m_mouseSensitivity;
        m_pitch -= deltaY * m_mouseSensitivity;
        m_pitch = std::clamp(m_pitch, -85.0f, 85.0f);
    }

    void PlayerController::SetPosition(float x, float y, float z)
    {
        m_x = x;
        m_y = y;
        m_z = z;
        m_verticalVelocity = 0.0f;
        m_grounded = false;
    }

    float PlayerController::GetX() const
    {
        return m_x;
    }

    float PlayerController::GetY() const
    {
        return m_y;
    }

    float PlayerController::GetZ() const
    {
        return m_z;
    }

    float PlayerController::GetYaw() const
    {
        return m_yaw;
    }

    float PlayerController::GetPitch() const
    {
        return m_pitch;
    }

    float PlayerController::GetEyeHeight() const
    {
        return m_eyeHeight;
    }

    void PlayerController::GetForward(float& x, float& y, float& z) const
    {
        const float yaw = m_yaw * DegreesToRadians;
        const float pitch = m_pitch * DegreesToRadians;
        const float horizontal = std::cos(pitch);

        x = std::cos(yaw) * horizontal;
        y = std::sin(pitch);
        z = std::sin(yaw) * horizontal;
    }

    void PlayerController::GetFirstPersonCameraPosition(
        float& x,
        float& y,
        float& z) const
    {
        x = m_x;
        y = m_y + m_eyeHeight;
        z = m_z;
    }

    bool PlayerController::IsGrounded() const
    {
        return m_grounded;
    }

    bool PlayerController::IsInitialized() const
    {
        return m_initialized;
    }
}
