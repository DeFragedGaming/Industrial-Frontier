
#include "industrial_frontier/renderer/Renderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float DegreesToRadians = Pi / 180.0f;

        struct Vertex
        {
            float x, y, z;
            float r, g, b;
        };

        struct Vec3
        {
            float x, y, z;
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
            float farPlane)
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

        void AddTriangle(
            std::vector<Vertex>& vertices,
            Vec3 a,
            Vec3 b,
            Vec3 c,
            const std::array<float, 3>& color)
        {
            vertices.push_back({
                a.x, a.y, a.z,
                color[0], color[1], color[2]
            });

            vertices.push_back({
                b.x, b.y, b.z,
                color[0], color[1], color[2]
            });

            vertices.push_back({
                c.x, c.y, c.z,
                color[0], color[1], color[2]
            });
        }

        void AddQuad(
            std::vector<Vertex>& vertices,
            Vec3 a,
            Vec3 b,
            Vec3 c,
            Vec3 d,
            const std::array<float, 3>& color)
        {
            AddTriangle(vertices, a, b, c, color);
            AddTriangle(vertices, a, c, d, color);
        }

        std::vector<Vertex> CreateSceneGeometry()
        {
            std::vector<Vertex> vertices;

            const float groundSize = 10.0f;
            const float groundY = -1.0f;

            const Vec3 groundA{ -groundSize, groundY, -groundSize };
            const Vec3 groundB{ groundSize, groundY, -groundSize };
            const Vec3 groundC{ groundSize, groundY, groundSize };
            const Vec3 groundD{ -groundSize, groundY, groundSize };

            AddQuad(
                vertices,
                groundA,
                groundD,
                groundC,
                groundB,
                { 0.28f, 0.32f, 0.35f }
            );

            const float s = 1.0f;

            const Vec3 p000{ -s, -s, -s };
            const Vec3 p001{ -s, -s, s };
            const Vec3 p010{ -s, s, -s };
            const Vec3 p011{ -s, s, s };
            const Vec3 p100{ s, -s, -s };
            const Vec3 p101{ s, -s, s };
            const Vec3 p110{ s, s, -s };
            const Vec3 p111{ s, s, s };

            AddQuad(
                vertices,
                p001, p101, p111, p011,
                { 0.85f, 0.28f, 0.12f }
            );

            AddQuad(
                vertices,
                p100, p000, p010, p110,
                { 0.65f, 0.18f, 0.08f }
            );

            AddQuad(
                vertices,
                p000, p001, p011, p010,
                { 0.72f, 0.22f, 0.10f }
            );

            AddQuad(
                vertices,
                p101, p100, p110, p111,
                { 0.95f, 0.38f, 0.16f }
            );

            AddQuad(
                vertices,
                p010, p011, p111, p110,
                { 1.0f, 0.48f, 0.20f }
            );

            AddQuad(
                vertices,
                p000, p100, p101, p001,
                { 0.55f, 0.15f, 0.06f }
            );

            return vertices;
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

        using DrawArraysFunction = void (APIENTRYP)(
            GLenum mode,
            GLint first,
            GLsizei count
        );

        DrawArraysFunction DrawArrays = nullptr;

        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint shaderProgram = 0;

        GLint mvpLocation = -1;

        GLsizei groundVertexCount = 6;
        GLsizei cubeVertexCount = 0;

        Vec3 cameraPosition{ 0.0f, 0.0f, 6.0f };

        float cameraYaw = 0.0f;
        float cameraPitch = 0.0f;

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

#undef LOAD_GL_FUNCTION

            DrawArrays = reinterpret_cast<DrawArraysFunction>(
                SDL_GL_GetProcAddress("glDrawArrays")
            );

            if (DrawArrays == nullptr)
            {
                std::cerr << "Failed to load OpenGL function: glDrawArrays\n";
                return false;
            }

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
                    static_cast<size_t>(logLength) + 1
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
                layout (location = 1) in vec3 aColor;

                uniform mat4 uMVP;

                out vec3 vertexColor;

                void main()
                {
                    gl_Position = uMVP * vec4(aPosition, 1.0);
                    vertexColor = aColor;
                }
            )";

            constexpr const char* fragmentSource = R"(
                #version 330 core

                in vec3 vertexColor;

                out vec4 FragColor;

                void main()
                {
                    FragColor = vec4(vertexColor, 1.0);
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
                    static_cast<size_t>(logLength) + 1
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

            mvpLocation = GetUniformLocation(shaderProgram, "uMVP");

            return mvpLocation >= 0;
        }

        bool CreateGeometry()
        {
            const std::vector<Vertex> vertices = CreateSceneGeometry();

            groundVertexCount = 6;
            cubeVertexCount =
                static_cast<GLsizei>(vertices.size()) - groundVertexCount;

            GenVertexArrays(1, &vao);
            GenBuffers(1, &vbo);

            if (vao == 0 || vbo == 0)
            {
                std::cerr << "Failed to create scene vertex buffers.\n";
                return false;
            }

            BindVertexArray(vao);
            BindBuffer(GL_ARRAY_BUFFER, vbo);

            BufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    vertices.size() * sizeof(Vertex)
                ),
                vertices.data(),
                GL_STATIC_DRAW
            );

            EnableVertexAttribArray(0);

            VertexAttribPointer(
                0,
                3,
                GL_FLOAT,
                GL_FALSE,
                sizeof(Vertex),
                reinterpret_cast<void*>(offsetof(Vertex, x))
            );

            EnableVertexAttribArray(1);

            VertexAttribPointer(
                1,
                3,
                GL_FLOAT,
                GL_FALSE,
                sizeof(Vertex),
                reinterpret_cast<void*>(offsetof(Vertex, r))
            );

            BindBuffer(GL_ARRAY_BUFFER, 0);
            BindVertexArray(0);

            return true;
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

        if (!m_impl->CreateGeometry())
        {
            return false;
        }

        std::cout << "3D renderer initialized successfully.\n";
        std::cout << "Ground plane and cube geometry created.\n";
        std::cout << "First-person free-fly camera initialized.\n";

        return true;
    }

    void Renderer::UpdateCamera(
        float deltaTime,
        float forwardInput,
        float rightInput,
        float verticalInput,
        bool sprint)
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

        const float movementLength = std::sqrt(Dot(movement, movement));

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
        m_impl->cameraYaw += deltaX * m_impl->mouseSensitivity;
        m_impl->cameraPitch -= deltaY * m_impl->mouseSensitivity;

        const float pitchLimit = 89.0f * DegreesToRadians;

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
            m_impl->vao == 0)
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
            200.0f
        );

        const Vec3 forward = m_impl->GetForwardDirection();

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

        m_impl->BindVertexArray(m_impl->vao);

        m_impl->DrawArrays(
            GL_TRIANGLES,
            0,
            m_impl->groundVertexCount
        );

        m_impl->DrawArrays(
            GL_TRIANGLES,
            m_impl->groundVertexCount,
            m_impl->cubeVertexCount
        );

        m_impl->BindVertexArray(0);
        m_impl->UseProgram(0);
    }

    void Renderer::Shutdown()
    {
        if (!m_impl)
        {
            return;
        }

        if (m_impl->DeleteBuffers != nullptr && m_impl->vbo != 0)
        {
            m_impl->DeleteBuffers(1, &m_impl->vbo);
            m_impl->vbo = 0;
        }

        if (m_impl->DeleteVertexArrays != nullptr && m_impl->vao != 0)
        {
            m_impl->DeleteVertexArrays(1, &m_impl->vao);
            m_impl->vao = 0;
        }

        if (m_impl->DeleteProgram != nullptr &&
            m_impl->shaderProgram != 0)
        {
            m_impl->DeleteProgram(m_impl->shaderProgram);
            m_impl->shaderProgram = 0;
        }
    }
}
