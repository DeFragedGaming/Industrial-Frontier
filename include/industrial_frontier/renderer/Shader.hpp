#pragma once

#include <memory>
#include <string>

namespace industrial_frontier
{
    class Shader
    {
    public:
        Shader();
        ~Shader();

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;

        Shader(Shader&&) noexcept;
        Shader& operator=(Shader&&) noexcept;

        bool LoadFromFiles(
            const std::string& vertexPath,
            const std::string& fragmentPath
        );

        void Use() const;
        void Shutdown();

        bool IsValid() const;

        unsigned int GetProgramID() const;

        void SetBool(
            const std::string& name,
            bool value
        ) const;

        void SetInt(
            const std::string& name,
            int value
        ) const;

        void SetFloat(
            const std::string& name,
            float value
        ) const;

        void SetVec3(
            const std::string& name,
            float x,
            float y,
            float z
        ) const;

        void SetMat4(
            const std::string& name,
            const float* matrix
        ) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}