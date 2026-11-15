#pragma once

#include "industrial_frontier/world/TerrainGenerator.hpp"
#include "industrial_frontier/world/VoxelTerrain.hpp"
#include "industrial_frontier/resources/OutcropInteraction.hpp"
#include "industrial_frontier/forest/ForestGenerator.hpp"
#include "industrial_frontier/player/StructureCollision.hpp"

#include <vector>

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
            const VoxelTerrain& terrain
        );

        void Rotate(float deltaX, float deltaY);
        void SetOutcropColliders(const std::vector<OutcropCollisionShape>* shapes);
        void SetTreeColliders(const std::vector<ForestTree>* trees);
        void SetStructureColliders(const std::vector<StructureCollider>* colliders);

        void SetPosition(float x, float y, float z);
        void SetViewRotation(float yaw, float pitch);

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

        bool GetCollisionSafeCameraPosition(
            const VoxelTerrain& terrain,
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
        const std::vector<OutcropCollisionShape>* m_outcropColliders = nullptr;
        const std::vector<ForestTree>* m_treeColliders = nullptr;
        const std::vector<StructureCollider>* m_structureColliders = nullptr;

        bool IsCollisionSolid(
            const VoxelTerrain& terrain, float x, float y, float z) const;
        bool IsOutcropBlocking(float x, float y, float z) const;
        bool IsTreeBlocking(float x, float y, float z, float padding) const;
        bool IsStructureBlocking(float x, float y, float z, float padding) const;

        bool IsCapsuleClear(
            float x,
            float y,
            float z,
            const VoxelTerrain& terrain
        ) const;

        bool TryHorizontalStep(
            float deltaX,
            float deltaZ,
            const VoxelTerrain& terrain
        );

        bool SnapToGround(
            float maximumDrop,
            const VoxelTerrain& terrain
        );

        void ApplyVerticalMovement(
            float deltaTime,
            const VoxelTerrain& terrain
        );
    };
}
