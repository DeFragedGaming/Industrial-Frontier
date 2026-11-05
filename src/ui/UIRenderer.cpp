
#include "industrial_frontier/ui/UIRenderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#ifdef DrawText
#undef DrawText
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>

namespace industrial_frontier
{
    namespace
    {
        using CreateShaderFn =
            GLuint(APIENTRY*)(GLenum);

        using ShaderSourceFn =
            void(APIENTRY*)(
                GLuint,
                GLsizei,
                const GLchar* const*,
                const GLint*
            );

        using CompileShaderFn =
            void(APIENTRY*)(GLuint);

        using GetShaderivFn =
            void(APIENTRY*)(GLuint, GLenum, GLint*);

        using GetShaderInfoLogFn =
            void(APIENTRY*)(
                GLuint,
                GLsizei,
                GLsizei*,
                GLchar*
            );

        using DeleteShaderFn =
            void(APIENTRY*)(GLuint);

        using CreateProgramFn =
            GLuint(APIENTRY*)();

        using AttachShaderFn =
            void(APIENTRY*)(GLuint, GLuint);

        using LinkProgramFn =
            void(APIENTRY*)(GLuint);

        using GetProgramivFn =
            void(APIENTRY*)(GLuint, GLenum, GLint*);

        using GetProgramInfoLogFn =
            void(APIENTRY*)(
                GLuint,
                GLsizei,
                GLsizei*,
                GLchar*
            );

        using DeleteProgramFn =
            void(APIENTRY*)(GLuint);

        using UseProgramFn =
            void(APIENTRY*)(GLuint);

        using GetUniformLocationFn =
            GLint(APIENTRY*)(GLuint, const GLchar*);

        using Uniform2fFn =
            void(APIENTRY*)(GLint, GLfloat, GLfloat);

        using Uniform4fFn =
            void(APIENTRY*)(
                GLint,
                GLfloat,
                GLfloat,
                GLfloat,
                GLfloat
            );

        using GenVertexArraysFn =
            void(APIENTRY*)(GLsizei, GLuint*);

        using BindVertexArrayFn =
            void(APIENTRY*)(GLuint);

        using DeleteVertexArraysFn =
            void(APIENTRY*)(GLsizei, const GLuint*);

        using GenBuffersFn =
            void(APIENTRY*)(GLsizei, GLuint*);

        using BindBufferFn =
            void(APIENTRY*)(GLenum, GLuint);

        using BufferDataFn =
            void(APIENTRY*)(
                GLenum,
                std::ptrdiff_t,
                const void*,
                GLenum
            );

        using DeleteBuffersFn =
            void(APIENTRY*)(GLsizei, const GLuint*);

        using EnableVertexAttribArrayFn =
            void(APIENTRY*)(GLuint);

        using VertexAttribPointerFn =
            void(APIENTRY*)(
                GLuint,
                GLint,
                GLenum,
                GLboolean,
                GLsizei,
                const void*
            );

        struct GLFunctions
        {
            CreateShaderFn createShader = nullptr;
            ShaderSourceFn shaderSource = nullptr;
            CompileShaderFn compileShader = nullptr;
            GetShaderivFn getShaderiv = nullptr;
            GetShaderInfoLogFn getShaderInfoLog = nullptr;
            DeleteShaderFn deleteShader = nullptr;

            CreateProgramFn createProgram = nullptr;
            AttachShaderFn attachShader = nullptr;
            LinkProgramFn linkProgram = nullptr;
            GetProgramivFn getProgramiv = nullptr;
            GetProgramInfoLogFn getProgramInfoLog = nullptr;
            DeleteProgramFn deleteProgram = nullptr;
            UseProgramFn useProgram = nullptr;

            GetUniformLocationFn getUniformLocation = nullptr;
            Uniform2fFn uniform2f = nullptr;
            Uniform4fFn uniform4f = nullptr;

            GenVertexArraysFn genVertexArrays = nullptr;
            BindVertexArrayFn bindVertexArray = nullptr;
            DeleteVertexArraysFn deleteVertexArrays = nullptr;

            GenBuffersFn genBuffers = nullptr;
            BindBufferFn bindBuffer = nullptr;
            BufferDataFn bufferData = nullptr;
            DeleteBuffersFn deleteBuffers = nullptr;

            EnableVertexAttribArrayFn enableVertexAttribArray = nullptr;
            VertexAttribPointerFn vertexAttribPointer = nullptr;
        };

        GLFunctions gl{};

        template<typename T>
        bool LoadFunction(T& function, const char* name)
        {
            function = reinterpret_cast<T>(
                SDL_GL_GetProcAddress(name)
            );

            if (!function)
            {
                std::cerr
                    << "Missing OpenGL function: "
                    << name << '\n';

                return false;
            }

            return true;
        }

        bool LoadFunctions()
        {
            bool success = true;

#define LOAD_GL(member, name) \
    success = LoadFunction(gl.member, name) && success

            LOAD_GL(createShader, "glCreateShader");
            LOAD_GL(shaderSource, "glShaderSource");
            LOAD_GL(compileShader, "glCompileShader");
            LOAD_GL(getShaderiv, "glGetShaderiv");
            LOAD_GL(getShaderInfoLog, "glGetShaderInfoLog");
            LOAD_GL(deleteShader, "glDeleteShader");

            LOAD_GL(createProgram, "glCreateProgram");
            LOAD_GL(attachShader, "glAttachShader");
            LOAD_GL(linkProgram, "glLinkProgram");
            LOAD_GL(getProgramiv, "glGetProgramiv");
            LOAD_GL(getProgramInfoLog, "glGetProgramInfoLog");
            LOAD_GL(deleteProgram, "glDeleteProgram");
            LOAD_GL(useProgram, "glUseProgram");

            LOAD_GL(
                getUniformLocation,
                "glGetUniformLocation"
            );

            LOAD_GL(uniform2f, "glUniform2f");
            LOAD_GL(uniform4f, "glUniform4f");

            LOAD_GL(genVertexArrays, "glGenVertexArrays");
            LOAD_GL(bindVertexArray, "glBindVertexArray");

            LOAD_GL(
                deleteVertexArrays,
                "glDeleteVertexArrays"
            );

            LOAD_GL(genBuffers, "glGenBuffers");
            LOAD_GL(bindBuffer, "glBindBuffer");
            LOAD_GL(bufferData, "glBufferData");
            LOAD_GL(deleteBuffers, "glDeleteBuffers");

            LOAD_GL(
                enableVertexAttribArray,
                "glEnableVertexAttribArray"
            );

            LOAD_GL(
                vertexAttribPointer,
                "glVertexAttribPointer"
            );

#undef LOAD_GL

            return success;
        }

        constexpr const char* VertexSource = R"(
            #version 330 core

            layout(location = 0) in vec2 aPosition;

            uniform vec2 uViewport;

            void main()
            {
                vec2 normalized = aPosition / uViewport;

                vec2 clipPosition = vec2(
                    normalized.x * 2.0 - 1.0,
                    1.0 - normalized.y * 2.0
                );

                gl_Position = vec4(
                    clipPosition,
                    0.0,
                    1.0
                );
            }
        )";

        constexpr const char* FragmentSource = R"(
            #version 330 core

            out vec4 FragColor;

            uniform vec4 uColor;

            void main()
            {
                FragColor = uColor;
            }
        )";

        GLuint CompileShader(
            GLenum type,
            const char* source)
        {
            const GLuint shader = gl.createShader(type);

            if (shader == 0)
            {
                return 0;
            }

            gl.shaderSource(shader, 1, &source, nullptr);
            gl.compileShader(shader);

            GLint success = GL_FALSE;
            gl.getShaderiv(shader, GL_COMPILE_STATUS, &success);

            if (success != GL_TRUE)
            {
                std::array<char, 2048> log{};

                gl.getShaderInfoLog(
                    shader,
                    static_cast<GLsizei>(log.size()),
                    nullptr,
                    log.data()
                );

                std::cerr
                    << "UI shader compilation failed:\n"
                    << log.data() << '\n';

                gl.deleteShader(shader);
                return 0;
            }

            return shader;
        }

        using Glyph = std::array<unsigned char, 7>;

        Glyph GetGlyph(char character)
        {
            const char c = static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(character)
                )
            );

            switch (c)
            {
                case 'A': return {14,17,17,31,17,17,17};
                case 'B': return {30,17,17,30,17,17,30};
                case 'C': return {14,17,16,16,16,17,14};
                case 'D': return {30,17,17,17,17,17,30};
                case 'E': return {31,16,16,30,16,16,31};
                case 'F': return {31,16,16,30,16,16,16};
                case 'G': return {14,17,16,23,17,17,14};
                case 'H': return {17,17,17,31,17,17,17};
                case 'I': return {31,4,4,4,4,4,31};
                case 'J': return {7,2,2,2,18,18,12};
                case 'K': return {17,18,20,24,20,18,17};
                case 'L': return {16,16,16,16,16,16,31};
                case 'M': return {17,27,21,21,17,17,17};
                case 'N': return {17,25,21,19,17,17,17};
                case 'O': return {14,17,17,17,17,17,14};
                case 'P': return {30,17,17,30,16,16,16};
                case 'Q': return {14,17,17,17,21,18,13};
                case 'R': return {30,17,17,30,20,18,17};
                case 'S': return {15,16,16,14,1,1,30};
                case 'T': return {31,4,4,4,4,4,4};
                case 'U': return {17,17,17,17,17,17,14};
                case 'V': return {17,17,17,17,17,10,4};
                case 'W': return {17,17,17,21,21,21,10};
                case 'X': return {17,17,10,4,10,17,17};
                case 'Y': return {17,17,10,4,4,4,4};
                case 'Z': return {31,1,2,4,8,16,31};

                case '0': return {14,17,19,21,25,17,14};
                case '1': return {4,12,4,4,4,4,14};
                case '2': return {14,17,1,2,4,8,31};
                case '3': return {30,1,1,14,1,1,30};
                case '4': return {2,6,10,18,31,2,2};
                case '5': return {31,16,16,30,1,1,30};
                case '6': return {14,16,16,30,17,17,14};
                case '7': return {31,1,2,4,8,8,8};
                case '8': return {14,17,17,14,17,17,14};
                case '9': return {14,17,17,15,1,1,14};

                case '.': return {0,0,0,0,0,12,12};
                case ',': return {0,0,0,0,4,4,8};
                case ':': return {0,4,4,0,4,4,0};
                case '-': return {0,0,0,31,0,0,0};
                case '!': return {4,4,4,4,4,0,4};
                case '?': return {14,17,1,2,4,0,4};
                case '/': return {1,1,2,4,8,16,16};
                case '\'': return {4,4,8,0,0,0,0};
                case ' ': return {0,0,0,0,0,0,0};

                default: return {31,17,5,9,4,0,4};
            }
        }
    }

    UIRenderer::~UIRenderer()
    {
        Shutdown();
    }

    bool UIRenderer::Initialize()
    {
        if (m_initialized)
        {
            return true;
        }

        if (!LoadFunctions())
        {
            return false;
        }

        const GLuint vertexShader =
            CompileShader(GL_VERTEX_SHADER, VertexSource);

        const GLuint fragmentShader =
            CompileShader(GL_FRAGMENT_SHADER, FragmentSource);

        if (vertexShader == 0 || fragmentShader == 0)
        {
            if (vertexShader != 0)
            {
                gl.deleteShader(vertexShader);
            }

            if (fragmentShader != 0)
            {
                gl.deleteShader(fragmentShader);
            }

            return false;
        }

        m_program = gl.createProgram();

        if (m_program == 0)
        {
            gl.deleteShader(vertexShader);
            gl.deleteShader(fragmentShader);
            return false;
        }

        gl.attachShader(m_program, vertexShader);
        gl.attachShader(m_program, fragmentShader);
        gl.linkProgram(m_program);

        gl.deleteShader(vertexShader);
        gl.deleteShader(fragmentShader);

        GLint linked = GL_FALSE;

        gl.getProgramiv(
            m_program,
            GL_LINK_STATUS,
            &linked
        );

        if (linked != GL_TRUE)
        {
            std::array<char, 2048> log{};

            gl.getProgramInfoLog(
                m_program,
                static_cast<GLsizei>(log.size()),
                nullptr,
                log.data()
            );

            std::cerr
                << "UI shader linking failed:\n"
                << log.data() << '\n';

            Shutdown();
            return false;
        }

        m_viewportUniform = gl.getUniformLocation(
            m_program,
            "uViewport"
        );

        m_colorUniform = gl.getUniformLocation(
            m_program,
            "uColor"
        );

        gl.genVertexArrays(1, &m_vertexArray);
        gl.genBuffers(1, &m_vertexBuffer);

        if (m_vertexArray == 0 || m_vertexBuffer == 0)
        {
            Shutdown();
            return false;
        }

        gl.bindVertexArray(m_vertexArray);
        gl.bindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);

        gl.enableVertexAttribArray(0);

        gl.vertexAttribPointer(
            0,
            2,
            GL_FLOAT,
            GL_FALSE,
            sizeof(float) * 2,
            nullptr
        );

        gl.bindVertexArray(0);
        gl.bindBuffer(GL_ARRAY_BUFFER, 0);

        m_initialized = true;

        std::cout << "UI renderer initialized.\n";

        return true;
    }

    void UIRenderer::Shutdown()
    {
        if (gl.deleteBuffers && m_vertexBuffer != 0)
        {
            gl.deleteBuffers(1, &m_vertexBuffer);
            m_vertexBuffer = 0;
        }

        if (gl.deleteVertexArrays && m_vertexArray != 0)
        {
            gl.deleteVertexArrays(1, &m_vertexArray);
            m_vertexArray = 0;
        }

        if (gl.deleteProgram && m_program != 0)
        {
            gl.deleteProgram(m_program);
            m_program = 0;
        }

        m_initialized = false;
    }

    void UIRenderer::BeginFrame(int width, int height)
    {
        if (!m_initialized)
        {
            return;
        }

        m_viewportWidth = std::max(width, 1);
        m_viewportHeight = std::max(height, 1);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glEnable(GL_BLEND);
        glBlendFunc(
            GL_SRC_ALPHA,
            GL_ONE_MINUS_SRC_ALPHA
        );

        gl.useProgram(m_program);
        gl.bindVertexArray(m_vertexArray);

        gl.uniform2f(
            m_viewportUniform,
            static_cast<float>(m_viewportWidth),
            static_cast<float>(m_viewportHeight)
        );
    }

    void UIRenderer::EndFrame()
    {
        if (!m_initialized)
        {
            return;
        }

        gl.bindVertexArray(0);
        gl.bindBuffer(GL_ARRAY_BUFFER, 0);
        gl.useProgram(0);

        glDisable(GL_BLEND);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
    }

    void UIRenderer::DrawQuad(
        float x,
        float y,
        float width,
        float height,
        const UIColor& color)
    {
        if (!m_initialized ||
            width <= 0.0f ||
            height <= 0.0f)
        {
            return;
        }

        const float vertices[] =
        {
            x,         y,
            x + width, y,
            x + width, y + height,

            x,         y,
            x + width, y + height,
            x,         y + height
        };

        gl.uniform4f(
            m_colorUniform,
            color.r,
            color.g,
            color.b,
            color.a
        );

        gl.bindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);

        gl.bufferData(
            GL_ARRAY_BUFFER,
            static_cast<std::ptrdiff_t>(sizeof(vertices)),
            vertices,
            GL_STREAM_DRAW
        );

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    void UIRenderer::DrawRectangle(
        float x,
        float y,
        float width,
        float height,
        const UIColor& color)
    {
        DrawQuad(x, y, width, height, color);
    }

    void UIRenderer::DrawGlyph(
        char character,
        float x,
        float y,
        float scale,
        const UIColor& color)
    {
        const Glyph glyph = GetGlyph(character);

        for (int row = 0; row < 7; ++row)
        {
            for (int column = 0; column < 5; ++column)
            {
                const unsigned char mask =
                    static_cast<unsigned char>(
                        1u << (4 - column)
                    );

                if ((glyph[row] & mask) == 0)
                {
                    continue;
                }

                DrawQuad(
                    x + static_cast<float>(column) * scale,
                    y + static_cast<float>(row) * scale,
                    scale,
                    scale,
                    color
                );
            }
        }
    }

    void UIRenderer::DrawText(
        std::string_view text,
        float x,
        float y,
        float scale,
        const UIColor& color)
    {
        if (!m_initialized || scale <= 0.0f)
        {
            return;
        }

        const float startX = x;

        for (const char character : text)
        {
            if (character == '\n')
            {
                x = startX;
                y += scale * 9.0f;
                continue;
            }

            DrawGlyph(
                character,
                x,
                y,
                scale,
                color
            );

            x += scale * 6.0f;
        }
    }

    float UIRenderer::MeasureText(
        std::string_view text,
        float scale) const
    {
        float currentWidth = 0.0f;
        float maximumWidth = 0.0f;

        for (const char character : text)
        {
            if (character == '\n')
            {
                maximumWidth = std::max(
                    maximumWidth,
                    currentWidth
                );

                currentWidth = 0.0f;
                continue;
            }

            currentWidth += scale * 6.0f;
        }

        return std::max(maximumWidth, currentWidth);
    }

    bool UIRenderer::IsInitialized() const
    {
        return m_initialized;
    }
}
