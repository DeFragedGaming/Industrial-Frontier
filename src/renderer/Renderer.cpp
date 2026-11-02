
#include "industrial_frontier/renderer/Renderer.hpp"
#include "industrial_frontier/world/TerrainWorld.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float DegreesToRadians = Pi / 180.0f;

        using DrawElementsProc = void (APIENTRY*)(
            GLenum,
            GLsizei,
            GLenum,
            const void*
        );

        struct Vec3
        {
            float x;
            float y;
            float z;
        };

        using Mat4 = std::array<float, 16>;

        Vec3 Add(Vec3 a, Vec3 b)
        {
            return { a.x + b.x, a.y + b.y, a.z + b.z };
        }

        Vec3 Subtract(Vec3 a, Vec3 b)
        {
            return { a.x - b.x, a.y - b.y, a.z - b.z };
        }

        Vec3 Scale(Vec3 v, float amount)
        {
            return {
                v.x * amount,
                v.y * amount,
                v.z * amount
            };
        }

        float Dot(Vec3 a, Vec3 b)
        {
            return a.x * b.x + a.y * b.y + a.z * b.z;
        }

        Vec3 Cross(Vec3 a, Vec3 b)
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        Vec3 Normalize(Vec3 v)
        {
            const float length = std::sqrt(Dot(v, v));

            if (length <= 0.00001f)
            {
                return { 0.0f, 0.0f, 0.0f };
            }

            return Scale(v, 1.0f / length);
        }

        Mat4 Multiply(const Mat4& a, const Mat4& b)
        {
            Mat4 result{};

            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    for (int k = 0; k < 4; ++k)
                    {
                        result[column * 4 + row] +=
                            a[k * 4 + row] * b[column * 4 + k];
                    }
                }
            }

            return result;
        }

        Mat4 Perspective(
            float fov,
            float aspect,
            float nearPlane,
            float farPlane
        )
        {
            Mat4 result{};

            const float tanHalfFov = std::tan(fov * 0.5f);

            result[0] = 1.0f / (aspect * tanHalfFov);
            result[5] = 1.0f / tanHalfFov;
            result[10] = -(farPlane + nearPlane) /
                         (farPlane - nearPlane);
            result[11] = -1.0f;
            result[14] = -(2.0f * farPlane * nearPlane) /
                         (farPlane - nearPlane);

            return result;
        }

        Mat4 LookAt(Vec3 eye, Vec3 center, Vec3 up)
        {
            const Vec3 forward = Normalize(Subtract(center, eye));
            const Vec3 side = Normalize(Cross(forward, up));
            const Vec3 correctedUp = Cross(side, forward);

            Mat4 result{};

            result[0] = side.x;
            result[1] = correctedUp.x;
            result[2] = -forward.x;

            result[4] = side.y;
            result[5] = correctedUp.y;
            result[6] = -forward.y;

            result[8] = side.z;
            result[9] = correctedUp.z;
            result[10] = -forward.z;

            result[12] = -Dot(side, eye);
            result[13] = -Dot(correctedUp, eye);
            result[14] = Dot(forward, eye);
            result[15] = 1.0f;

            return result;
        }
    }

    struct Renderer::Impl
    {
        PFNGLCREATESHADERPROC CreateShader = nullptr;
        PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
        PFNGLCOMPILESHADERPROC CompileShaderFn = nullptr;
        PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
        PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;
        PFNGLDELETESHADERPROC DeleteShader = nullptr;

        PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
        PFNGLATTACHSHADERPROC AttachShader = nullptr;
        PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
        PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
        PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
        PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
        PFNGLUSEPROGRAMPROC UseProgram = nullptr;

        PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
        PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv = nullptr;

        PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr;
        PFNGLBINDVERTEXARRAYPROC BindVertexArray = nullptr;
        PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays = nullptr;

        PFNGLGENBUFFERSPROC GenBuffers = nullptr;
        PFNGLBINDBUFFERPROC BindBuffer = nullptr;
        PFNGLBUFFERDATAPROC BufferData = nullptr;
        PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr;

        PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;
        PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr;

        DrawElementsProc DrawElements = nullptr;

        struct GpuTerrainChunk
        {
            GLuint vao = 0;
            GLuint vbo = 0;
            GLuint ebo = 0;
            GLsizei indexCount = 0;
        };

        GLuint shaderProgram = 0;
        GLint mvpLocation = -1;

        TerrainWorld terrainWorld;
        std::vector<GpuTerrainChunk> gpuChunks;

        Vec3 cameraPosition{ 0.0f, 25.0f, 70.0f };

        float cameraYaw = 0.0f;
        float cameraPitch = -0.20f;

        float movementSpeed = 5.0f;
        float sprintSpeed = 12.0f;
        float mouseSensitivity = 0.0025f;

        bool LoadFunctions()
        {
#define LOAD_GL_FUNCTION(member, procType, functionName) \
            member = reinterpret_cast<procType>( \
                SDL_GL_GetProcAddress(functionName)); \
            if (member == nullptr) \
            { \
                std::cerr << "Failed to load OpenGL function: " \
                          << functionName << '\n'; \
                return false; \
            }

            LOAD_GL_FUNCTION(CreateShader, PFNGLCREATESHADERPROC, "glCreateShader")
            LOAD_GL_FUNCTION(ShaderSource, PFNGLSHADERSOURCEPROC, "glShaderSource")
            LOAD_GL_FUNCTION(CompileShaderFn, PFNGLCOMPILESHADERPROC, "glCompileShader")
            LOAD_GL_FUNCTION(GetShaderiv, PFNGLGETSHADERIVPROC, "glGetShaderiv")
            LOAD_GL_FUNCTION(GetShaderInfoLog, PFNGLGETSHADERINFOLOGPROC, "glGetShaderInfoLog")
            LOAD_GL_FUNCTION(DeleteShader, PFNGLDELETESHADERPROC, "glDeleteShader")

            LOAD_GL_FUNCTION(CreateProgram, PFNGLCREATEPROGRAMPROC, "glCreateProgram")
            LOAD_GL_FUNCTION(AttachShader, PFNGLATTACHSHADERPROC, "glAttachShader")
            LOAD_GL_FUNCTION(LinkProgram, PFNGLLINKPROGRAMPROC, "glLinkProgram")
            LOAD_GL_FUNCTION(GetProgramiv, PFNGLGETPROGRAMIVPROC, "glGetProgramiv")
            LOAD_GL_FUNCTION(GetProgramInfoLog, PFNGLGETPROGRAMINFOLOGPROC, "glGetProgramInfoLog")
            LOAD_GL_FUNCTION(DeleteProgram, PFNGLDELETEPROGRAMPROC, "glDeleteProgram")
            LOAD_GL_FUNCTION(UseProgram, PFNGLUSEPROGRAMPROC, "glUseProgram")

            LOAD_GL_FUNCTION(GetUniformLocation, PFNGLGETUNIFORMLOCATIONPROC, "glGetUniformLocation")
            LOAD_GL_FUNCTION(UniformMatrix4fv, PFNGLUNIFORMMATRIX4FVPROC, "glUniformMatrix4fv")

            LOAD_GL_FUNCTION(GenVertexArrays, PFNGLGENVERTEXARRAYSPROC, "glGenVertexArrays")
            LOAD_GL_FUNCTION(BindVertexArray, PFNGLBINDVERTEXARRAYPROC, "glBindVertexArray")
            LOAD_GL_FUNCTION(DeleteVertexArrays, PFNGLDELETEVERTEXARRAYSPROC, "glDeleteVertexArrays")

            LOAD_GL_FUNCTION(GenBuffers, PFNGLGENBUFFERSPROC, "glGenBuffers")
            LOAD_GL_FUNCTION(BindBuffer, PFNGLBINDBUFFERPROC, "glBindBuffer")
            LOAD_GL_FUNCTION(BufferData, PFNGLBUFFERDATAPROC, "glBufferData")
            LOAD_GL_FUNCTION(DeleteBuffers, PFNGLDELETEBUFFERSPROC, "glDeleteBuffers")

            LOAD_GL_FUNCTION(EnableVertexAttribArray, PFNGLENABLEVERTEXATTRIBARRAYPROC, "glEnableVertexAttribArray")
            LOAD_GL_FUNCTION(VertexAttribPointer, PFNGLVERTEXATTRIBPOINTERPROC, "glVertexAttribPointer")
            LOAD_GL_FUNCTION(DrawElements, DrawElementsProc, "glDrawElements")

#undef LOAD_GL_FUNCTION

            return true;
        }

        GLuint CompileShaderObject(GLenum type, const char* source)
        {
            const GLuint shader = CreateShader(type);

            if (shader == 0)
            {
                std::cerr << "Could not create shader object.\n";
                return 0;
            }

            ShaderSource(shader, 1, &source, nullptr);
            CompileShaderFn(shader);

            GLint success = GL_FALSE;
            GetShaderiv(shader, GL_COMPILE_STATUS, &success);

            if (success != GL_TRUE)
            {
                GLint logLength = 0;
                GetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

                std::vector<char> log(
                    static_cast<std::size_t>(std::max(logLength, 1)) + 1
                );

                GetShaderInfoLog(
                    shader,
                    static_cast<GLsizei>(log.size()),
                    nullptr,
                    log.data()
                );

                std::cerr << "Shader compilation failed:\n"
                          << log.data() << '\n';

                DeleteShader(shader);
                return 0;
            }

            return shader;
        }

        bool CreateShaders()
        {
            constexpr const char* vertexSource = R"(
                #version 330 core

                layout (location = 0) in vec3 aPosition;
                layout (location = 1) in vec3 aNormal;
                layout (location = 2) in vec3 aColor;

                uniform mat4 uMVP;

                out vec3 vertexColor;
                out vec3 vertexNormal;

                void main()
                {
                    gl_Position = uMVP * vec4(aPosition, 1.0);
                    vertexColor = aColor;
                    vertexNormal = aNormal;
                }
            )";

            constexpr const char* fragmentSource = R"(
                #version 330 core

                in vec3 vertexColor;
                in vec3 vertexNormal;

                out vec4 FragColor;

                void main()
                {
                    vec3 normal = normalize(vertexNormal);
                    vec3 lightDirection = normalize(vec3(-0.4, 1.0, -0.3));

                    float diffuse = max(dot(normal, lightDirection), 0.0);
                    float lighting = 0.38 + diffuse * 0.62;

                    FragColor = vec4(vertexColor * lighting, 1.0);
                }
            )";

            const GLuint vertexShader =
                CompileShaderObject(GL_VERTEX_SHADER, vertexSource);

            if (vertexShader == 0)
            {
                return false;
            }

            const GLuint fragmentShader =
                CompileShaderObject(GL_FRAGMENT_SHADER, fragmentSource);

            if (fragmentShader == 0)
            {
                DeleteShader(vertexShader);
                return false;
            }

            shaderProgram = CreateProgram();

            if (shaderProgram == 0)
            {
                DeleteShader(vertexShader);
                DeleteShader(fragmentShader);
                return false;
            }

            AttachShader(shaderProgram, vertexShader);
            AttachShader(shaderProgram, fragmentShader);
            LinkProgram(shaderProgram);

            DeleteShader(vertexShader);
            DeleteShader(fragmentShader);

            GLint success = GL_FALSE;
            GetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

            if (success != GL_TRUE)
            {
                GLint logLength = 0;
                GetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);

                std::vector<char> log(
                    static_cast<std::size_t>(std::max(logLength, 1)) + 1
                );

                GetProgramInfoLog(
                    shaderProgram,
                    static_cast<GLsizei>(log.size()),
                    nullptr,
                    log.data()
                );

                std::cerr << "Shader program linking failed:\n"
                          << log.data() << '\n';

                DeleteProgram(shaderProgram);
                shaderProgram = 0;

                return false;
            }

            mvpLocation =
                GetUniformLocation(shaderProgram, "uMVP");

            return mvpLocation >= 0;
        }

        void DeleteGpuChunk(GpuTerrainChunk& chunk)
        {
            if (chunk.ebo != 0 && DeleteBuffers != nullptr)
            {
                DeleteBuffers(1, &chunk.ebo);
                chunk.ebo = 0;
            }

            if (chunk.vbo != 0 && DeleteBuffers != nullptr)
            {
                DeleteBuffers(1, &chunk.vbo);
                chunk.vbo = 0;
            }

            if (chunk.vao != 0 && DeleteVertexArrays != nullptr)
            {
                DeleteVertexArrays(1, &chunk.vao);
                chunk.vao = 0;
            }

            chunk.indexCount = 0;
        }

        bool UploadTerrainChunk(const TerrainChunk& chunk)
        {
            const auto& vertices = chunk.GetVertices();
            const auto& indices = chunk.GetIndices();

            if (vertices.empty() || indices.empty())
            {
                return false;
            }

            if (indices.size() >
                static_cast<std::size_t>(
                    std::numeric_limits<GLsizei>::max()
                ))
            {
                std::cerr << "Terrain chunk has too many indices.\n";
                return false;
            }

            if (vertices.size() >
                static_cast<std::size_t>(
                    std::numeric_limits<GLsizeiptr>::max() /
                    static_cast<GLsizeiptr>(sizeof(TerrainVertex))
                ))
            {
                std::cerr << "Terrain chunk vertex buffer is too large.\n";
                return false;
            }

            if (indices.size() >
                static_cast<std::size_t>(
                    std::numeric_limits<GLsizeiptr>::max() /
                    static_cast<GLsizeiptr>(sizeof(unsigned int))
                ))
            {
                std::cerr << "Terrain chunk index buffer is too large.\n";
                return false;
            }

            GpuTerrainChunk gpuChunk{};

            GenVertexArrays(1, &gpuChunk.vao);
            GenBuffers(1, &gpuChunk.vbo);
            GenBuffers(1, &gpuChunk.ebo);

            if (gpuChunk.vao == 0 ||
                gpuChunk.vbo == 0 ||
                gpuChunk.ebo == 0)
            {
                std::cerr << "Failed to create terrain GPU buffers.\n";

                DeleteGpuChunk(gpuChunk);
                return false;
            }

            BindVertexArray(gpuChunk.vao);

            BindBuffer(GL_ARRAY_BUFFER, gpuChunk.vbo);

            BufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    vertices.size() * sizeof(TerrainVertex)
                ),
                vertices.data(),
                GL_STATIC_DRAW
            );

            BindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpuChunk.ebo);

            BufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    indices.size() * sizeof(unsigned int)
                ),
                indices.data(),
                GL_STATIC_DRAW
            );

            EnableVertexAttribArray(0);
            VertexAttribPointer(
                0,
                3,
                GL_FLOAT,
                GL_FALSE,
                sizeof(TerrainVertex),
                reinterpret_cast<void*>(
                    offsetof(TerrainVertex, position)
                )
            );

            EnableVertexAttribArray(1);
            VertexAttribPointer(
                1,
                3,
                GL_FLOAT,
                GL_FALSE,
                sizeof(TerrainVertex),
                reinterpret_cast<void*>(
                    offsetof(TerrainVertex, normal)
                )
            );

            EnableVertexAttribArray(2);
            VertexAttribPointer(
                2,
                3,
                GL_FLOAT,
                GL_FALSE,
                sizeof(TerrainVertex),
                reinterpret_cast<void*>(
                    offsetof(TerrainVertex, color)
                )
            );

            BindVertexArray(0);
            BindBuffer(GL_ARRAY_BUFFER, 0);

            gpuChunk.indexCount =
                static_cast<GLsizei>(indices.size());

            gpuChunks.push_back(gpuChunk);

            return true;
        }

        bool CreateTerrain()
        {
            TerrainSettings settings;

            settings.seed = 12345;
            settings.heightScale = 35.0f;
            settings.baseHeight = -5.0f;
            settings.continentalScale = 0.0015f;
            settings.mountainScale = 0.003f;
            settings.detailScale = 0.012f;
            settings.noiseOctaves = 5;
            settings.persistence = 0.5f;
            settings.lacunarity = 2.0f;
            settings.mountainStrength = 1.0f;
            settings.valleyStrength = 0.65f;

            terrainWorld = TerrainWorld(
                settings,
                32.0f,
                1.0f
            );

            constexpr int initialChunkRadius = 4;

            std::cout << "Generating terrain chunks...\n";

            if (!terrainWorld.GenerateWorld(initialChunkRadius))
            {
                std::cerr << "Failed to generate terrain world.\n";
                return false;
            }

            gpuChunks.reserve(
                static_cast<std::size_t>(terrainWorld.GetChunkCount())
            );

            for (const auto& entry : terrainWorld.GetChunks())
            {
                if (!UploadTerrainChunk(entry.second))
                {
                    std::cerr << "Failed to upload terrain chunk at "
                              << entry.second.GetChunkX() << ", "
                              << entry.second.GetChunkZ() << ".\n";

                    return false;
                }
            }

            std::cout << "Generated "
                      << terrainWorld.GetChunkCount()
                      << " terrain chunks.\n";

            std::cout << "Uploaded "
                      << gpuChunks.size()
                      << " terrain meshes to OpenGL.\n";

            return !gpuChunks.empty();
        }

        Vec3 GetForwardDirection() const
        {
            const float cosPitch = std::cos(cameraPitch);

            return Normalize({
                std::sin(cameraYaw) * cosPitch,
                std::sin(cameraPitch),
                -std::cos(cameraYaw) * cosPitch
            });
        }

        Vec3 GetRightDirection() const
        {
            return Normalize({
                std::cos(cameraYaw),
                0.0f,
                std::sin(cameraYaw)
            });
        }

        void ReleaseTerrain()
        {
            for (GpuTerrainChunk& chunk : gpuChunks)
            {
                DeleteGpuChunk(chunk);
            }

            gpuChunks.clear();
            terrainWorld.Clear();
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
        if (!m_impl->LoadFunctions())
        {
            return false;
        }

        if (!m_impl->CreateShaders())
        {
            return false;
        }

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        if (!m_impl->CreateTerrain())
        {
            return false;
        }

        std::cout << "3D terrain renderer initialized successfully.\n";
        std::cout << "Smooth procedural terrain enabled.\n";
        std::cout << "First-person free-fly camera initialized.\n";

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
        if (deltaTime <= 0.0f)
        {
            return;
        }

        Vec3 movement{ 0.0f, 0.0f, 0.0f };

        movement = Add(
            movement,
            Scale(m_impl->GetForwardDirection(), forwardInput)
        );

        movement = Add(
            movement,
            Scale(m_impl->GetRightDirection(), rightInput)
        );

        movement.y += verticalInput;

        const float movementLength =
            std::sqrt(Dot(movement, movement));

        if (movementLength > 1.0f)
        {
            movement = Scale(movement, 1.0f / movementLength);
        }

        const float speed = sprint
            ? m_impl->sprintSpeed
            : m_impl->movementSpeed;

        m_impl->cameraPosition = Add(
            m_impl->cameraPosition,
            Scale(movement, speed * deltaTime)
        );
    }

    void Renderer::RotateCamera(float deltaX, float deltaY)
    {
        m_impl->cameraYaw +=
            deltaX * m_impl->mouseSensitivity;

        m_impl->cameraPitch -=
            deltaY * m_impl->mouseSensitivity;

        const float pitchLimit =
            89.0f * DegreesToRadians;

        m_impl->cameraPitch = std::clamp(
            m_impl->cameraPitch,
            -pitchLimit,
            pitchLimit
        );
    }

    void Renderer::Render(int width, int height)
    {
        if (width <= 0 ||
            height <= 0 ||
            m_impl->shaderProgram == 0 ||
            m_impl->gpuChunks.empty())
        {
            return;
        }

        const float aspect =
            static_cast<float>(width) /
            static_cast<float>(height);

        const Mat4 projection = Perspective(
            0.785398163f,
            aspect,
            0.1f,
            1000.0f
        );

        const Vec3 forward =
            m_impl->GetForwardDirection();

        const Mat4 view = LookAt(
            m_impl->cameraPosition,
            Add(m_impl->cameraPosition, forward),
            { 0.0f, 1.0f, 0.0f }
        );

        const Mat4 mvp = Multiply(projection, view);

        m_impl->UseProgram(m_impl->shaderProgram);

        m_impl->UniformMatrix4fv(
            m_impl->mvpLocation,
            1,
            GL_FALSE,
            mvp.data()
        );

        for (const Impl::GpuTerrainChunk& chunk :
             m_impl->gpuChunks)
        {
            if (chunk.vao == 0 || chunk.indexCount <= 0)
            {
                continue;
            }

            m_impl->BindVertexArray(chunk.vao);

            m_impl->DrawElements(
                GL_TRIANGLES,
                chunk.indexCount,
                GL_UNSIGNED_INT,
                nullptr
            );
        }

        m_impl->BindVertexArray(0);
        m_impl->UseProgram(0);
    }

    void Renderer::Shutdown()
    {
        if (!m_impl)
        {
            return;
        }

        m_impl->ReleaseTerrain();

        if (m_impl->DeleteProgram != nullptr &&
            m_impl->shaderProgram != 0)
        {
            m_impl->DeleteProgram(m_impl->shaderProgram);
            m_impl->shaderProgram = 0;
        }
    }
}
