#include "industrial_frontier/ui/SaveOverlay.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>

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
        };
        GLFns glf;

        template <typename T>
        bool Load(T& result, const char* name)
        {
            result = reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
            return result != nullptr;
        }

        using Glyph = std::array<unsigned char, 7>;
        Glyph Letter(char c)
        {
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
                case '5': return {31,16,30,1,1,17,14};
                case '6': return {6,8,16,30,17,17,14};
                case '7': return {31,1,2,4,8,8,8};
                case '8': return {14,17,17,14,17,17,14};
                case '9': return {14,17,17,15,1,2,12};
                case '-': return {0,0,0,31,0,0,0};
                case '_': return {0,0,0,0,0,0,31};
                case '.': return {0,0,0,0,0,12,12};
                case ':': return {0,12,12,0,12,12,0};
                case '/': return {1,2,2,4,8,8,16};
                case '!': return {4,4,4,4,4,0,4};
                case '?': return {14,17,1,2,4,0,4};
                case '[': return {14,8,8,8,8,8,14};
                case ']': return {14,2,2,2,2,2,14};
                case '#': return {10,10,31,10,31,10,10};
                default: return {0,0,0,0,0,0,0};
            }
        }
    }

    bool SaveOverlay::SetupGL()
    {
        bool ok = true;
        ok &= Load(glf.createShader, "glCreateShader");
        ok &= Load(glf.shaderSource, "glShaderSource");
        ok &= Load(glf.compileShader, "glCompileShader");
        ok &= Load(glf.getShaderiv, "glGetShaderiv");
        ok &= Load(glf.getShaderInfoLog, "glGetShaderInfoLog");
        ok &= Load(glf.deleteShader, "glDeleteShader");
        ok &= Load(glf.createProgram, "glCreateProgram");
        ok &= Load(glf.attachShader, "glAttachShader");
        ok &= Load(glf.linkProgram, "glLinkProgram");
        ok &= Load(glf.getProgramiv, "glGetProgramiv");
        ok &= Load(glf.getProgramInfoLog, "glGetProgramInfoLog");
        ok &= Load(glf.deleteProgram, "glDeleteProgram");
        ok &= Load(glf.useProgram, "glUseProgram");
        ok &= Load(glf.genVertexArrays, "glGenVertexArrays");
        ok &= Load(glf.bindVertexArray, "glBindVertexArray");
        ok &= Load(glf.deleteVertexArrays, "glDeleteVertexArrays");
        ok &= Load(glf.genBuffers, "glGenBuffers");
        ok &= Load(glf.bindBuffer, "glBindBuffer");
        ok &= Load(glf.bufferData, "glBufferData");
        ok &= Load(glf.deleteBuffers, "glDeleteBuffers");
        ok &= Load(glf.enableVertexAttribArray, "glEnableVertexAttribArray");
        ok &= Load(glf.vertexAttribPointer, "glVertexAttribPointer");
        ok &= Load(glf.drawArrays, "glDrawArrays");
        if (!ok)
            return false;

        constexpr const char* vertexSource =
            "#version 330 core\n"
            "layout(location=0) in vec2 position;\n"
            "layout(location=1) in vec4 color;\n"
            "out vec4 vColor;\n"
            "void main(){vColor=color;gl_Position=vec4(position,0.0,1.0);}\n";
        constexpr const char* fragmentSource =
            "#version 330 core\n"
            "in vec4 vColor;out vec4 FragColor;\n"
            "void main(){FragColor=vColor;}\n";

        auto compile = [](GLenum type, const char* code) -> GLuint
        {
            GLuint shader = glf.createShader(type);
            glf.shaderSource(shader, 1, &code, nullptr);
            glf.compileShader(shader);
            GLint success = 0;
            glf.getShaderiv(shader, 0x8B81, &success);
            if (!success)
            {
                char log[2048]{};
                glf.getShaderInfoLog(shader, 2048, nullptr, log);
                std::cerr << "Save UI shader error: " << log << '\n';
                glf.deleteShader(shader);
                return 0;
            }
            return shader;
        };
        GLuint vs = compile(0x8B31, vertexSource);
        GLuint fs = compile(0x8B30, fragmentSource);
        if (!vs || !fs)
        {
            if (vs) glf.deleteShader(vs);
            if (fs) glf.deleteShader(fs);
            return false;
        }
        m_program = glf.createProgram();
        glf.attachShader(m_program, vs);
        glf.attachShader(m_program, fs);
        glf.linkProgram(m_program);
        GLint linked = 0;
        glf.getProgramiv(m_program, 0x8B82, &linked);
        glf.deleteShader(vs);
        glf.deleteShader(fs);
        if (!linked)
        {
            char log[2048]{};
            glf.getProgramInfoLog(m_program, 2048, nullptr, log);
            std::cerr << "Save UI link error: " << log << '\n';
            glf.deleteProgram(m_program);
            m_program = 0;
            return false;
        }
        glf.genVertexArrays(1, &m_vertexArray);
        glf.genBuffers(1, &m_vertexBuffer);
        glf.bindVertexArray(m_vertexArray);
        glf.bindBuffer(0x8892, m_vertexBuffer);
        glf.enableVertexAttribArray(0);
        glf.vertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
        glf.enableVertexAttribArray(1);
        glf.vertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                                reinterpret_cast<const void*>(2 * sizeof(float)));
        glf.bindVertexArray(0);
        glf.bindBuffer(0x8892, 0);
        return true;
    }

    bool SaveOverlay::Initialize()
    {
        return SetupGL();
    }

    void SaveOverlay::Shutdown()
    {
        if (m_vertexBuffer) glf.deleteBuffers(1, &m_vertexBuffer);
        if (m_vertexArray) glf.deleteVertexArrays(1, &m_vertexArray);
        if (m_program) glf.deleteProgram(m_program);
        m_vertexBuffer = 0;
        m_vertexArray = 0;
        m_program = 0;
    }

    void SaveOverlay::OpenSave()
    {
        m_mode = Mode::Save;
        m_name.clear();
        m_message.clear();
        DismissDeleteConfirmation();
    }

    void SaveOverlay::OpenLoad()
    {
        m_mode = Mode::Load;
        m_page = 0;
        m_message.clear();
        DismissDeleteConfirmation();
    }

    void SaveOverlay::Close()
    {
        m_mode = Mode::Closed;
        DismissDeleteConfirmation();
    }

    bool SaveOverlay::IsOpen() const
    {
        return m_mode != Mode::Closed;
    }

    bool SaveOverlay::IsNaming() const
    {
        return m_mode == Mode::Save;
    }

    void SaveOverlay::DismissDeleteConfirmation()
    {
        m_confirmDelete = false;
        m_deleteSlotId.clear();
        m_deleteSlotName.clear();
    }

    void SaveOverlay::SetMessage(std::string message)
    {
        m_message = std::move(message);
    }

    void SaveOverlay::TextInput(const char* value)
    {
        if (!value || !IsNaming())
            return;
        for (const unsigned char* ptr = reinterpret_cast<const unsigned char*>(value); *ptr; ++ptr)
        {
            if (*ptr >= 32 && *ptr < 127 && *ptr != '"' && *ptr != '\\' && m_name.size() < 36)
                m_name.push_back(static_cast<char>(*ptr));
        }
    }

    SaveUiResult SaveOverlay::KeyDown(SDL_Scancode key)
    {
        if (!IsOpen()) return {};
        if (m_confirmDelete)
        {
            if (key == SDL_SCANCODE_ESCAPE)
                DismissDeleteConfirmation();
            return {};
        }
        if (key == SDL_SCANCODE_ESCAPE)
            return {SaveUiAction::Close};
        if (IsNaming())
        {
            if (key == SDL_SCANCODE_BACKSPACE && !m_name.empty())
                m_name.pop_back();
            if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER)
                return {SaveUiAction::SaveNamed, {}, m_name};
        }
        return {};
    }

    bool SaveOverlay::Inside(float x, float y, float left, float top,
                             float width, float height) const
    {
        return x >= left && y >= top && x <= left + width && y <= top + height;
    }

    SaveUiResult SaveOverlay::Click(int width, int height, float x, float y,
                                    bool, bool, bool,
                                    const std::vector<SaveSlotInfo>& slots)
    {
        if (!IsOpen() || width < 350 || height < 320)
            return {};

        const float panelWidth = std::min(750.0f, static_cast<float>(width) - 24.0f);
        const float panelHeight = std::min(520.0f, static_cast<float>(height) - 24.0f);
        const float left = (static_cast<float>(width) - panelWidth) * 0.5f;
        const float top = (static_cast<float>(height) - panelHeight) * 0.5f;

        if (m_confirmDelete)
        {
            const float boxLeft = left + (panelWidth - 470.0f) * 0.5f;
            const float boxTop = top + (panelHeight - 216.0f) * 0.5f;
            if (Inside(x, y, boxLeft + 238.0f, boxTop + 155.0f, 199.0f, 40.0f))
            {
                DismissDeleteConfirmation();
                return {};
            }
            if (Inside(x, y, boxLeft + 25.0f, boxTop + 155.0f, 199.0f, 40.0f))
            {
                const std::string id = m_deleteSlotId;
                DismissDeleteConfirmation();
                return {SaveUiAction::DeleteSlot, id};
            }
            return {};
        }

        if (Inside(x, y, left + panelWidth - 112.0f, top + 18.0f, 95.0f, 38.0f))
            return {SaveUiAction::Close};

        if (m_mode == Mode::Save)
        {
            if (Inside(x, y, left + 24.0f, top + 188.0f, 172.0f, 42.0f))
                return {SaveUiAction::SaveNamed, {}, m_name};
            return {};
        }

        const std::size_t perPage = static_cast<std::size_t>(std::clamp(
            static_cast<int>((panelHeight - 194.0f) / 57.0f) + 1, 1, 6));
        const int maxPage = slots.empty() ? 0 :
            static_cast<int>((slots.size() - 1) / perPage);
        m_page = std::clamp(m_page, 0, maxPage);
        const std::size_t start = static_cast<std::size_t>(m_page) * perPage;
        for (std::size_t i = 0; i < perPage && start + i < slots.size(); ++i)
        {
            const float rowTop = top + 90.0f + static_cast<float>(i) * 57.0f;
            const auto& slot = slots[start + i];
            if (Inside(x, y, left + panelWidth - 327.0f, rowTop + 7.0f, 65.0f, 36.0f))
                return {SaveUiAction::LoadSlot, slot.id, {}, 0};
            if (slot.backupAvailable &&
                Inside(x, y, left + panelWidth - 254.0f, rowTop + 7.0f, 68.0f, 36.0f))
                return {SaveUiAction::LoadSlot, slot.id, {}, 1};
            if (slot.olderBackupAvailable &&
                Inside(x, y, left + panelWidth - 178.0f, rowTop + 7.0f, 68.0f, 36.0f))
                return {SaveUiAction::LoadSlot, slot.id, {}, 2};
            if (Inside(x, y, left + panelWidth - 102.0f, rowTop + 7.0f, 84.0f, 36.0f))
            {
                m_deleteSlotId = slot.id;
                m_deleteSlotName = slot.name;
                m_confirmDelete = true;
                return {};
            }
        }

        const float footerY = top + panelHeight - 57.0f;
        if (Inside(x, y, left + 24.0f, footerY, 96.0f, 38.0f) && m_page > 0)
            --m_page;
        if (Inside(x, y, left + 132.0f, footerY, 96.0f, 38.0f) &&
            start + perPage < slots.size())
            ++m_page;
        return {};
    }

    void SaveOverlay::AddRect(int width, int height, float x, float y,
                              float w, float h, float r, float g, float b, float a)
    {
        if (w <= 0 || h <= 0 || width <= 0 || height <= 0)
            return;
        const float left = 2.0f*x/width-1.0f;
        const float right = 2.0f*(x+w)/width-1.0f;
        const float top = 1.0f-2.0f*y/height;
        const float bottom = 1.0f-2.0f*(y+h)/height;
        auto add = [this,r,g,b,a](float px,float py)
        {
            m_vertices.insert(m_vertices.end(), {px,py,r,g,b,a});
        };
        add(left,top); add(right,top); add(right,bottom);
        add(left,top); add(right,bottom); add(left,bottom);
    }

    void SaveOverlay::AddText(int width, int height, float x, float y,
                              const std::string& text, int scale,
                              float r, float g, float b, int maxChars)
    {
        int count = 0;
        for (unsigned char original : text)
        {
            if (count >= maxChars) break;
            const char c = (original >= 'a' && original <= 'z')
                ? static_cast<char>(original - 'a' + 'A')
                : static_cast<char>(original);
            const Glyph rows = Letter(c);
            for (int row = 0; row < 7; ++row)
                for (int col = 0; col < 5; ++col)
                    if ((rows[static_cast<std::size_t>(row)] & (1u << (4-col))) != 0)
                        AddRect(width, height, x+col*scale, y+row*scale,
                                static_cast<float>(scale),static_cast<float>(scale),r,g,b,1.0f);
            x += scale * 6;
            ++count;
        }
    }

    void SaveOverlay::DrawButton(int width,int height,float x,float y,float w,float h,
                                 const std::string& text,bool enabled)
    {
        AddRect(width,height,x,y,w,h,enabled?0.18f:0.12f,
                enabled?0.28f:0.15f,enabled?0.37f:0.19f,0.96f);
        AddRect(width,height,x,y,w,2,enabled?0.86f:0.28f,
                enabled?0.61f:0.30f,enabled?0.28f:0.33f,1.0f);
        AddText(width,height,x+12,y+(h-14)*0.5f,text,2,
                enabled?0.96f:0.55f,enabled?0.96f:0.57f,enabled?0.98f:0.60f,
                static_cast<int>((w-18)/12));
    }

    void SaveOverlay::Render(int width,int height,bool menuAvailable,
                             bool, bool,
                             const std::vector<SaveSlotInfo>& slots)
    {
        if (width <= 0 || height <= 0 || !m_program) return;
        m_vertices.clear();
        if (m_mode == Mode::Closed)
        {
            if (!menuAvailable || m_message.empty()) return;
            AddRect(width,height,15,static_cast<float>(height)-42,
                    static_cast<float>(width)-30,28,0.08f,0.10f,0.15f,0.92f);
            AddText(width,height,28,static_cast<float>(height)-35,
                    m_message,2,1.0f,0.75f,0.42f,65);
        }
        else if (width >= 350 && height >= 320)
        {
            AddRect(width,height,0,0,static_cast<float>(width),static_cast<float>(height),
                    0.0f,0.0f,0.0f,0.72f);
            const float pw = std::min(750.0f, static_cast<float>(width)-24.0f);
            const float ph = std::min(520.0f, static_cast<float>(height)-24.0f);
            const float left = (static_cast<float>(width)-pw)*0.5f;
            const float top = (static_cast<float>(height)-ph)*0.5f;
            AddRect(width,height,left,top,pw,ph,0.075f,0.09f,0.12f,0.99f);
            AddRect(width,height,left,top,pw,4,0.85f,0.60f,0.26f,1.0f);
            AddText(width,height,left+24,top+26,
                    m_mode==Mode::Save?"CREATE NAMED SAVE":"LOAD SAVED WORLD",
                    3,0.97f,0.97f,0.97f,26);
            DrawButton(width,height,left+pw-112,top+18,95,38,"CLOSE",true);
            if (m_mode == Mode::Save)
            {
                AddText(width,height,left+24,top+92,"ENTER A NAME FOR THIS SNAPSHOT",2,
                        0.77f,0.82f,0.86f,35);
                AddRect(width,height,left+24,top+130,pw-48,42,0.13f,0.17f,0.23f,1.0f);
                AddText(width,height,left+36,top+143,m_name+"_",2,1,1,1,37);
                DrawButton(width,height,left+24,top+188,172,42,"SAVE NEW SLOT",true);
                AddText(width,height,left+24,top+262,"MANUAL SAVES ARE NEVER OVERWRITTEN",2,
                        0.79f,0.80f,0.83f,42);
                AddText(width,height,left+24,top+290,"AUTOSAVE RETAINS TWO OLDER BACKUPS",2,
                        0.79f,0.80f,0.83f,42);
                AddText(width,height,left+24,top+ph-76,"ENTER TO SAVE - ESC TO CANCEL",2,
                        0.84f,0.65f,0.37f,40);
            }
            else
            {
                const std::size_t perPage = static_cast<std::size_t>(std::clamp(
                    static_cast<int>((ph-194.0f)/57.0f)+1,1,6));
                const int maxPage = slots.empty()?0:
                    static_cast<int>((slots.size()-1)/perPage);
                m_page = std::clamp(m_page,0,maxPage);
                const std::size_t start = static_cast<std::size_t>(m_page)*perPage;
                if (slots.empty())
                    AddText(width,height,left+24,top+132,"NO SAVE FILES FOUND",2,1,0.75f,0.44f);
                for (std::size_t i=0;i<perPage && start+i<slots.size();++i)
                {
                    const auto& slot=slots[start+i];
                    const float ry=top+90+static_cast<float>(i)*57;
                    AddRect(width,height,left+15,ry,pw-30,53,0.12f,0.15f,0.20f,0.96f);
                    AddText(width,height,left+25,ry+17,slot.name,2,0.95f,0.95f,0.97f,
                            std::max(1,static_cast<int>((pw-355)/12)));
                    DrawButton(width,height,left+pw-327,ry+7,65,36,"LOAD",true);
                    DrawButton(width,height,left+pw-254,ry+7,68,36,"BAK 1",slot.backupAvailable);
                    DrawButton(width,height,left+pw-178,ry+7,68,36,"BAK 2",slot.olderBackupAvailable);
                    DrawButton(width,height,left+pw-102,ry+7,84,36,"DELETE",true);
                }
                DrawButton(width,height,left+24,top+ph-57,96,38,"PREV",m_page>0);
                DrawButton(width,height,left+132,top+ph-57,96,38,"NEXT",start+perPage<slots.size());
                AddText(width,height,left+252,top+ph-45,"DELETE IS PERMANENT",2,
                        0.87f,0.65f,0.40f,30);
            }
            if (!m_message.empty())
                AddText(width,height,left+24,top+ph-76,m_message,2,1.0f,0.71f,0.42f,48);

            if (m_confirmDelete)
            {
                AddRect(width,height,left,top,pw,ph,0.0f,0.0f,0.0f,0.84f);
                const float boxLeft = left+(pw-470.0f)*0.5f;
                const float boxTop = top+(ph-216.0f)*0.5f;
                AddRect(width,height,boxLeft,boxTop,470,216,0.095f,0.11f,0.15f,1.0f);
                AddRect(width,height,boxLeft,boxTop,470,4,0.91f,0.32f,0.21f,1.0f);
                AddText(width,height,boxLeft+25,boxTop+23,"DELETE THIS WORLD?",3,
                        1.0f,0.65f,0.52f,23);
                AddText(width,height,boxLeft+25,boxTop+75,m_deleteSlotName,2,
                        0.95f,0.95f,0.97f,35);
                AddText(width,height,boxLeft+25,boxTop+109,"SAVE AND BOTH BACKUPS WILL BE LOST",2,
                        1.0f,0.67f,0.49f,37);
                DrawButton(width,height,boxLeft+25,boxTop+155,199,40,"DELETE FOREVER",true);
                DrawButton(width,height,boxLeft+238,boxTop+155,199,40,"CANCEL",true);
            }
        }
        if (m_vertices.empty()) return;

        GLint oldProgram=0;
        GLint oldVAO=0;
        GLint oldBuffer=0;
        glGetIntegerv(0x8B8D,&oldProgram);
        glGetIntegerv(0x85B5,&oldVAO);
        glGetIntegerv(0x8894,&oldBuffer);
        const GLboolean oldDepth=glIsEnabled(GL_DEPTH_TEST);
        const GLboolean oldCull=glIsEnabled(GL_CULL_FACE);
        const GLboolean oldBlend=glIsEnabled(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glf.useProgram(m_program);
        glf.bindVertexArray(m_vertexArray);
        glf.bindBuffer(0x8892,m_vertexBuffer);
        glf.bufferData(0x8892,static_cast<GLsizeiptr>(m_vertices.size()*sizeof(float)),
                       m_vertices.data(),0x88E0);
        glf.drawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(m_vertices.size()/6));
        glf.bindBuffer(0x8892,static_cast<GLuint>(oldBuffer));
        glf.bindVertexArray(static_cast<GLuint>(oldVAO));
        glf.useProgram(static_cast<GLuint>(oldProgram));
        if (oldDepth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (oldCull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        if (oldBlend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    }
}
