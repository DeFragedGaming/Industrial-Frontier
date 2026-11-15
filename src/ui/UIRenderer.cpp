#include "industrial_frontier/ui/UIRenderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef DrawText
#undef DrawText
#endif

namespace industrial_frontier
{
    namespace
    {
        using CreateShaderFn = GLuint(APIENTRY*)(GLenum);
        using ShaderSourceFn = void(APIENTRY*)(GLuint, GLsizei, const GLchar* const*, const GLint*);
        using CompileShaderFn = void(APIENTRY*)(GLuint);
        using GetShaderivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
        using GetShaderInfoLogFn = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
        using DeleteShaderFn = void(APIENTRY*)(GLuint);
        using CreateProgramFn = GLuint(APIENTRY*)();
        using AttachShaderFn = void(APIENTRY*)(GLuint, GLuint);
        using LinkProgramFn = void(APIENTRY*)(GLuint);
        using GetProgramivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
        using GetProgramInfoLogFn = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
        using DeleteProgramFn = void(APIENTRY*)(GLuint);
        using UseProgramFn = void(APIENTRY*)(GLuint);
        using GetUniformLocationFn = GLint(APIENTRY*)(GLuint, const GLchar*);
        using Uniform1iFn = void(APIENTRY*)(GLint, GLint);
        using GenVertexArraysFn = void(APIENTRY*)(GLsizei, GLuint*);
        using BindVertexArrayFn = void(APIENTRY*)(GLuint);
        using DeleteVertexArraysFn = void(APIENTRY*)(GLsizei, const GLuint*);
        using GenBuffersFn = void(APIENTRY*)(GLsizei, GLuint*);
        using BindBufferFn = void(APIENTRY*)(GLenum, GLuint);
        using BufferDataFn = void(APIENTRY*)(GLenum, GLsizeiptr, const void*, GLenum);
        using DeleteBuffersFn = void(APIENTRY*)(GLsizei, const GLuint*);
        using EnableVertexAttribArrayFn = void(APIENTRY*)(GLuint);
        using VertexAttribPointerFn = void(APIENTRY*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
        using DrawArraysFn = void(APIENTRY*)(GLenum, GLint, GLsizei);
        using ActiveTextureFn = void(APIENTRY*)(GLenum);

        template<class T>
        bool Load(T& function, const char* name)
        {
            function = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
            return function != nullptr;
        }

        std::filesystem::path LocateFont()
        {
            std::vector<std::filesystem::path> options;
#ifdef _WIN32
            const char* windir = std::getenv("WINDIR");
            const std::filesystem::path root = windir ? windir : "C:\\Windows";
            for (const char* name : {"segoeui.ttf", "seguisb.ttf", "arial.ttf", "tahoma.ttf", "verdana.ttf"})
            {
                options.push_back(root / "Fonts" / name);
            }
#else
            for (const char* name : {
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf",
                "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
                "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
                "/usr/share/fonts/truetype/ubuntu/Ubuntu-R.ttf"
            })
            {
                options.emplace_back(name);
            }
#endif
            for (const auto& path : options)
            {
                std::error_code error;
                if (std::filesystem::is_regular_file(path, error))
                {
                    return path;
                }
            }
            return {};
        }

        int FontPixels(float scale)
        {
            return std::clamp(static_cast<int>(14.5f + (scale - 1.0f) * 5.0f + 0.5f), 13, 32);
        }

        std::string CacheKey(const std::string& text, int pointSize)
        {
            return std::to_string(pointSize) + ":" + text;
        }
    }

    struct UIRenderer::Impl
    {
        struct GLFns
        {
            CreateShaderFn createShader{};
            ShaderSourceFn shaderSource{};
            CompileShaderFn compileShader{};
            GetShaderivFn getShaderiv{};
            GetShaderInfoLogFn getShaderInfoLog{};
            DeleteShaderFn deleteShader{};
            CreateProgramFn createProgram{};
            AttachShaderFn attachShader{};
            LinkProgramFn linkProgram{};
            GetProgramivFn getProgramiv{};
            GetProgramInfoLogFn getProgramInfoLog{};
            DeleteProgramFn deleteProgram{};
            UseProgramFn useProgram{};
            GetUniformLocationFn getUniformLocation{};
            Uniform1iFn uniform1i{};
            GenVertexArraysFn genVertexArrays{};
            BindVertexArrayFn bindVertexArray{};
            DeleteVertexArraysFn deleteVertexArrays{};
            GenBuffersFn genBuffers{};
            BindBufferFn bindBuffer{};
            BufferDataFn bufferData{};
            DeleteBuffersFn deleteBuffers{};
            EnableVertexAttribArrayFn enableVertexAttribArray{};
            VertexAttribPointerFn vertexAttribPointer{};
            DrawArraysFn drawArrays{};
            ActiveTextureFn activeTexture{};
        } gl;

        struct DrawCall
        {
            bool text = false;
            bool image = false;
            float x = 0.0f;
            float y = 0.0f;
            float width = 0.0f;
            float height = 0.0f;
            int fontSize = 14;
            std::string content;
            UIColor color;
        };

        struct CachedTexture
        {
            GLuint texture = 0;
            int width = 0;
            int height = 0;
            std::uint64_t usedFrame = 0;
        };

        GLuint program = 0;
        GLuint vertexArray = 0;
        GLuint vertexBuffer = 0;
        GLint textureLocation = -1;
        GLint useTextureLocation = -1;
        TTF_Font* font = nullptr;
        bool ownsTtf = false;
        int currentFontSize = 0;
        int viewportWidth = 1;
        int viewportHeight = 1;
        std::uint64_t frame = 0;
        std::vector<DrawCall> calls;
        std::unordered_map<std::string, CachedTexture> textures;
        std::unordered_map<std::string, CachedTexture> iconTextures;
        std::unordered_map<std::string, bool> missingIcons;
        std::unordered_map<std::string, std::pair<int,int>> measured;

        bool LoadGL()
        {
            bool success = true;
            success &= Load(gl.createShader, "glCreateShader");
            success &= Load(gl.shaderSource, "glShaderSource");
            success &= Load(gl.compileShader, "glCompileShader");
            success &= Load(gl.getShaderiv, "glGetShaderiv");
            success &= Load(gl.getShaderInfoLog, "glGetShaderInfoLog");
            success &= Load(gl.deleteShader, "glDeleteShader");
            success &= Load(gl.createProgram, "glCreateProgram");
            success &= Load(gl.attachShader, "glAttachShader");
            success &= Load(gl.linkProgram, "glLinkProgram");
            success &= Load(gl.getProgramiv, "glGetProgramiv");
            success &= Load(gl.getProgramInfoLog, "glGetProgramInfoLog");
            success &= Load(gl.deleteProgram, "glDeleteProgram");
            success &= Load(gl.useProgram, "glUseProgram");
            success &= Load(gl.getUniformLocation, "glGetUniformLocation");
            success &= Load(gl.uniform1i, "glUniform1i");
            success &= Load(gl.genVertexArrays, "glGenVertexArrays");
            success &= Load(gl.bindVertexArray, "glBindVertexArray");
            success &= Load(gl.deleteVertexArrays, "glDeleteVertexArrays");
            success &= Load(gl.genBuffers, "glGenBuffers");
            success &= Load(gl.bindBuffer, "glBindBuffer");
            success &= Load(gl.bufferData, "glBufferData");
            success &= Load(gl.deleteBuffers, "glDeleteBuffers");
            success &= Load(gl.enableVertexAttribArray, "glEnableVertexAttribArray");
            success &= Load(gl.vertexAttribPointer, "glVertexAttribPointer");
            success &= Load(gl.drawArrays, "glDrawArrays");
            success &= Load(gl.activeTexture, "glActiveTexture");
            return success;
        }

        GLuint Compile(GLenum type, const char* source)
        {
            GLuint shader = gl.createShader(type);
            gl.shaderSource(shader, 1, &source, nullptr);
            gl.compileShader(shader);
            GLint status = 0;
            gl.getShaderiv(shader, 0x8B81, &status);
            if (status == 0)
            {
                char log[2048]{};
                gl.getShaderInfoLog(shader, static_cast<GLsizei>(sizeof(log)), nullptr, log);
                std::cerr << "UI font shader: " << log << '\n';
                gl.deleteShader(shader);
                return 0;
            }
            return shader;
        }

        bool SetupShader()
        {
            constexpr const char* vertexSource = R"(#version 330 core
layout(location=0) in vec2 aPosition;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
out vec2 vUV;
out vec4 vColor;
void main()
{
    gl_Position = vec4(aPosition, 0.0, 1.0);
    vUV = aUV;
    vColor = aColor;
}
)";
            constexpr const char* fragmentSource = R"(#version 330 core
in vec2 vUV;
in vec4 vColor;
uniform sampler2D uTexture;
uniform int uUseTexture;
out vec4 fragColor;
void main()
{
    if (uUseTexture == 0)
    {
        fragColor = vColor;
    }
    else if (uUseTexture == 1)
    {
        fragColor = vec4(vColor.rgb, vColor.a * texture(uTexture, vUV).a);
    }
    else
    {
        vec4 icon = texture(uTexture, vUV);
        fragColor = vec4(icon.rgb * vColor.rgb, icon.a * vColor.a);
    }
}
)";

            const GLuint vertex = Compile(0x8B31, vertexSource);
            const GLuint fragment = Compile(0x8B30, fragmentSource);
            if (vertex == 0 || fragment == 0)
            {
                if (vertex != 0) gl.deleteShader(vertex);
                if (fragment != 0) gl.deleteShader(fragment);
                return false;
            }
            program = gl.createProgram();
            gl.attachShader(program, vertex);
            gl.attachShader(program, fragment);
            gl.linkProgram(program);
            gl.deleteShader(vertex);
            gl.deleteShader(fragment);

            GLint linked = 0;
            gl.getProgramiv(program, 0x8B82, &linked);
            if (linked == 0)
            {
                char log[2048]{};
                gl.getProgramInfoLog(program, static_cast<GLsizei>(sizeof(log)), nullptr, log);
                std::cerr << "UI font shader link: " << log << '\n';
                return false;
            }
            textureLocation = gl.getUniformLocation(program, "uTexture");
            useTextureLocation = gl.getUniformLocation(program, "uUseTexture");

            gl.genVertexArrays(1, &vertexArray);
            gl.genBuffers(1, &vertexBuffer);
            gl.bindVertexArray(vertexArray);
            gl.bindBuffer(0x8892, vertexBuffer);
            constexpr GLsizei stride = static_cast<GLsizei>(sizeof(float) * 8);
            gl.enableVertexAttribArray(0);
            gl.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, nullptr);
            gl.enableVertexAttribArray(1);
            gl.vertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(sizeof(float) * 2));
            gl.enableVertexAttribArray(2);
            gl.vertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(sizeof(float) * 4));
            gl.bindVertexArray(0);
            gl.bindBuffer(0x8892, 0);
            return true;
        }

        bool SetSize(int size)
        {
            if (currentFontSize == size)
            {
                return true;
            }
            if (!TTF_SetFontSize(font, static_cast<float>(size)))
            {
                return false;
            }
            currentFontSize = size;
            return true;
        }

        std::pair<int,int> Measure(const std::string& text, int size)
        {
            const std::string key = CacheKey(text, size);
            const auto existing = measured.find(key);
            if (existing != measured.end())
            {
                return existing->second;
            }
            int width = 0;
            int height = 0;
            if (font != nullptr && SetSize(size))
            {
                TTF_GetStringSize(font, text.c_str(), 0, &width, &height);
            }
            if (measured.size() > 5000)
            {
                measured.clear();
            }
            measured.emplace(key, std::make_pair(width, height));
            return {width, height};
        }

        CachedTexture* TextureFor(const std::string& text, int size)
        {
            const std::string key = CacheKey(text, size);
            auto found = textures.find(key);
            if (found != textures.end())
            {
                found->second.usedFrame = frame;
                return &found->second;
            }
            if (font == nullptr || text.empty() || !SetSize(size))
            {
                return nullptr;
            }

            SDL_Surface* rendered = TTF_RenderText_Blended(font, text.c_str(), 0,
                SDL_Color{255, 255, 255, 255});
            if (rendered == nullptr)
            {
                return nullptr;
            }
            SDL_Surface* rgba = SDL_ConvertSurface(rendered, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(rendered);
            if (rgba == nullptr || rgba->w <= 0 || rgba->h <= 0)
            {
                if (rgba != nullptr) SDL_DestroySurface(rgba);
                return nullptr;
            }

            if (textures.size() > 700)
            {
                for (auto it = textures.begin(); it != textures.end();)
                {
                    if (frame - it->second.usedFrame > 120)
                    {
                        glDeleteTextures(1, &it->second.texture);
                        it = textures.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
            }

            CachedTexture cached;
            cached.width = rgba->w;
            cached.height = rgba->h;
            cached.usedFrame = frame;
            glGenTextures(1, &cached.texture);
            glBindTexture(GL_TEXTURE_2D, cached.texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h,
                0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
            SDL_DestroySurface(rgba);

            auto [inserted, _] = textures.emplace(key, cached);
            return &inserted->second;
        }

        CachedTexture* IconTexture(const std::string& id)
        {
            if (id.empty() || id.size() > 80 ||
                id.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") !=
                    std::string::npos)
                return nullptr;
            if (auto found = iconTextures.find(id); found != iconTextures.end())
                return &found->second;
            if (missingIcons.contains(id))
                return nullptr;
            const std::filesystem::path filename = id + ".bmp";
            std::filesystem::path location =
                std::filesystem::path("assets") / "icons" / filename;
            std::error_code ec;
            if (!std::filesystem::is_regular_file(location, ec))
            {
                const char* base = SDL_GetBasePath();
                if (base)
                    location = std::filesystem::path(base) /
                        "assets" / "icons" / filename;
            }
            SDL_Surface* loaded = SDL_LoadBMP(location.string().c_str());
            if (!loaded)
            {
                missingIcons[id] = true;
                return nullptr;
            }
            SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(loaded);
            if (!rgba || rgba->w < 1 || rgba->h < 1)
            {
                if (rgba) SDL_DestroySurface(rgba);
                missingIcons[id] = true;
                return nullptr;
            }
            CachedTexture icon;
            icon.width = rgba->w;
            icon.height = rgba->h;
            glGenTextures(1, &icon.texture);
            glBindTexture(GL_TEXTURE_2D, icon.texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels);
            SDL_DestroySurface(rgba);
            auto [it, _] = iconTextures.emplace(id, icon);
            return &it->second;
        }

        void DrawQuad(const DrawCall& call, int textureMode, GLuint texture)
        {
            const float left = call.x * 2.0f / static_cast<float>(viewportWidth) - 1.0f;
            const float right = (call.x + call.width) * 2.0f / static_cast<float>(viewportWidth) - 1.0f;
            const float top = 1.0f - call.y * 2.0f / static_cast<float>(viewportHeight);
            const float bottom = 1.0f - (call.y + call.height) * 2.0f / static_cast<float>(viewportHeight);
            const auto& c = call.color;
            const std::array<float, 48> vertices{{
                left, top, 0, 0, c.r, c.g, c.b, c.a,
                right, top, 1, 0, c.r, c.g, c.b, c.a,
                right, bottom, 1, 1, c.r, c.g, c.b, c.a,
                left, top, 0, 0, c.r, c.g, c.b, c.a,
                right, bottom, 1, 1, c.r, c.g, c.b, c.a,
                left, bottom, 0, 1, c.r, c.g, c.b, c.a
            }};
            gl.uniform1i(useTextureLocation, textureMode);
            glBindTexture(GL_TEXTURE_2D, textureMode != 0 ? texture : 0);
            gl.bufferData(0x8892, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                vertices.data(), 0x88E0);
            gl.drawArrays(GL_TRIANGLES, 0, 6);
        }

        void Render()
        {
            if (calls.empty() || program == 0)
            {
                calls.clear();
                return;
            }
            ++frame;
            GLint oldProgram = 0;
            GLint oldArray = 0;
            GLint oldBuffer = 0;
            GLint oldTexture = 0;
            GLint oldActiveTexture = 0;
            GLint oldUnpackAlignment = 4;
            glGetIntegerv(0x8B8D, &oldProgram);
            glGetIntegerv(0x85B5, &oldArray);
            glGetIntegerv(0x8894, &oldBuffer);
            glGetIntegerv(0x84E0, &oldActiveTexture);
            gl.activeTexture(0x84C0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &oldUnpackAlignment);
            const GLboolean oldDepth = glIsEnabled(GL_DEPTH_TEST);
            const GLboolean oldCull = glIsEnabled(GL_CULL_FACE);
            const GLboolean oldBlend = glIsEnabled(GL_BLEND);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            gl.useProgram(program);
            gl.uniform1i(textureLocation, 0);
            gl.bindVertexArray(vertexArray);
            gl.bindBuffer(0x8892, vertexBuffer);
            for (const DrawCall& call : calls)
            {
                if (call.image)
                {
                    if (CachedTexture* texture = IconTexture(call.content))
                        DrawQuad(call, 2, texture->texture);
                }
                else if (call.text)
                {
                    if (CachedTexture* texture = TextureFor(call.content, call.fontSize))
                    {
                        DrawCall adjusted = call;
                        adjusted.width = static_cast<float>(texture->width);
                        adjusted.height = static_cast<float>(texture->height);
                        DrawQuad(adjusted, 1, texture->texture);
                    }
                }
                else if (call.width > 0.0f && call.height > 0.0f)
                {
                    DrawQuad(call, 0, 0);
                }
            }
            glPixelStorei(GL_UNPACK_ALIGNMENT, oldUnpackAlignment);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
            gl.activeTexture(static_cast<GLenum>(oldActiveTexture));
            gl.bindBuffer(0x8892, static_cast<GLuint>(oldBuffer));
            gl.bindVertexArray(static_cast<GLuint>(oldArray));
            gl.useProgram(static_cast<GLuint>(oldProgram));
            if (oldDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
            if (oldCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
            if (oldBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
            calls.clear();
        }
    };

    UIRenderer::UIRenderer() : m_impl(std::make_unique<Impl>()) {}
    UIRenderer::~UIRenderer() { Shutdown(); }

    bool UIRenderer::Initialize()
    {
        if (m_impl->font != nullptr)
        {
            return true;
        }
        if (!TTF_Init())
        {
            std::cerr << "SDL3_ttf initialization failed: " << SDL_GetError() << '\n';
            return false;
        }
        m_impl->ownsTtf = true;
        const auto fontPath = LocateFont();
        if (fontPath.empty())
        {
            std::cerr << "No supported system UI font installed. Install Segoe UI, DejaVu Sans, or Liberation Sans.\n";
            Shutdown();
            return false;
        }
        m_impl->font = TTF_OpenFont(fontPath.string().c_str(), 14.0f);
        if (m_impl->font == nullptr)
        {
            std::cerr << "Unable to load system font: " << fontPath << " : " << SDL_GetError() << '\n';
            Shutdown();
            return false;
        }
        m_impl->currentFontSize = 14;
        if (!m_impl->LoadGL() || !m_impl->SetupShader())
        {
            std::cerr << "UI OpenGL shader setup failed.\n";
            Shutdown();
            return false;
        }
        std::cout << "UI font loaded: " << fontPath.filename().string() << " (anti-aliased system font)\n";
        return true;
    }

    void UIRenderer::Shutdown()
    {
        if (!m_impl)
        {
            return;
        }
        for (const auto& [key, cached] : m_impl->textures)
        {
            (void)key;
            if (cached.texture != 0) glDeleteTextures(1, &cached.texture);
        }
        m_impl->textures.clear();
        for (const auto& [id, cached] : m_impl->iconTextures)
        {
            (void)id;
            if (cached.texture != 0) glDeleteTextures(1, &cached.texture);
        }
        m_impl->iconTextures.clear();
        m_impl->missingIcons.clear();
        m_impl->measured.clear();
        if (m_impl->vertexBuffer != 0) m_impl->gl.deleteBuffers(1, &m_impl->vertexBuffer);
        if (m_impl->vertexArray != 0) m_impl->gl.deleteVertexArrays(1, &m_impl->vertexArray);
        if (m_impl->program != 0) m_impl->gl.deleteProgram(m_impl->program);
        m_impl->vertexBuffer = 0;
        m_impl->vertexArray = 0;
        m_impl->program = 0;
        if (m_impl->font != nullptr) TTF_CloseFont(m_impl->font);
        m_impl->font = nullptr;
        m_impl->currentFontSize = 0;
        if (m_impl->ownsTtf) TTF_Quit();
        m_impl->ownsTtf = false;
    }

    void UIRenderer::BeginFrame(int width, int height)
    {
        m_impl->viewportWidth = std::max(1, width);
        m_impl->viewportHeight = std::max(1, height);
        m_impl->calls.clear();
    }

    void UIRenderer::EndFrame()
    {
        m_impl->Render();
    }

    void UIRenderer::DrawIcon(std::string_view iconId, float x, float y,
        float width, float height)
    {
        if (iconId.empty() || width <= 0.0f || height <= 0.0f)
            return;
        Impl::DrawCall call;
        call.image = true;
        call.content = std::string(iconId);
        call.x = x;
        call.y = y;
        call.width = width;
        call.height = height;
        call.color = UIColor{1.0f, 1.0f, 1.0f, 1.0f};
        m_impl->calls.push_back(std::move(call));
    }

    void UIRenderer::DrawRectangle(float x, float y, float width, float height, UIColor color)
    {
        if (width <= 0.0f || height <= 0.0f)
        {
            return;
        }
        Impl::DrawCall call;
        call.x = x;
        call.y = y;
        call.width = width;
        call.height = height;
        call.color = color;
        m_impl->calls.push_back(std::move(call));
    }

    void UIRenderer::DrawText(std::string_view text, float x, float y, float scale, UIColor color)
    {
        if (text.empty())
        {
            return;
        }
        Impl::DrawCall call;
        call.text = true;
        call.content = std::string(text);
        call.x = x;
        call.y = y;
        call.fontSize = FontPixels(scale);
        call.color = color;
        m_impl->calls.push_back(std::move(call));
    }

    float UIRenderer::MeasureText(std::string_view text, float scale)
    {
        if (text.empty())
        {
            return 0.0f;
        }
        return static_cast<float>(m_impl->Measure(std::string(text), FontPixels(scale)).first);
    }

    float UIRenderer::TextHeight(float scale) const
    {
        return static_cast<float>(FontPixels(scale));
    }
}
