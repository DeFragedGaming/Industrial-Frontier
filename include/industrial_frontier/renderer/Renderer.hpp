
#pragma once

#include <memory>

namespace industrial_frontier
{
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

        void Render(int width, int height);
        void Shutdown();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
