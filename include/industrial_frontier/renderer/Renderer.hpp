
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

        void UpdateCamera(
            float deltaTime,
            float forwardInput,
            float rightInput,
            float verticalInput,
            bool sprint
        );

        void RotateCamera(float deltaX, float deltaY);

        void Render(int width, int height);
        void Shutdown();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
