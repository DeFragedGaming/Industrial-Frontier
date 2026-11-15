
#pragma once

#include <memory>
#include <string>
#include <string_view>

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
        UIRenderer();
        ~UIRenderer();

        UIRenderer(const UIRenderer&) = delete;
        UIRenderer& operator=(const UIRenderer&) = delete;

        bool Initialize();
        void Shutdown();
        void BeginFrame(int width, int height);
        void EndFrame();
        void DrawRectangle(float x, float y, float width, float height, UIColor color);
        void DrawIcon(std::string_view iconId, float x, float y,
            float width, float height);
        void DrawText(std::string_view text, float x, float y, float scale, UIColor color);
        float MeasureText(std::string_view text, float scale);
        float TextHeight(float scale) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
