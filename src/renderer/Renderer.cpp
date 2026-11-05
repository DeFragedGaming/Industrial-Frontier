
#include "industrial_frontier/renderer/Renderer.hpp"

#include "industrial_frontier/player/PlayerController.hpp"
#include "industrial_frontier/renderer/Shader.hpp"
#include "industrial_frontier/world/TerrainWorld.hpp"
#include "industrial_frontier/world/TerrainGenerationWorker.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <utility>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float DegreesToRadians = Pi / 180.0f;

        constexpr int LoadRadius = 5;
        constexpr int UnloadRadius = 6;
        constexpr int MaxUploadsPerFrame = 2;

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
            return {
                a.x + b.x,
                a.y + b.y,
                a.z + b.z
            };
        }

        Vec3 Subtract(const Vec3& a, const Vec3& b)
        {
            return {
                a.x - b.x,
                a.y - b.y,
                a.z - b.z
            };
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
            return
                a.x * b.x +
                a.y * b.y +
                a.z * b.z;
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

        float SmoothStep(float value)
        {
            const float t = std::clamp(value, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
        {
            return {
                Lerp(a.x, b.x, t),
                Lerp(a.y, b.y, t),
                Lerp(a.z, b.z, t)
            };
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

        Mat4 MultiplyMatrices(const Mat4& a, const Mat4& b)
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
            float farPlane)
        {
            Mat4 result{};

            const float tangent =
                std::tan(fieldOfView * 0.5f);

            if (aspectRatio <= 0.0f || tangent <= 0.0f)
            {
                return Identity();
            }

            result.values[0] =
                1.0f / (aspectRatio * tangent);

            result.values[5] =
                1.0f / tangent;

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
            const Vec3& worldUp)
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
            void(APIENTRY*)(
                GLenum,
                GLsizeiptr,
                const void*,
                GLenum
            );

        using DeleteBuffersProc =
            void(APIENTRY*)(GLsizei, const GLuint*);

        using EnableVertexAttribArrayProc =
            void(APIENTRY*)(GLuint);

        using VertexAttribPointerProc =
            void(APIENTRY*)(
                GLuint,
                GLint,
                GLenum,
                GLboolean,
                GLsizei,
                const void*
            );

        using DrawElementsProc =
            void(APIENTRY*)(
                GLenum,
                GLsizei,
                GLenum,
                const void*
            );

        template<typename T>
        bool LoadGLFunction(T& function, const char* name)
        {
            function = reinterpret_cast<T>(
                SDL_GL_GetProcAddress(name)
            );

            if (function == nullptr)
            {
                std::cerr
                    << "Failed to load OpenGL function: "
                    << name
                    << '\n';

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
        TerrainGenerationWorker terrainWorker;
        PlayerController player;

        std::unordered_map<std::uint64_t, GPUChunk> gpuChunks;

        Vec3 cameraPosition{};
        Vec3 cameraForward{0.0f, 0.0f, -1.0f};
        Vec3 cameraTarget{};

        Vec3 playerCameraPosition{};
        Vec3 playerCameraForward{0.0f, 0.0f, -1.0f};

        Vec3 cinematicPosition{};
        Vec3 cinematicTarget{};

        float cinematicTime = 0.0f;

        int currentChunkX = 0;
        int currentChunkZ = 0;

        bool cinematicMode = false;
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

        std::uint64_t MakeChunkKey(int x, int z) const
        {
            return
                (
                    static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(x)
                    ) << 32
                ) |
                static_cast<std::uint32_t>(z);
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

            chunk.indexCount = 0;
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

            const auto result = gpuChunks.emplace(
                MakeChunkKey(
                    chunk.GetChunkX(),
                    chunk.GetChunkZ()
                ),
                gpuChunk
            );

            if (!result.second)
            {
                DeleteChunk(gpuChunk);
                return false;
            }

            return true;
        }

        void ClearGPUChunks()
        {
            for (auto& entry : gpuChunks)
            {
                DeleteChunk(entry.second);
            }

            gpuChunks.clear();
        }

        void UpdatePlayerCameraPose()
        {
            float forwardX = 0.0f;
            float forwardY = 0.0f;
            float forwardZ = 0.0f;

            player.GetForward(
                forwardX,
                forwardY,
                forwardZ
            );

            playerCameraForward = Normalize({
                forwardX,
                forwardY,
                forwardZ
            });

            player.GetFirstPersonCameraPosition(
                playerCameraPosition.x,
                playerCameraPosition.y,
                playerCameraPosition.z
            );

            if (!cinematicMode)
            {
                cameraPosition = playerCameraPosition;
                cameraForward = playerCameraForward;
                cameraTarget = Add(
                    cameraPosition,
                    cameraForward
                );
            }
        }

        void UpdateActiveCamera()
        {
            if (cinematicMode)
            {
                cameraPosition = cinematicPosition;
                cameraTarget = cinematicTarget;
                cameraForward = Normalize(
                    Subtract(cameraTarget, cameraPosition)
                );
            }
            else
            {
                cameraPosition = playerCameraPosition;
                cameraForward = playerCameraForward;
                cameraTarget = Add(
                    cameraPosition,
                    cameraForward
                );
            }
        }

        void UpdateCinematicPose(
            float deltaTime,
            bool introduction,
            float progress)
        {
            cinematicTime += std::max(deltaTime, 0.0f);

            const Vec3 landingPosition = playerCameraPosition;

            if (introduction)
            {
                const float t = SmoothStep(progress);

                const Vec3 startingPosition{
                    landingPosition.x - 145.0f,
                    landingPosition.y + 185.0f,
                    landingPosition.z + 145.0f
                };

                const Vec3 approachPosition{
                    landingPosition.x - 42.0f,
                    landingPosition.y + 36.0f,
                    landingPosition.z + 48.0f
                };

                const Vec3 finalPosition{
                    landingPosition.x - 5.0f,
                    landingPosition.y + 5.0f,
                    landingPosition.z + 7.0f
                };

                if (t < 0.75f)
                {
                    cinematicPosition = Lerp(
                        startingPosition,
                        approachPosition,
                        SmoothStep(t / 0.75f)
                    );
                }
                else
                {
                    cinematicPosition = Lerp(
                        approachPosition,
                        finalPosition,
                        SmoothStep((t - 0.75f) / 0.25f)
                    );
                }

                const Vec3 distantTarget{
                    landingPosition.x + 28.0f,
                    landingPosition.y,
                    landingPosition.z - 28.0f
                };

                cinematicTarget = Lerp(
                    distantTarget,
                    landingPosition,
                    t
                );
            }
            else
            {
                const float angle =
                    cinematicTime * 0.075f;

                const float radius = 95.0f;

                cinematicPosition = {
                    landingPosition.x +
                        std::cos(angle) * radius,
                    landingPosition.y +
                        72.0f +
                        std::sin(cinematicTime * 0.18f) * 7.0f,
                    landingPosition.z +
                        std::sin(angle) * radius
                };

                cinematicTarget = {
                    landingPosition.x,
                    landingPosition.y - 4.0f,
                    landingPosition.z
                };
            }

            UpdateActiveCamera();
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

        void UpdateStreamingCenter()
        {
            const float chunkSize =
                terrainWorld.GetChunkSize();

            if (chunkSize <= 0.0f)
            {
                return;
            }

            const Vec3 center =
                cinematicMode
                    ? cinematicTarget
                    : playerCameraPosition;

            currentChunkX = static_cast<int>(
                std::floor(center.x / chunkSize)
            );

            currentChunkZ = static_cast<int>(
                std::floor(center.z / chunkSize)
            );

            terrainWorker.SetCenter(
                currentChunkX,
                currentChunkZ
            );
        }

        void RemoveDistantChunks()
        {
            for (auto iterator = gpuChunks.begin();
                 iterator != gpuChunks.end();)
            {
                const int chunkX =
                    static_cast<std::int32_t>(
                        iterator->first >> 32
                    );

                const int chunkZ =
                    static_cast<std::int32_t>(
                        iterator->first & 0xFFFFFFFFu
                    );

                if (std::abs(chunkX - currentChunkX) >
                        UnloadRadius ||
                    std::abs(chunkZ - currentChunkZ) >
                        UnloadRadius)
                {
                    DeleteChunk(iterator->second);

                    terrainWorld.RemoveChunk(
                        chunkX,
                        chunkZ
                    );

                    iterator = gpuChunks.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }
        }

        void RequestNearbyChunks()
        {
            for (int z = -LoadRadius;
                 z <= LoadRadius;
                 ++z)
            {
                for (int x = -LoadRadius;
                     x <= LoadRadius;
                     ++x)
                {
                    const int chunkX = currentChunkX + x;
                    const int chunkZ = currentChunkZ + z;

                    const std::uint64_t key =
                        MakeChunkKey(chunkX, chunkZ);

                    if (gpuChunks.find(key) != gpuChunks.end())
                    {
                        continue;
                    }

                    if (terrainWorld.GetChunk(
                            chunkX,
                            chunkZ) != nullptr)
                    {
                        continue;
                    }

                    terrainWorker.RequestChunk(
                        chunkX,
                        chunkZ
                    );
                }
            }
        }

        void UploadCompletedChunks()
        {
            TerrainChunk chunk;
            int uploaded = 0;

            while (
                uploaded < MaxUploadsPerFrame &&
                terrainWorker.TryGetCompletedChunk(chunk)
            )
            {
                const int chunkX = chunk.GetChunkX();
                const int chunkZ = chunk.GetChunkZ();

                const int dx = chunkX - currentChunkX;
                const int dz = chunkZ - currentChunkZ;

                if (std::abs(dx) > UnloadRadius ||
                    std::abs(dz) > UnloadRadius)
                {
                    continue;
                }

                if (!terrainWorld.AddGeneratedChunk(
                        std::move(chunk)))
                {
                    continue;
                }

                const TerrainChunk* storedChunk =
                    terrainWorld.GetChunk(
                        chunkX,
                        chunkZ
                    );

                if (storedChunk == nullptr ||
                    !UploadChunk(*storedChunk))
                {
                    terrainWorld.RemoveChunk(
                        chunkX,
                        chunkZ
                    );

                    std::cerr
                        << "Failed to upload terrain chunk ("
                        << chunkX
                        << ", "
                        << chunkZ
                        << ").\n";

                    continue;
                }

                ++uploaded;
            }
        }

        void StreamTerrain()
        {
            UpdateStreamingCenter();
            RemoveDistantChunks();
            UploadCompletedChunks();
            RequestNearbyChunks();
        }

        bool StartTerrainGeneration()
        {
            terrainWorker.Start(
                terrainWorld.GetSettings(),
                terrainWorld.GetChunkSize(),
                terrainWorld.GetVertexSpacing()
            );

            if (!terrainWorker.IsRunning())
            {
                std::cerr
                    << "Failed to start terrain generation worker.\n";

                return false;
            }

            UpdateStreamingCenter();
            RequestNearbyChunks();

            std::cout
                << "Background terrain generation started. "
                << "Load radius: "
                << LoadRadius
                << ", target chunks: "
                << (LoadRadius * 2 + 1) *
                   (LoadRadius * 2 + 1)
                << ".\n";

            return true;
        }

        void StopTerrainGeneration()
        {
            terrainWorker.Stop();
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
                "assets/shaders/terrain.frag"))
        {
            std::cerr
                << "Failed to load terrain shaders.\n";

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

        m_impl->player.Initialize(
            m_impl->terrainWorld.GetGenerator()
        );

        m_impl->UpdatePlayerCameraPose();

        m_impl->cinematicPosition =
            m_impl->playerCameraPosition;

        m_impl->cinematicTarget = Add(
            m_impl->cinematicPosition,
            m_impl->playerCameraForward
        );

        if (!m_impl->StartTerrainGeneration())
        {
            m_impl->terrainShader.Shutdown();
            return false;
        }

        m_impl->initialized = true;

        std::cout
            << "Renderer initialized in first-person mode "
            << "with independent cinematic camera "
            << "and terrain streaming.\n";

        return true;
    }

    void Renderer::UpdatePlayer(
        float deltaTime,
        float forwardInput,
        float rightInput,
        bool sprint,
        bool jumpPressed)
    {
        if (!m_impl || !m_impl->initialized)
        {
            return;
        }

        PlayerInput input{};

        input.forward = forwardInput;
        input.right = rightInput;
        input.sprint = sprint;
        input.jumpPressed = jumpPressed;

        m_impl->player.Update(
            deltaTime,
            input,
            m_impl->terrainWorld.GetGenerator()
        );

        m_impl->UpdatePlayerCameraPose();
        m_impl->UpdateActiveCamera();
    }

    void Renderer::RotatePlayerCamera(
        float deltaX,
        float deltaY)
    {
        if (!m_impl || !m_impl->initialized)
        {
            return;
        }

        m_impl->player.Rotate(
            deltaX,
            deltaY
        );

        m_impl->UpdatePlayerCameraPose();
        m_impl->UpdateActiveCamera();
    }

    void Renderer::SetCinematicMode(bool enabled)
    {
        if (!m_impl || !m_impl->initialized)
        {
            return;
        }

        if (m_impl->cinematicMode == enabled)
        {
            return;
        }

        m_impl->cinematicMode = enabled;

        m_impl->UpdatePlayerCameraPose();
        m_impl->UpdateActiveCamera();
    }

    void Renderer::UpdateCinematic(
        float deltaTime,
        bool introduction,
        float progress)
    {
        if (!m_impl ||
            !m_impl->initialized ||
            !m_impl->cinematicMode)
        {
            return;
        }

        m_impl->UpdateCinematicPose(
            deltaTime,
            introduction,
            std::clamp(progress, 0.0f, 1.0f)
        );
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

        m_impl->StreamTerrain();
        m_impl->UpdateActiveCamera();

        glViewport(
            0,
            0,
            width,
            height
        );

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        const Mat4 projection = Perspective(
            70.0f * DegreesToRadians,
            static_cast<float>(width) /
                static_cast<float>(height),
            0.1f,
            2000.0f
        );

        const Mat4 view = LookAt(
            m_impl->cameraPosition,
            m_impl->cameraTarget,
            {0.0f, 1.0f, 0.0f}
        );

        const Mat4 model = Identity();

        const Mat4 viewProjection =
            MultiplyMatrices(projection, view);

        const Mat4 mvp =
            MultiplyMatrices(viewProjection, model);

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
            const Impl::GPUChunk& chunk =
                entry.second;

            m_impl->BindVertexArray(
                chunk.vertexArray
            );

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

        m_impl->StopTerrainGeneration();
        m_impl->ClearGPUChunks();
        m_impl->terrainWorld.Clear();
        m_impl->terrainShader.Shutdown();

        m_impl->initialized = false;
        m_impl->cinematicMode = false;
        m_impl->cinematicTime = 0.0f;
    }
}
