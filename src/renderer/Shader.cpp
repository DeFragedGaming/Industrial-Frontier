#include "industrial_frontier/renderer/Shader.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

namespace industrial_frontier
{
    namespace
    {
        using CreateShaderProc = GLuint(APIENTRY*)(GLenum);
        using ShaderSourceProc = void(APIENTRY*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
        using CompileShaderProc = void(APIENTRY*)(GLuint);
        using GetShaderivProc = void(APIENTRY*)(GLuint, GLenum, GLint*);
        using GetShaderInfoLogProc = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
        using DeleteShaderProc = void(APIENTRY*)(GLuint);

        using CreateProgramProc = GLuint(APIENTRY*)();
        using AttachShaderProc = void(APIENTRY*)(GLuint, GLuint);
        using LinkProgramProc = void(APIENTRY*)(GLuint);
        using GetProgramivProc = void(APIENTRY*)(GLuint, GLenum, GLint*);
        using GetProgramInfoLogProc = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
        using DeleteProgramProc = void(APIENTRY*)(GLuint);
        using UseProgramProc = void(APIENTRY*)(GLuint);

        using GetUniformLocationProc = GLint(APIENTRY*)(GLuint, const GLchar*);
        using Uniform1iProc = void(APIENTRY*)(GLint, GLint);
        using Uniform1fProc = void(APIENTRY*)(GLint, GLfloat);
        using Uniform3fProc = void(APIENTRY*)(GLint, GLfloat, GLfloat, GLfloat);
        using UniformMatrix4fvProc = void(APIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*);

        template<typename T>
        bool LoadFunction(T& function, const char* name)
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

        bool ReadTextFile(
            const std::string& path,
            std::string& output
        )
        {
            std::ifstream file(path);

            if (!file.is_open())
            {
                std::cerr << "Failed to open shader file: "
                          << path << '\n';

                return false;
            }

            std::ostringstream contents;
            contents << file.rdbuf();

            output = contents.str();

            return !output.empty();
        }
    }

    struct Shader::Impl
    {
        CreateShaderProc CreateShader = nullptr;
        ShaderSourceProc ShaderSource = nullptr;
        CompileShaderProc CompileShader = nullptr;
        GetShaderivProc GetShaderiv = nullptr;
        GetShaderInfoLogProc GetShaderInfoLog = nullptr;
        DeleteShaderProc DeleteShader = nullptr;

        CreateProgramProc CreateProgram = nullptr;
        AttachShaderProc AttachShader = nullptr;
        LinkProgramProc LinkProgram = nullptr;
        GetProgramivProc GetProgramiv = nullptr;
        GetProgramInfoLogProc GetProgramInfoLog = nullptr;
        DeleteProgramProc DeleteProgram = nullptr;
        UseProgramProc UseProgram = nullptr;

        GetUniformLocationProc GetUniformLocation = nullptr;
        Uniform1iProc Uniform1i = nullptr;
        Uniform1fProc Uniform1f = nullptr;
        Uniform3fProc Uniform3f = nullptr;
        UniformMatrix4fvProc UniformMatrix4fv = nullptr;

        GLuint program = 0;
        bool functionsLoaded = false;

        bool LoadFunctions()
        {
            if (functionsLoaded)
            {
                return true;
            }

            bool success = true;

            success &= LoadFunction(CreateShader, "glCreateShader");
            success &= LoadFunction(ShaderSource, "glShaderSource");
            success &= LoadFunction(CompileShader, "glCompileShader");
            success &= LoadFunction(GetShaderiv, "glGetShaderiv");
            success &= LoadFunction(GetShaderInfoLog, "glGetShaderInfoLog");
            success &= LoadFunction(DeleteShader, "glDeleteShader");

            success &= LoadFunction(CreateProgram, "glCreateProgram");
            success &= LoadFunction(AttachShader, "glAttachShader");
            success &= LoadFunction(LinkProgram, "glLinkProgram");
            success &= LoadFunction(GetProgramiv, "glGetProgramiv");
            success &= LoadFunction(GetProgramInfoLog, "glGetProgramInfoLog");
            success &= LoadFunction(DeleteProgram, "glDeleteProgram");
            success &= LoadFunction(UseProgram, "glUseProgram");

            success &= LoadFunction(GetUniformLocation, "glGetUniformLocation");
            success &= LoadFunction(Uniform1i, "glUniform1i");
            success &= LoadFunction(Uniform1f, "glUniform1f");
            success &= LoadFunction(Uniform3f, "glUniform3f");
            success &= LoadFunction(UniformMatrix4fv, "glUniformMatrix4fv");

            functionsLoaded = success;

            return success;
        }

        GLuint Compile(
            GLenum type,
            const std::string& source,
            const std::string& label
        )
        {
            const GLuint shader = CreateShader(type);

            if (shader == 0)
            {
                std::cerr << "Failed to create shader: "
                          << label << '\n';

                return 0;
            }

            const GLchar* sourcePointer = source.c_str();

            ShaderSource(
                shader,
                1,
                &sourcePointer,
                nullptr
            );

            CompileShader(shader);

            GLint compiled = GL_FALSE;
            GetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

            if (compiled != GL_TRUE)
            {
                GLint logLength = 0;
                GetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

                std::string log(
                    static_cast<std::size_t>(logLength > 0 ? logLength : 1),
                    '\0'
                );

                GetShaderInfoLog(
                    shader,
                    logLength,
                    nullptr,
                    log.data()
                );

                std::cerr << "Shader compilation failed: "
                          << label << '\n'
                          << log << '\n';

                DeleteShader(shader);

                return 0;
            }

            return shader;
        }

        GLint GetLocation(const std::string& name) const
        {
            if (program == 0 || GetUniformLocation == nullptr)
            {
                return -1;
            }

            return GetUniformLocation(program, name.c_str());
        }
    };

    Shader::Shader()
        : m_impl(std::make_unique<Impl>())
    {
    }

    Shader::~Shader()
    {
        Shutdown();
    }

    Shader::Shader(Shader&& other) noexcept = default;

    Shader& Shader::operator=(Shader&& other) noexcept = default;

    bool Shader::LoadFromFiles(
        const std::string& vertexPath,
        const std::string& fragmentPath
    )
    {
        if (!m_impl)
        {
            m_impl = std::make_unique<Impl>();
        }

        if (!m_impl->LoadFunctions())
        {
            return false;
        }

        std::string vertexSource;
        std::string fragmentSource;

        if (!ReadTextFile(vertexPath, vertexSource) ||
            !ReadTextFile(fragmentPath, fragmentSource))
        {
            return false;
        }

        const GLuint vertexShader = m_impl->Compile(
            GL_VERTEX_SHADER,
            vertexSource,
            vertexPath
        );

        if (vertexShader == 0)
        {
            return false;
        }

        const GLuint fragmentShader = m_impl->Compile(
            GL_FRAGMENT_SHADER,
            fragmentSource,
            fragmentPath
        );

        if (fragmentShader == 0)
        {
            m_impl->DeleteShader(vertexShader);
            return false;
        }

        const GLuint newProgram = m_impl->CreateProgram();

        if (newProgram == 0)
        {
            std::cerr << "Failed to create shader program.\n";

            m_impl->DeleteShader(vertexShader);
            m_impl->DeleteShader(fragmentShader);

            return false;
        }

        m_impl->AttachShader(newProgram, vertexShader);
        m_impl->AttachShader(newProgram, fragmentShader);
        m_impl->LinkProgram(newProgram);

        GLint linked = GL_FALSE;

        m_impl->GetProgramiv(
            newProgram,
            GL_LINK_STATUS,
            &linked
        );

        m_impl->DeleteShader(vertexShader);
        m_impl->DeleteShader(fragmentShader);

        if (linked != GL_TRUE)
        {
            GLint logLength = 0;

            m_impl->GetProgramiv(
                newProgram,
                GL_INFO_LOG_LENGTH,
                &logLength
            );

            std::string log(
                static_cast<std::size_t>(logLength > 0 ? logLength : 1),
                '\0'
            );

            m_impl->GetProgramInfoLog(
                newProgram,
                logLength,
                nullptr,
                log.data()
            );

            std::cerr << "Shader program linking failed:\n"
                      << log << '\n';

            m_impl->DeleteProgram(newProgram);

            return false;
        }

        if (m_impl->program != 0)
        {
            m_impl->DeleteProgram(m_impl->program);
        }

        m_impl->program = newProgram;

        std::cout << "Loaded shader program successfully.\n";

        return true;
    }

    void Shader::Use() const
    {
        if (m_impl && m_impl->program != 0)
        {
            m_impl->UseProgram(m_impl->program);
        }
    }

    void Shader::Shutdown()
    {
        if (m_impl &&
            m_impl->program != 0 &&
            m_impl->DeleteProgram != nullptr)
        {
            m_impl->DeleteProgram(m_impl->program);
            m_impl->program = 0;
        }
    }

    bool Shader::IsValid() const
    {
        return m_impl && m_impl->program != 0;
    }

    unsigned int Shader::GetProgramID() const
    {
        return m_impl
            ? static_cast<unsigned int>(m_impl->program)
            : 0;
    }

    void Shader::SetBool(
        const std::string& name,
        bool value
    ) const
    {
        SetInt(name, value ? 1 : 0);
    }

    void Shader::SetInt(
        const std::string& name,
        int value
    ) const
    {
        if (!m_impl || m_impl->program == 0)
        {
            return;
        }

        const GLint location = m_impl->GetLocation(name);

        if (location >= 0)
        {
            m_impl->Uniform1i(location, value);
        }
    }

    void Shader::SetFloat(
        const std::string& name,
        float value
    ) const
    {
        if (!m_impl || m_impl->program == 0)
        {
            return;
        }

        const GLint location = m_impl->GetLocation(name);

        if (location >= 0)
        {
            m_impl->Uniform1f(location, value);
        }
    }

    void Shader::SetVec3(
        const std::string& name,
        float x,
        float y,
        float z
    ) const
    {
        if (!m_impl || m_impl->program == 0)
        {
            return;
        }

        const GLint location = m_impl->GetLocation(name);

        if (location >= 0)
        {
            m_impl->Uniform3f(location, x, y, z);
        }
    }

    void Shader::SetMat4(
        const std::string& name,
        const float* matrix
    ) const
    {
        if (!m_impl ||
            m_impl->program == 0 ||
            matrix == nullptr)
        {
            return;
        }

        const GLint location = m_impl->GetLocation(name);

        if (location >= 0)
        {
            m_impl->UniformMatrix4fv(
                location,
                1,
                GL_FALSE,
                matrix
            );
        }
    }
}