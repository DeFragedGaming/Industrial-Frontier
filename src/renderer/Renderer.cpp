#include "industrial_frontier/renderer/Renderer.hpp"

#include "industrial_frontier/renderer/Shader.hpp"
#include "industrial_frontier/world/TerrainWorld.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <memory>
#include <unordered_map>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float DegreesToRadians = Pi / 180.0f;

        struct Vec3
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        struct Mat4
        {
            float values[16]{};
        };

        Vec3 Add(const Vec3& a, const Vec3& b)
        {
            return { a.x + b.x, a.y + b.y, a.z + b.z };
        }

        Vec3 Subtract(const Vec3& a, const Vec3& b)
        {
            return { a.x - b.x, a.y - b.y, a.z - b.z };
        }

        Vec3 Multiply(const Vec3& value, float scalar)
        {
            return {
                value.x * scalar,
                value.y * scalar,
                value.z * scalar
            };
        }

        float Dot(const Vec3& a, const Vec3& b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        Vec3 Cross(const Vec3& a, const Vec3& b)
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        Vec3 Normalize(const Vec3& value)
        {
            const float length = std::sqrt(Dot(value, value));

            if (length <= 0.00001f)
            {
                return {};
            }

            return Multiply(value, 1.0f / length);
        }

        Mat4 Identity()
        {
            Mat4 result{};

            result.values[0] = 1.0f;
            result.values[5] = 1.0f;
            result.values[10] = 1.0f;
            result.values[15] = 1.0f;

            return result;
        }

        Mat4 MultiplyMatrices(
            const Mat4& a,
            const Mat4& b
        )
        {
            Mat4 result{};

            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    for (int index = 0; index < 4; ++index)
                    {
                        result.values[column * 4 + row] +=
                            a.values[index * 4 + row] *
                            b.values[column * 4 + index];
                    }
                }
            }

            return result;
        }

        Mat4 Perspective(
            float fieldOfView,
            float aspectRatio,
            float nearPlane,
            float farPlane
        )
        {
            Mat4 result{};

            const float tangent = std::tan(
                fieldOfView * 0.5f
            );

            if (aspectRatio <= 0.0f || tangent <= 0.0f)
            {
                return Identity();
            }

            result.values[0] = 1.0f / (aspectRatio * tangent);
            result.values[5] = 1.0f / tangent;

            result.values[10] =
                -(farPlane + nearPlane) /
                (farPlane - nearPlane);

            result.values[11] = -1.0f;

            result.values[14] =
                -(2.0f * farPlane * nearPlane) /
                (farPlane - nearPlane);

            return result;
        }

        Mat4 LookAt(
            const Vec3& position,
            const Vec3& target,
            const Vec3& worldUp
        )
        {
            const Vec3 forward = Normalize(
                Subtract(target, position)
            );

            const Vec3 right = Normalize(
                Cross(forward, worldUp)
            );

            const Vec3 up = Cross(right, forward);

            Mat4 result = Identity();

            result.values[0] = right.x;
            result.values[1] = up.x;
            result.values[2] = -forward.x;

            result.values[4] = right.y;
            result.values[5] = up.y;
            result.values[6] = -forward.y;

            result.values[8] = right.z;
            result.values[9] = up.z;
            result.values[10] = -forward.z;

            result.values[12] = -Dot(right, position);
            result.values[13] = -Dot(up, position);
            result.values[14] = Dot(forward, position);

            return result;
        }

        using GenVertexArraysProc =
            void(APIENTRY*)(GLsizei, GLuint*);

        using BindVertexArrayProc =
            void(APIENTRY*)(GLuint);

        using DeleteVertexArraysProc =
            void(APIENTRY*)(GLsizei, const GLuint*);

        using GenBuffersProc =
            void(APIENTRY*)(GLsizei, GLuint*);

        using BindBufferProc =
            void(APIENTRY*)(GLenum, GLuint);

        using BufferDataProc =
            void(APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);

        using DeleteBuffersProc =
            void(APIENTRY*)(GLsizei, const GLuint*);

        using EnableVertexAttribArrayProc =
            void(APIENTRY*)(GLuint);

        using VertexAttribPointerProc =
            void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);

        using DrawElementsProc =
            void(APIENTRY*)(GLenum, GLsizei, GLenum, const void*);

        template<typename T>
        bool LoadGLFunction(T& function, const char* name)
        {
            function = reinterpret_cast<T>(
                SDL_GL_GetProcAddress(name)
            );

            if (function == nullptr)
            {
                std::cerr << "Failed to load OpenGL function: "
                          << name << '\n';

                return false;
            }

            return true;
        }
    }

    struct Renderer::Impl
    {
        struct GPUChunk
        {
            GLuint vertexArray = 0;
            GLuint vertexBuffer = 0;
            GLuint indexBuffer = 0;
            GLsizei indexCount = 0;
        };

        GenVertexArraysProc GenVertexArrays = nullptr;
        BindVertexArrayProc BindVertexArray = nullptr;
        DeleteVertexArraysProc DeleteVertexArrays = nullptr;

        GenBuffersProc GenBuffers = nullptr;
        BindBufferProc BindBuffer = nullptr;
        BufferDataProc BufferData = nullptr;
        DeleteBuffersProc DeleteBuffers = nullptr;

        EnableVertexAttribArrayProc EnableVertexAttribArray = nullptr;
        VertexAttribPointerProc VertexAttribPointer = nullptr;

        DrawElementsProc DrawElements = nullptr;

        Shader terrainShader;
        TerrainWorld terrainWorld;

        std::unordered_map<std::uint64_t, GPUChunk> gpuChunks;

        Vec3 cameraPosition{ 0.0f, 45.0f, 65.0f };
        Vec3 cameraForward{ 0.0f, -0.35f, -0.94f };
        Vec3 cameraRight{ 1.0f, 0.0f, 0.0f };
        Vec3 cameraUp{ 0.0f, 1.0f, 0.0f };

        float cameraYaw = -90.0f;
        float cameraPitch = -20.0f;

        float movementSpeed = 12.0f;
        float sprintMultiplier = 3.0f;
        float mouseSensitivity = 0.12f;

        bool initialized = false;

        bool LoadFunctions()
        {
            bool success = true;

            success &= LoadGLFunction(
                GenVertexArrays,
                "glGenVertexArrays"
            );

            success &= LoadGLFunction(
                BindVertexArray,
                "glBindVertexArray"
            );

            success &= LoadGLFunction(
                DeleteVertexArrays,
                "glDeleteVertexArrays"
            );

            success &= LoadGLFunction(
                GenBuffers,
                "glGenBuffers"
            );

            success &= LoadGLFunction(
                BindBuffer,
                "glBindBuffer"
            );

            success &= LoadGLFunction(
                BufferData,
                "glBufferData"
            );

            success &= LoadGLFunction(
                DeleteBuffers,
                "glDeleteBuffers"
            );

            success &= LoadGLFunction(
                EnableVertexAttribArray,
                "glEnableVertexAttribArray"
            );

            success &= LoadGLFunction(
                VertexAttribPointer,
                "glVertexAttribPointer"
            );

            success &= LoadGLFunction(
                DrawElements,
                "glDrawElements"
            );

            return success;
        }

        void UpdateCameraVectors()
        {
            const float yaw = cameraYaw * DegreesToRadians;
            const float pitch = cameraPitch * DegreesToRadians;

            cameraForward = Normalize({
                std::cos(yaw) * std::cos(pitch),
                std::sin(pitch),
                std::sin(yaw) * std::cos(pitch)
            });

            cameraRight = Normalize(
                Cross(cameraForward, { 0.0f, 1.0f, 0.0f })
            );

            cameraUp = Normalize(
                Cross(cameraRight, cameraForward)
            );
        }

        std::uint64_t MakeChunkKey(int x, int z) const
        {
            return
                (static_cast<std::uint64_t>(
                    static_cast<std::uint32_t>(x)
                ) << 32) |
                static_cast<std::uint32_t>(z);
        }

        bool UploadChunk(const TerrainChunk& chunk)
        {
            const auto& vertices = chunk.GetVertices();
            const auto& indices = chunk.GetIndices();

            if (vertices.empty() || indices.empty())
            {
                return false;
            }

            GPUChunk gpuChunk{};

            GenVertexArrays(1, &gpuChunk.vertexArray);
            GenBuffers(1, &gpuChunk.vertexBuffer);
            GenBuffers(1, &gpuChunk.indexBuffer);

            if (gpuChunk.vertexArray == 0 ||
                gpuChunk.vertexBuffer == 0 ||
                gpuChunk.indexBuffer == 0)
            {
                DeleteChunk(gpuChunk);
                return false;
            }

            BindVertexArray(gpuChunk.vertexArray);

            BindBuffer(
                GL_ARRAY_BUFFER,
                gpuChunk.vertexBuffer
            );

            BufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    vertices.size() * sizeof(TerrainVertex)
                ),
                vertices.data(),
                GL_STATIC_DRAW
            );

            BindBuffer(
                GL_ELEMENT_ARRAY_BUFFER,
                gpuChunk.indexBuffer
            );

            BufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    indices.size() * sizeof(unsigned int)
                ),
                indices.data(),
                GL_STATIC_DRAW
            );

            const GLsizei stride =
                static_cast<GLsizei>(sizeof(TerrainVertex));

            EnableVertexAttribArray(0);

            VertexAttribPointer(
                0,
                3,
                GL_FLOAT,
                GL_FALSE,
                stride,
                reinterpret_cast<const void*>(
                    offsetof(TerrainVertex, position)
                )
            );

            EnableVertexAttribArray(1);

            VertexAttribPointer(
                1,
                3,
                GL_FLOAT,
                GL_FALSE,
                stride,
                reinterpret_cast<const void*>(
                    offsetof(TerrainVertex, normal)
                )
            );

            EnableVertexAttribArray(2);

            VertexAttribPointer(
                2,
                3,
                GL_FLOAT,
                GL_FALSE,
                stride,
                reinterpret_cast<const void*>(
                    offsetof(TerrainVertex, color)
                )
            );

            BindVertexArray(0);

            gpuChunk.indexCount =
                static_cast<GLsizei>(indices.size());

            gpuChunks.emplace(
                MakeChunkKey(
                    chunk.GetChunkX(),
                    chunk.GetChunkZ()
                ),
                gpuChunk
            );

            return true;
        }

        void DeleteChunk(GPUChunk& chunk)
        {
            if (chunk.indexBuffer != 0)
            {
                DeleteBuffers(1, &chunk.indexBuffer);
                chunk.indexBuffer = 0;
            }

            if (chunk.vertexBuffer != 0)
            {
                DeleteBuffers(1, &chunk.vertexBuffer);
                chunk.vertexBuffer = 0;
            }

            if (chunk.vertexArray != 0)
            {
                DeleteVertexArrays(1, &chunk.vertexArray);
                chunk.vertexArray = 0;
            }
        }

        void ClearGPUChunks()
        {
            for (auto& entry : gpuChunks)
            {
                DeleteChunk(entry.second);
            }

            gpuChunks.clear();
        }

        bool GenerateTerrain()
        {
            if (!terrainWorld.GenerateWorld(3))
            {
                std::cerr << "Failed to generate terrain world.\n";
                return false;
            }

            for (const auto& entry : terrainWorld.GetChunks())
            {
                if (!UploadChunk(entry.second))
                {
                    std::cerr << "Failed to upload terrain chunk.\n";
                    return false;
                }
            }

            std::cout << "Uploaded "
                      << gpuChunks.size()
                      << " terrain chunks to the GPU.\n";

            return !gpuChunks.empty();
        }

        void SetLighting()
        {
            terrainShader.SetVec3(
                "uLightDirection",
                -0.45f,
                -1.0f,
                -0.35f
            );

            terrainShader.SetVec3(
                "uLightColor",
                1.0f,
                0.96f,
                0.88f
            );

            terrainShader.SetVec3(
                "uAmbientColor",
                0.42f,
                0.48f,
                0.58f
            );

            terrainShader.SetFloat(
                "uLightIntensity",
                1.15f
            );

            terrainShader.SetFloat(
                "uAmbientIntensity",
                0.65f
            );

            terrainShader.SetVec3(
                "uViewPosition",
                cameraPosition.x,
                cameraPosition.y,
                cameraPosition.z
            );
        }
    };

    Renderer::Renderer()
        : m_impl(std::make_unique<Impl>())
    {
    }

    Renderer::~Renderer()
    {
        Shutdown();
    }

    bool Renderer::Initialize()
    {
        if (!m_impl)
        {
            m_impl = std::make_unique<Impl>();
        }

        if (m_impl->initialized)
        {
            return true;
        }

        if (!m_impl->LoadFunctions())
        {
            return false;
        }

        if (!m_impl->terrainShader.LoadFromFiles(
            "assets/shaders/terrain.vert",
            "assets/shaders/terrain.frag"
        ))
        {
            std::cerr << "Failed to load terrain shaders.\n";
            return false;
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        glClearColor(
            0.48f,
            0.62f,
            0.78f,
            1.0f
        );

        m_impl->UpdateCameraVectors();

        if (!m_impl->GenerateTerrain())
        {
            m_impl->ClearGPUChunks();
            m_impl->terrainShader.Shutdown();
            return false;
        }

        m_impl->initialized = true;

        std::cout << "Renderer initialized with external terrain shaders.\n";

        return true;
    }

    void Renderer::UpdateCamera(
        float deltaTime,
        float forwardInput,
        float rightInput,
        float verticalInput,
        bool sprint
    )
    {
        if (!m_impl)
        {
            return;
        }

        const float speed =
            m_impl->movementSpeed *
            (sprint ? m_impl->sprintMultiplier : 1.0f);

        Vec3 movement{};

        movement = Add(
            movement,
            Multiply(m_impl->cameraForward, forwardInput)
        );

        movement = Add(
            movement,
            Multiply(m_impl->cameraRight, rightInput)
        );

        movement = Add(
            movement,
            Vec3{ 0.0f, verticalInput, 0.0f }
        );

        const float movementLength =
            std::sqrt(Dot(movement, movement));

        if (movementLength > 1.0f)
        {
            movement = Multiply(movement, 1.0f / movementLength);
        }

        m_impl->cameraPosition = Add(
            m_impl->cameraPosition,
            Multiply(movement, speed * deltaTime)
        );
    }

    void Renderer::RotateCamera(
        float deltaX,
        float deltaY
    )
    {
        if (!m_impl)
        {
            return;
        }

        m_impl->cameraYaw +=
            deltaX * m_impl->mouseSensitivity;

        m_impl->cameraPitch -=
            deltaY * m_impl->mouseSensitivity;

        m_impl->cameraPitch = std::clamp(
            m_impl->cameraPitch,
            -89.0f,
            89.0f
        );

        m_impl->UpdateCameraVectors();
    }

    void Renderer::Render(int width, int height)
    {
        if (!m_impl ||
            !m_impl->initialized ||
            width <= 0 ||
            height <= 0)
        {
            return;
        }

        glViewport(0, 0, width, height);

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        const Mat4 projection = Perspective(
            70.0f * DegreesToRadians,
            static_cast<float>(width) / static_cast<float>(height),
            0.1f,
            2000.0f
        );

        const Mat4 view = LookAt(
            m_impl->cameraPosition,
            Add(
                m_impl->cameraPosition,
                m_impl->cameraForward
            ),
            { 0.0f, 1.0f, 0.0f }
        );

        const Mat4 model = Identity();

        const Mat4 viewProjection = MultiplyMatrices(
            projection,
            view
        );

        const Mat4 mvp = MultiplyMatrices(
            viewProjection,
            model
        );

        m_impl->terrainShader.Use();

        m_impl->terrainShader.SetMat4(
            "uMVP",
            mvp.values
        );

        m_impl->terrainShader.SetMat4(
            "uModel",
            model.values
        );

        m_impl->SetLighting();

        for (const auto& entry : m_impl->gpuChunks)
        {
            const Impl::GPUChunk& chunk = entry.second;

            m_impl->BindVertexArray(chunk.vertexArray);

            m_impl->DrawElements(
                GL_TRIANGLES,
                chunk.indexCount,
                GL_UNSIGNED_INT,
                nullptr
            );
        }

        m_impl->BindVertexArray(0);
    }

    void Renderer::Shutdown()
    {
        if (!m_impl)
        {
            return;
        }

        m_impl->ClearGPUChunks();
        m_impl->terrainShader.Shutdown();

        m_impl->initialized = false;
    }
}