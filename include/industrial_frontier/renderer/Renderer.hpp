
#pragma once

#include <cstddef>
#include <memory>
#include "industrial_frontier/save/SaveManager.hpp"

namespace industrial_frontier
{
    struct HUDWorldState
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        bool grounded = false;
        bool terrainUpdating = false;
        std::size_t loadedChunks = 0;
        bool hasTarget = false;
        float targetDistance = 0.0f;
        const char* targetMaterial = "NONE";
    };

    class Renderer
    {
    public:
        Renderer();
        ~Renderer();

        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;

        bool Initialize();

        void UpdatePlayer(
            float deltaTime,
            float forwardInput,
            float rightInput,
            bool sprint,
            bool jumpPressed
        );

        void RotatePlayerCamera(
            float deltaX,
            float deltaY
        );

        void SetCinematicMode(bool enabled);

        void UpdateCinematic(
            float deltaTime,
            bool introduction,
            float progress
        );

        bool DigTerrain(float radius = 2.0f, float reach = 8.0f);
        bool GetHUDWorldState(HUDWorldState& state) const;
        bool CaptureWorld(WorldSave& save) const;
        bool RestoreWorld(const WorldSave& save);
        bool StartFreshWorld();

        void Render(int width, int height);
        void Shutdown();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
