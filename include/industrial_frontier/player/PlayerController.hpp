
#pragma once

#include "industrial_frontier/world/TerrainGenerator.hpp"

namespace industrial_frontier
{
    struct PlayerInput
    {
        float forward = 0.0f;
        float right = 0.0f;
        bool sprint = false;
        bool jumpPressed = false;
    };

    class PlayerController
    {
    public:
        PlayerController() = default;

        void Initialize(const TerrainGenerator& terrain);

        void Update(
            float deltaTime,
            const PlayerInput& input,
            const TerrainGenerator& terrain
        );

        void Rotate(float deltaX, float deltaY);

        void SetPosition(float x, float y, float z);

        float GetX() const;
        float GetY() const;
        float GetZ() const;

        float GetYaw() const;
        float GetPitch() const;

        float GetEyeHeight() const;

        void GetForward(
            float& x,
            float& y,
            float& z
        ) const;

        void GetFirstPersonCameraPosition(
            float& x,
            float& y,
            float& z
        ) const;

        bool IsGrounded() const;
        bool IsInitialized() const;

    private:
        float m_x = 0.0f;
        float m_y = 0.0f;
        float m_z = 0.0f;

        float m_verticalVelocity = 0.0f;

        float m_yaw = -90.0f;
        float m_pitch = 0.0f;

        float m_walkSpeed = 5.0f;
        float m_sprintSpeed = 8.0f;

        float m_jumpVelocity = 7.0f;
        float m_gravity = -22.0f;

        float m_eyeHeight = 1.60f;
        float m_collisionRadius = 0.35f;

        float m_maxStepHeight = 0.65f;
        float m_groundOffset = 0.04f;

        float m_mouseSensitivity = 0.12f;

        bool m_grounded = false;
        bool m_initialized = false;

        float SampleGroundHeight(
            float x,
            float z,
            const TerrainGenerator& terrain
        ) const;

        bool CanMoveTo(
            float x,
            float z,
            const TerrainGenerator& terrain
        ) const;
    };
}
