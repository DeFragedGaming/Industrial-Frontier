
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
#include <unordered_set>
#include <vector>
#include <utility>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float DegreesToRadians = Pi / 180.0f;

        constexpr int LoadRadius = 5;
        constexpr int UnloadRadius = 8;
        constexpr int MaxUploadsPerFrame = 4;

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
        std::unique_ptr<VoxelTerrain> visibleCollisionSnapshot;

        std::unordered_map<std::uint64_t, GPUChunk> gpuChunks;
        std::unordered_set<std::uint64_t> emptyChunks;
        std::unordered_set<std::uint64_t> pendingEditKeys;
        std::unordered_map<std::uint64_t, GPUChunk> pendingEditReady;

        Vec3 cameraPosition{};
        Vec3 cameraForward{0.0f, 0.0f, -1.0f};
        Vec3 cameraTarget{};

        Vec3 playerCameraPosition{};
        Vec3 playerCameraForward{0.0f, 0.0f, -1.0f};
        Vec3 lastSafePlayerCameraPosition{};
        Vec3 lastSafePlayerFeetPosition{};
        Vec3 checkedFeetPosition{};
        Vec3 checkedCameraPosition{};
        std::uint64_t checkedTerrainRevision = 0;
        bool hasSafePlayerCameraPosition = false;
        bool hasSafePlayerFeetPosition = false;
        bool hasCheckedCameraPosition = false;

        Vec3 cinematicPosition{};
        Vec3 cinematicTarget{};

        float cinematicTime = 0.0f;

        int currentChunkX = 0;
        int currentChunkZ = 0;
        int prefetchDirectionX = 0;
        int prefetchDirectionZ = 0;
        Vec3 previousStreamingPosition{};
        bool hasPreviousStreamingPosition = false;

        bool cinematicMode = false;
        bool initialized = false;
        std::uint64_t targetLastUpdate = 0;
        bool cachedTargetHit = false;
        TerrainMaterial cachedTargetMaterial = TerrainMaterial::None;
        float cachedTargetDistance = 0.0f;

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

        bool BuildGPUChunk(const TerrainChunk& chunk, GPUChunk& gpuChunk)
        {
            const auto& vertices = chunk.GetVertices();
            const auto& indices = chunk.GetIndices();

            if (vertices.empty() || indices.empty())
            {
                return false;
            }

            gpuChunk = {};

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

            return true;
        }

        bool InstallChunk(const TerrainChunk& chunk)
        {
            const std::uint64_t key = MakeChunkKey(
                chunk.GetChunkX(), chunk.GetChunkZ());

            GPUChunk replacement{};
            const bool hasGeometry = !chunk.GetIndices().empty();
            if (hasGeometry && !BuildGPUChunk(chunk, replacement))
            {
                return false;
            }

            auto existing = gpuChunks.find(key);
            if (existing != gpuChunks.end())
            {
                DeleteChunk(existing->second);
                gpuChunks.erase(existing);
            }

            if (hasGeometry)
            {
                gpuChunks.emplace(key, replacement);
                emptyChunks.erase(key);
            }
            else
            {
                emptyChunks.insert(key);
            }

            return true;
        }

        bool CommitEditedChunks()
        {
            if (pendingEditKeys.empty())
            {
                if (visibleCollisionSnapshot)
                {
                    visibleCollisionSnapshot.reset();
                    hasCheckedCameraPosition = false;
                }
                return false;
            }

            if (pendingEditReady.size() != pendingEditKeys.size())
            {
                return false;
            }

            for (const std::uint64_t key : pendingEditKeys)
            {
                auto existing = gpuChunks.find(key);
                if (existing != gpuChunks.end())
                {
                    DeleteChunk(existing->second);
                    gpuChunks.erase(existing);
                }

                auto prepared = pendingEditReady.find(key);
                if (prepared != pendingEditReady.end() &&
                    prepared->second.indexCount > 0)
                {
                    gpuChunks.emplace(key, prepared->second);
                    emptyChunks.erase(key);
                }
                else
                {
                    emptyChunks.insert(key);
                }
            }

            pendingEditKeys.clear();
            pendingEditReady.clear();
            visibleCollisionSnapshot.reset();
            hasCheckedCameraPosition = false;
            return true;
        }

        void ClearGPUChunks()
        {
            for (auto& entry : gpuChunks)
            {
                DeleteChunk(entry.second);
            }

            gpuChunks.clear();
            for (auto& entry : pendingEditReady)
            {
                DeleteChunk(entry.second);
            }
            emptyChunks.clear();
            pendingEditKeys.clear();
            pendingEditReady.clear();
            visibleCollisionSnapshot.reset();
            hasCheckedCameraPosition = false;
            hasPreviousStreamingPosition = false;
            prefetchDirectionX = 0;
            prefetchDirectionZ = 0;
        }

        void UpdatePlayerCameraPose()
        {
            float forwardX = 0.0f;
            float forwardY = 0.0f;
            float forwardZ = 0.0f;

            player.GetForward(forwardX, forwardY, forwardZ);
            playerCameraForward = Normalize({
                forwardX,
                forwardY,
                forwardZ
            });

            const Vec3 feet{
                player.GetX(),
                player.GetY(),
                player.GetZ()
            };

            const auto voxelTerrain = terrainWorker.GetVoxelTerrain();
            const VoxelTerrain* visibleTerrain = visibleCollisionSnapshot
                ? visibleCollisionSnapshot.get() : voxelTerrain.get();

            if (visibleTerrain)
            {
                const std::uint64_t revision = visibleTerrain->GetRevision();
                const Vec3 offset = Subtract(feet, checkedFeetPosition);
                const bool mustCheck = !hasCheckedCameraPosition ||
                    Dot(offset, offset) > 0.00000001f ||
                    revision != checkedTerrainRevision;

                if (!mustCheck)
                {
                    playerCameraPosition = checkedCameraPosition;
                }
                else
                {
                    Vec3 safe{};
                    bool valid = player.GetCollisionSafeCameraPosition(
                        *visibleTerrain, safe.x, safe.y, safe.z);

                    if (!valid && hasSafePlayerFeetPosition)
                    {
                        const Vec3 previousFeet = lastSafePlayerFeetPosition;
                        player.SetPosition(
                            previousFeet.x,
                            previousFeet.y,
                            previousFeet.z);

                        valid = player.GetCollisionSafeCameraPosition(
                            *visibleTerrain, safe.x, safe.y, safe.z);
                    }

                    if (!valid)
                    {
                        const float baseX = player.GetX();
                        const float baseZ = player.GetZ();
                        const float surfaceY =
                            visibleTerrain->GetGenerator().GetHeight(baseX, baseZ);
                        const float minimumY = std::max(
                            player.GetY(), surfaceY + 0.2f);

                        for (float rise : {
                                 0.0f, 0.25f, 0.5f, 1.0f,
                                 2.0f, 3.5f, 5.0f, 8.0f,
                                 12.0f, 20.0f, 32.0f})
                        {
                            player.SetPosition(
                                baseX, minimumY + rise, baseZ);
                            if (player.GetCollisionSafeCameraPosition(
                                    *visibleTerrain, safe.x, safe.y, safe.z))
                            {
                                valid = true;
                                break;
                            }
                        }
                    }

                    if (valid)
                    {
                        playerCameraPosition = safe;
                        lastSafePlayerCameraPosition = safe;
                        hasSafePlayerCameraPosition = true;
                        lastSafePlayerFeetPosition = {
                            player.GetX(), player.GetY(), player.GetZ()};
                        hasSafePlayerFeetPosition = true;
                        checkedFeetPosition = lastSafePlayerFeetPosition;
                        checkedCameraPosition = safe;
                        checkedTerrainRevision = revision;
                        hasCheckedCameraPosition = true;
                    }
                    else
                    {
                        hasCheckedCameraPosition = false;
                        if (hasSafePlayerCameraPosition)
                        {
                            playerCameraPosition = lastSafePlayerCameraPosition;
                        }
                        else
                        {
                            player.GetFirstPersonCameraPosition(
                                playerCameraPosition.x,
                                playerCameraPosition.y,
                                playerCameraPosition.z);
                        }
                    }
                }
            }
            else
            {
                player.GetFirstPersonCameraPosition(
                    playerCameraPosition.x,
                    playerCameraPosition.y,
                    playerCameraPosition.z);
            }

            if (!cinematicMode)
            {
                cameraPosition = playerCameraPosition;
                cameraForward = playerCameraForward;
                cameraTarget = Add(cameraPosition, cameraForward);
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
            const float chunkSize = terrainWorld.GetChunkSize();
            if (chunkSize <= 0.0f)
            {
                return;
            }

            const Vec3 center = cinematicMode
                ? cinematicTarget
                : playerCameraPosition;

            if (hasPreviousStreamingPosition)
            {
                const float deltaX = center.x - previousStreamingPosition.x;
                const float deltaZ = center.z - previousStreamingPosition.z;
                const float planarDistanceSquared = deltaX * deltaX + deltaZ * deltaZ;

                if (planarDistanceSquared > 0.0001f &&
                    planarDistanceSquared < chunkSize * chunkSize * 4.0f)
                {
                    prefetchDirectionX = deltaX > 0.01f ? 1 :
                        (deltaX < -0.01f ? -1 : 0);
                    prefetchDirectionZ = deltaZ > 0.01f ? 1 :
                        (deltaZ < -0.01f ? -1 : 0);
                }
                else if (planarDistanceSquared >= chunkSize * chunkSize * 4.0f)
                {
                    prefetchDirectionX = 0;
                    prefetchDirectionZ = 0;
                }
            }

            previousStreamingPosition = center;
            hasPreviousStreamingPosition = true;

            currentChunkX = static_cast<int>(std::floor(center.x / chunkSize));
            currentChunkZ = static_cast<int>(std::floor(center.z / chunkSize));
            terrainWorker.SetCenter(currentChunkX, currentChunkZ);
        }

        bool IsOutsideUnloadRadius(std::uint64_t key) const
        {
            const int chunkX = static_cast<std::int32_t>(key >> 32);
            const int chunkZ = static_cast<std::int32_t>(key & 0xFFFFFFFFu);
            return std::abs(chunkX - currentChunkX) > UnloadRadius ||
                std::abs(chunkZ - currentChunkZ) > UnloadRadius;
        }

        void RemoveDistantChunks()
        {
            for (auto iterator = gpuChunks.begin(); iterator != gpuChunks.end();)
            {
                if (IsOutsideUnloadRadius(iterator->first))
                {
                    DeleteChunk(iterator->second);
                    iterator = gpuChunks.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }

            for (auto iterator = emptyChunks.begin(); iterator != emptyChunks.end();)
            {
                if (IsOutsideUnloadRadius(*iterator))
                {
                    iterator = emptyChunks.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }

            for (auto iterator = pendingEditKeys.begin();
                 iterator != pendingEditKeys.end();)
            {
                if (IsOutsideUnloadRadius(*iterator))
                {
                    auto prepared = pendingEditReady.find(*iterator);
                    if (prepared != pendingEditReady.end())
                    {
                        DeleteChunk(prepared->second);
                        pendingEditReady.erase(prepared);
                    }
                    iterator = pendingEditKeys.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }
        }

        void RequestRegion(int centerX, int centerZ)
        {
            for (int z = -LoadRadius; z <= LoadRadius; ++z)
            {
                for (int x = -LoadRadius; x <= LoadRadius; ++x)
                {
                    const int chunkX = centerX + x;
                    const int chunkZ = centerZ + z;
                    const std::uint64_t key = MakeChunkKey(chunkX, chunkZ);

                    if (gpuChunks.find(key) != gpuChunks.end() ||
                        emptyChunks.find(key) != emptyChunks.end() ||
                        pendingEditKeys.find(key) != pendingEditKeys.end())
                    {
                        continue;
                    }

                    terrainWorker.RequestChunk(chunkX, chunkZ);
                }
            }
        }

        void RequestNearbyChunks()
        {
            RequestRegion(currentChunkX, currentChunkZ);

            if (!cinematicMode &&
                (prefetchDirectionX != 0 || prefetchDirectionZ != 0))
            {
                constexpr int LookAheadChunks = 2;
                RequestRegion(
                    currentChunkX + prefetchDirectionX * LookAheadChunks,
                    currentChunkZ + prefetchDirectionZ * LookAheadChunks);
            }
        }

        void UploadCompletedChunks()
        {
            TerrainChunk chunk;
            int consumed = 0;

            while (consumed < MaxUploadsPerFrame &&
                   terrainWorker.TryGetCompletedChunk(chunk))
            {
                ++consumed;

                const std::uint64_t key = MakeChunkKey(
                    chunk.GetChunkX(), chunk.GetChunkZ());

                if (IsOutsideUnloadRadius(key))
                {
                    continue;
                }

                if (pendingEditKeys.find(key) != pendingEditKeys.end())
                {
                    GPUChunk prepared{};
                    if (chunk.GetIndices().empty() ||
                        BuildGPUChunk(chunk, prepared))
                    {
                        auto oldPrepared = pendingEditReady.find(key);
                        if (oldPrepared != pendingEditReady.end())
                        {
                            DeleteChunk(oldPrepared->second);
                        }
                        pendingEditReady.insert_or_assign(key, prepared);
                    }
                    else
                    {
                        std::cerr << "Failed to prepare edited chunk ("
                            << chunk.GetChunkX() << ", " << chunk.GetChunkZ()
                            << "). Retaining the previous mesh.\n";
                        terrainWorker.RegenerateChunk(
                            chunk.GetChunkX(), chunk.GetChunkZ());
                    }
                    continue;
                }

                if (!InstallChunk(chunk))
                {
                    std::cerr << "Failed to upload terrain chunk ("
                        << chunk.GetChunkX() << ", " << chunk.GetChunkZ()
                        << "). The old mesh was retained.\n";
                }
            }

            CommitEditedChunks();
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

        const std::shared_ptr<const VoxelTerrain> voxelTerrain =
            m_impl->terrainWorker.GetVoxelTerrain();
        const VoxelTerrain* collisionTerrain = m_impl->visibleCollisionSnapshot
            ? m_impl->visibleCollisionSnapshot.get() : voxelTerrain.get();

        if (collisionTerrain)
        {
            m_impl->player.Update(
                deltaTime,
                input,
                *collisionTerrain
            );
        }

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

    bool Renderer::DigTerrain(float radius, float reach)
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode ||
            radius <= 0.0f || reach <= 0.0f)
            return false;

        const Vec3 origin = m_impl->playerCameraPosition;
        const Vec3 direction = Normalize(m_impl->playerCameraForward);
        const float step = 0.20f;
        float previousDensity = m_impl->terrainWorker.SampleDensity(
            origin.x, origin.y, origin.z);

        for (float distance = step; distance <= reach; distance += step)
        {
            const Vec3 point = Add(origin, {
                direction.x * distance,
                direction.y * distance,
                direction.z * distance
            });
            const float density = m_impl->terrainWorker.SampleDensity(
                point.x, point.y, point.z);

            if (previousDensity <= 0.0f && density > 0.0f)
            {
                if (!m_impl->visibleCollisionSnapshot)
                {
                    const auto currentTerrain =
                        m_impl->terrainWorker.GetVoxelTerrain();
                    if (!currentTerrain)
                    {
                        return false;
                    }
                    m_impl->visibleCollisionSnapshot =
                        std::make_unique<VoxelTerrain>(
                            m_impl->terrainWorld.GetSettings(),
                            m_impl->terrainWorld.GetChunkSize(),
                            m_impl->terrainWorld.GetVertexSpacing());
                    m_impl->visibleCollisionSnapshot->SetEdits(
                        currentTerrain->GetEdits());
                    m_impl->hasCheckedCameraPosition = false;
                }

                if (!m_impl->terrainWorker.DigSphere(
                        point.x, point.y, point.z, radius))
                {
                    if (m_impl->pendingEditKeys.empty())
                    {
                        m_impl->visibleCollisionSnapshot.reset();
                        m_impl->hasCheckedCameraPosition = false;
                    }
                    return false;
                }

                const float chunkSize = m_impl->terrainWorld.GetChunkSize();
                const float extent = radius + 2.0f;
                const int minX = static_cast<int>(std::floor(
                    (point.x - extent) / chunkSize));
                const int maxX = static_cast<int>(std::floor(
                    (point.x + extent) / chunkSize));
                const int minZ = static_cast<int>(std::floor(
                    (point.z - extent) / chunkSize));
                const int maxZ = static_cast<int>(std::floor(
                    (point.z + extent) / chunkSize));

                for (int z = minZ; z <= maxZ; ++z)
                {
                    for (int x = minX; x <= maxX; ++x)
                    {
                        if (std::abs(x - m_impl->currentChunkX) > UnloadRadius ||
                            std::abs(z - m_impl->currentChunkZ) > UnloadRadius)
                        {
                            continue;
                        }
                        const std::uint64_t key = m_impl->MakeChunkKey(x, z);
                        m_impl->pendingEditKeys.insert(key);
                        auto prepared = m_impl->pendingEditReady.find(key);
                        if (prepared != m_impl->pendingEditReady.end())
                        {
                            m_impl->DeleteChunk(prepared->second);
                            m_impl->pendingEditReady.erase(prepared);
                        }
                        m_impl->terrainWorker.RegenerateChunk(x, z);
                    }
                }
                if (m_impl->pendingEditKeys.empty())
                {
                    m_impl->visibleCollisionSnapshot.reset();
                    m_impl->hasCheckedCameraPosition = false;
                }
                return true;
            }
            previousDensity = density;
        }
        return false;
    }

    bool Renderer::GetHUDWorldState(HUDWorldState& state) const
    {
        if (!m_impl || !m_impl->initialized)
        {
            return false;
        }

        state.x = m_impl->player.GetX();
        state.y = m_impl->player.GetY();
        state.z = m_impl->player.GetZ();
        state.grounded = m_impl->player.IsGrounded();
        state.terrainUpdating = !m_impl->pendingEditKeys.empty();
        state.loadedChunks = m_impl->gpuChunks.size();

        const std::uint64_t now = SDL_GetTicks();
        if (m_impl->targetLastUpdate == 0 ||
            now - m_impl->targetLastUpdate >= 90)
        {
            m_impl->targetLastUpdate = now == 0 ? 1 : now;
            m_impl->cachedTargetHit = false;
            m_impl->cachedTargetMaterial = TerrainMaterial::None;
            m_impl->cachedTargetDistance = 0.0f;
            const auto current = m_impl->terrainWorker.GetVoxelTerrain();
            const VoxelTerrain* terrain = m_impl->visibleCollisionSnapshot
                ? m_impl->visibleCollisionSnapshot.get() : current.get();
            if (terrain && !m_impl->cinematicMode)
            {
                const Vec3 origin = m_impl->playerCameraPosition;
                const Vec3 direction = Normalize(m_impl->playerCameraForward);
                float previousDensity = terrain->SampleDensity(
                    origin.x, origin.y, origin.z);
                constexpr float reach = 8.0f;
                constexpr float step = 0.30f;
                for (float distance = step; distance <= reach; distance += step)
                {
                    const Vec3 point = Add(origin, Multiply(direction, distance));
                    const float density = terrain->SampleDensity(
                        point.x, point.y, point.z);
                    if (previousDensity <= 0.0f && density > 0.0f)
                    {
                        float nearDistance = distance - step;
                        float farDistance = distance;
                        for (int i = 0; i < 5; ++i)
                        {
                            const float midpoint = 0.5f * (nearDistance + farDistance);
                            const Vec3 mid = Add(origin, Multiply(direction, midpoint));
                            if (terrain->SampleDensity(mid.x, mid.y, mid.z) > 0.0f)
                            {
                                farDistance = midpoint;
                            }
                            else
                            {
                                nearDistance = midpoint;
                            }
                        }
                        const Vec3 hit = Add(origin, Multiply(direction, farDistance));
                        constexpr float e = 0.22f;
                        const float dx = terrain->SampleDensity(
                            hit.x - e, hit.y, hit.z) -
                            terrain->SampleDensity(hit.x + e, hit.y, hit.z);
                        const float dy = terrain->SampleDensity(
                            hit.x, hit.y - e, hit.z) -
                            terrain->SampleDensity(hit.x, hit.y + e, hit.z);
                        const float dz = terrain->SampleDensity(
                            hit.x, hit.y, hit.z - e) -
                            terrain->SampleDensity(hit.x, hit.y, hit.z + e);
                        const Vec3 normal = Normalize({dx, dy, dz});
                        m_impl->cachedTargetHit = true;
                        m_impl->cachedTargetDistance = farDistance;
                        m_impl->cachedTargetMaterial = terrain->GetMaterialAt(
                            hit.x, hit.y, hit.z, normal.y);
                        break;
                    }
                    previousDensity = density;
                }
            }
        }

        state.hasTarget = m_impl->cachedTargetHit;
        state.targetDistance = m_impl->cachedTargetDistance;
        switch (m_impl->cachedTargetMaterial)
        {
            case TerrainMaterial::Grass: state.targetMaterial = "GRASS"; break;
            case TerrainMaterial::DirtClay: state.targetMaterial = "DIRT / CLAY"; break;
            case TerrainMaterial::Stone: state.targetMaterial = "STONE"; break;
            case TerrainMaterial::Snow: state.targetMaterial = "SNOW"; break;
            default: state.targetMaterial = "NONE"; break;
        }
        return true;
    }

    bool Renderer::CaptureWorld(WorldSave& save) const
    {
        if (!m_impl || !m_impl->initialized)
            return false;
        const auto voxel = m_impl->terrainWorker.GetVoxelTerrain();
        if (!voxel)
            return false;
        save.seed = m_impl->terrainWorld.GetSettings().seed;
        save.x = m_impl->player.GetX();
        save.y = m_impl->player.GetY();
        save.z = m_impl->player.GetZ();
        save.yaw = m_impl->player.GetYaw();
        save.pitch = m_impl->player.GetPitch();
        save.edits = voxel->GetEdits();
        return true;
    }

    bool Renderer::StartFreshWorld()
    {
        if (!m_impl || !m_impl->initialized)
            return false;
        m_impl->terrainWorker.Stop();
        m_impl->ClearGPUChunks();
        m_impl->hasSafePlayerCameraPosition = false;
        m_impl->hasSafePlayerFeetPosition = false;
        m_impl->hasCheckedCameraPosition = false;
        m_impl->targetLastUpdate = 0;
        m_impl->terrainWorld.Clear();
        m_impl->player.Initialize(m_impl->terrainWorld.GetGenerator());
        m_impl->UpdatePlayerCameraPose();
        return m_impl->StartTerrainGeneration();
    }

    bool Renderer::RestoreWorld(const WorldSave& save)
    {
        if (!m_impl || !m_impl->initialized ||
            save.seed != m_impl->terrainWorld.GetSettings().seed)
            return false;
        m_impl->terrainWorker.Stop();
        m_impl->ClearGPUChunks();
        m_impl->hasSafePlayerCameraPosition = false;
        m_impl->hasSafePlayerFeetPosition = false;
        m_impl->hasCheckedCameraPosition = false;
        m_impl->targetLastUpdate = 0;
        m_impl->terrainWorld.Clear();
        m_impl->terrainWorker.Start(m_impl->terrainWorld.GetSettings(),
            m_impl->terrainWorld.GetChunkSize(),
            m_impl->terrainWorld.GetVertexSpacing());
        if (!m_impl->terrainWorker.IsRunning() ||
            !m_impl->terrainWorker.ReplaceEdits(save.edits))
            return false;
        m_impl->player.Initialize(m_impl->terrainWorld.GetGenerator());
        m_impl->player.SetPosition(save.x, save.y, save.z);
        m_impl->player.SetViewRotation(save.yaw, save.pitch);
        m_impl->UpdatePlayerCameraPose();
        m_impl->UpdateActiveCamera();
        m_impl->UpdateStreamingCenter();
        m_impl->RequestNearbyChunks();
        return true;
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
            m_impl->cinematicMode ? 0.10f : 0.075f,
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

        glDisable(GL_CULL_FACE);

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
        glEnable(GL_CULL_FACE);
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
