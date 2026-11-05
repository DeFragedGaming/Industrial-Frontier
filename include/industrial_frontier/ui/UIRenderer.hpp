
#pragma once

#include <cstdint>
#include <string_view>

// Windows headers can define DrawText as DrawTextA or DrawTextW.
// Remove that macro so our OpenGL UI method retains its name.
#ifdef DrawText
#undef DrawText
#endif

namespace industrial_frontier
{
    struct UIColor
    {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;
    };

    class UIRenderer
    {
    public:
        UIRenderer() = default;
        ~UIRenderer();

        UIRenderer(const UIRenderer&) = delete;
        UIRenderer& operator=(const UIRenderer&) = delete;

        bool Initialize();
        void Shutdown();

        void BeginFrame(int width, int height);
        void EndFrame();

        void DrawRectangle(
            float x,
            float y,
            float width,
            float height,
            const UIColor& color
        );

        void DrawText(
            std::string_view text,
            float x,
            float y,
            float scale,
            const UIColor& color
        );

        float MeasureText(
            std::string_view text,
            float scale
        ) const;

        bool IsInitialized() const;

    private:
        void DrawGlyph(
            char character,
            float x,
            float y,
            float scale,
            const UIColor& color
        );

        void DrawQuad(
            float x,
            float y,
            float width,
            float height,
            const UIColor& color
        );

        unsigned int m_program = 0;
        unsigned int m_vertexArray = 0;
        unsigned int m_vertexBuffer = 0;

        int m_viewportWidth = 1;
        int m_viewportHeight = 1;

        int m_viewportUniform = -1;
        int m_colorUniform = -1;

        bool m_initialized = false;
    };
}
