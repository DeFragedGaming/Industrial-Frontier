
#pragma once

#include <memory>
#include <string>

namespace industrial_frontier
{
    class Model
    {
    public:
        Model();
        ~Model();

        Model(const Model&) = delete;
        Model& operator=(const Model&) = delete;

        bool Load(const std::string& path);

        void Draw(
            const float* viewProjection,
            float x,
            float y,
            float z,
            float yawDegrees
        );

        bool IsLoaded() const;
        void Shutdown();

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
