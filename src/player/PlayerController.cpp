#include "industrial_frontier/player/PlayerController.hpp"

#include <algorithm>
#include <cmath>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float DegreesToRadians = Pi / 180.0f;
    }

    void PlayerController::Initialize(const TerrainGenerator& terrain)
    {
        m_x = 0.0f;
        m_z = 0.0f;
        m_y = terrain.GetHeight(m_x, m_z) + m_groundOffset;
        m_verticalVelocity = 0.0f;
        m_grounded = true;
        m_yaw = -90.0f;
        m_pitch = 0.0f;
        m_initialized = true;
    }

    float PlayerController::SampleGroundHeight(
        float x, float z, const TerrainGenerator& terrain) const
    {
        const float radius = m_collisionRadius;

        float height = terrain.GetHeight(x, z);

        height = std::max(height, terrain.GetHeight(x + radius, z));
        height = std::max(height, terrain.GetHeight(x - radius, z));
        height = std::max(height, terrain.GetHeight(x, z + radius));
        height = std::max(height, terrain.GetHeight(x, z - radius));

        return height;
    }

    bool PlayerController::CanMoveTo(
        float x, float z, const TerrainGenerator& terrain) const
    {
        const float height = SampleGroundHeight(x, z, terrain);

        if (m_grounded && height > m_y + m_maxStepHeight)
        {
            return false;
        }

        return true;
    }

    void PlayerController::Update(
        float deltaTime,
        const PlayerInput& input,
        const TerrainGenerator& terrain)
    {
        if (!m_initialized || deltaTime <= 0.0f)
        {
            return;
        }

        deltaTime = std::min(deltaTime, 0.05f);

        const float yaw = m_yaw * DegreesToRadians;

        const float forwardX = std::cos(yaw);
        const float forwardZ = std::sin(yaw);

        // Camera-relative right direction.
        const float rightX = -forwardZ;
        const float rightZ = forwardX;

        float moveX =
            forwardX * input.forward +
            rightX * input.right;

        float moveZ =
            forwardZ * input.forward +
            rightZ * input.right;

        const float length = std::sqrt(
            moveX * moveX + moveZ * moveZ
        );

        if (length > 1.0f)
        {
            moveX /= length;
            moveZ /= length;
        }

        const float speed =
            input.sprint ? m_sprintSpeed : m_walkSpeed;

        const float distance = speed * deltaTime;

        const float movementDistance =
            std::sqrt(moveX * moveX + moveZ * moveZ) * distance;

        const int steps = std::max(
            1,
            static_cast<int>(
                std::ceil(movementDistance / 0.20f)
            )
        );

        const float stepX =
            moveX * distance / static_cast<float>(steps);

        const float stepZ =
            moveZ * distance / static_cast<float>(steps);

        for (int step = 0; step < steps; ++step)
        {
            const float candidateX = m_x + stepX;
            const float candidateZ = m_z + stepZ;

            if (CanMoveTo(candidateX, candidateZ, terrain))
            {
                m_x = candidateX;
                m_z = candidateZ;
            }
            else
            {
                if (CanMoveTo(m_x + stepX, m_z, terrain))
                {
                    m_x += stepX;
                }

                if (CanMoveTo(m_x, m_z + stepZ, terrain))
                {
                    m_z += stepZ;
                }
            }
        }

        if (input.jumpPressed && m_grounded)
        {
            m_verticalVelocity = m_jumpVelocity;
            m_grounded = false;
        }

        m_verticalVelocity += m_gravity * deltaTime;
        m_y += m_verticalVelocity * deltaTime;

        const float groundHeight =
            SampleGroundHeight(m_x, m_z, terrain);

        if (m_y <= groundHeight + m_groundOffset)
        {
            m_y = groundHeight + m_groundOffset;
            m_verticalVelocity = 0.0f;
            m_grounded = true;
        }
        else
        {
            m_grounded = false;
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

    void PlayerController::GetForward(
        float& x, float& y, float& z) const
    {
        const float yaw = m_yaw * DegreesToRadians;
        const float pitch = m_pitch * DegreesToRadians;

        const float horizontal = std::cos(pitch);

        x = std::cos(yaw) * horizontal;
        y = std::sin(pitch);
        z = std::sin(yaw) * horizontal;
    }

    void PlayerController::GetFirstPersonCameraPosition(
        float& x, float& y, float& z) const
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