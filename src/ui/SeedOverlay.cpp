#include "industrial_frontier/ui/SeedOverlay.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <random>
#include <string>

#ifdef DrawText
#undef DrawText
#endif

namespace industrial_frontier
{
    namespace
    {
        constexpr UIColor White{0.95f, 0.96f, 0.98f, 1.0f};
        constexpr UIColor Orange{0.95f, 0.57f, 0.19f, 1.0f};
        constexpr UIColor Muted{0.70f, 0.77f, 0.83f, 1.0f};
        constexpr UIColor Background{0.018f, 0.025f, 0.045f, 0.83f};
        constexpr UIColor Panel{0.055f, 0.08f, 0.115f, 0.98f};
        constexpr UIColor Field{0.10f, 0.15f, 0.20f, 1.0f};
        constexpr UIColor Button{0.15f, 0.21f, 0.27f, 1.0f};

        struct Rect
        {
            float x = 0.0f;
            float y = 0.0f;
            float w = 0.0f;
            float h = 0.0f;

            bool Contains(float px, float py) const
            {
                return px >= x && py >= y && px <= x + w && py <= y + h;
            }
        };

        struct Layout
        {
            Rect panel;
            Rect input;
            Rect randomize;
            Rect begin;
            Rect cancel;
        };

        Layout GetLayout(int width, int height)
        {
            const float w = static_cast<float>(std::max(width, 1));
            const float h = static_cast<float>(std::max(height, 1));
            const float pw = std::min(620.0f, w - 24.0f);
            const float ph = std::min(340.0f, h - 20.0f);
            const float x = (w - pw) * 0.5f;
            const float y = (h - ph) * 0.5f;
            const float pad = std::clamp(pw * 0.05f, 12.0f, 28.0f);
            const float buttonH = std::clamp(ph * 0.135f, 32.0f, 44.0f);
            const float buttonY = y + ph - buttonH - 25.0f;
            const float buttonW = (pw - 2.0f * pad - 20.0f) / 3.0f;
            const float fieldY = y + ph * 0.39f;
            return {
                {x, y, pw, ph},
                {x + pad, fieldY, pw - 2.0f * pad, 48.0f},
                {x + pad, buttonY, buttonW, buttonH},
                {x + pad + buttonW + 10.0f, buttonY, buttonW, buttonH},
                {x + pad + 2.0f * (buttonW + 10.0f), buttonY, buttonW, buttonH}
            };
        }

        void DrawCentered(UIRenderer& ui, const std::string& text,
            float centerX, float y, float scale, const UIColor& color)
        {
            ui.DrawText(text, centerX - ui.MeasureText(text, scale) * 0.5f,
                y, scale, color);
        }

        void DrawButton(UIRenderer& ui, const Rect& rect,
            const std::string& text, const UIColor& color)
        {
            ui.DrawRectangle(rect.x, rect.y, rect.w, rect.h, color);
            ui.DrawRectangle(rect.x, rect.y, rect.w, 2.0f, Orange);
            const float scale = 1.15f;
            DrawCentered(ui, text, rect.x + rect.w * 0.5f,
                rect.y + (rect.h - ui.TextHeight(scale)) * 0.5f,
                scale, White);
        }
    }

    void SeedOverlay::Randomize()
    {
        const std::uint64_t ticks = static_cast<std::uint64_t>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        std::random_device device;
        std::seed_seq sequence{
            device(), device(),
            static_cast<std::uint32_t>(ticks),
            static_cast<std::uint32_t>(ticks >> 32)
        };
        std::mt19937 engine(sequence);
        std::uniform_int_distribution<std::int32_t> distribution(
            1, std::numeric_limits<std::int32_t>::max());
        const std::string previous = m_seedText;
        std::int32_t result = distribution(engine);
        if (std::to_string(result) == previous)
        {
            result = result == std::numeric_limits<std::int32_t>::max()
                ? 1 : result + 1;
        }
        m_seedText = std::to_string(result);
        m_message.clear();
    }

    void SeedOverlay::Open()
    {
        m_open = true;
        Randomize();
    }

    void SeedOverlay::Close()
    {
        m_open = false;
        m_message.clear();
    }

    bool SeedOverlay::IsOpen() const
    {
        return m_open;
    }

    const std::string& SeedOverlay::GetSeedText() const
    {
        return m_seedText;
    }

    void SeedOverlay::TextInput(const char* text)
    {
        if (!m_open || !text)
        {
            return;
        }
        for (const unsigned char* p =
            reinterpret_cast<const unsigned char*>(text); *p; ++p)
        {
            const char ch = static_cast<char>(*p);
            if (ch == '-' && m_seedText.empty())
            {
                m_seedText.push_back(ch);
            }
            else if (ch >= '0' && ch <= '9' && m_seedText.size() < 11)
            {
                m_seedText.push_back(ch);
            }
        }
        m_message.clear();
    }

    SeedUiResult SeedOverlay::TryCreate()
    {
        int value = 0;
        const char* first = m_seedText.data();
        const char* last = first + m_seedText.size();
        const auto parsed = std::from_chars(first, last, value);
        if (m_seedText.empty() || parsed.ec != std::errc{} || parsed.ptr != last)
        {
            m_message = "Enter a valid 32-bit integer seed";
            return {};
        }
        return {SeedUiAction::Create, value};
    }

    SeedUiResult SeedOverlay::KeyDown(SDL_Scancode key)
    {
        if (!m_open)
        {
            return {};
        }
        if (key == SDL_SCANCODE_ESCAPE)
        {
            return {SeedUiAction::Cancel};
        }
        if (key == SDL_SCANCODE_BACKSPACE)
        {
            if (!m_seedText.empty())
            {
                m_seedText.pop_back();
            }
            m_message.clear();
        }
        if (key == SDL_SCANCODE_RETURN || key == SDL_SCANCODE_KP_ENTER)
        {
            return TryCreate();
        }
        return {};
    }

    SeedUiResult SeedOverlay::Click(int width, int height, float x, float y)
    {
        if (!m_open)
        {
            return {};
        }
        const Layout layout = GetLayout(width, height);
        if (layout.input.Contains(x, y))
        {
            m_seedText.clear();
            m_message = "Type a custom seed, then press CREATE";
        }
        else if (layout.randomize.Contains(x, y))
        {
            Randomize();
        }
        else if (layout.begin.Contains(x, y))
        {
            return TryCreate();
        }
        else if (layout.cancel.Contains(x, y))
        {
            return {SeedUiAction::Cancel};
        }
        return {};
    }

    void SeedOverlay::Render(UIRenderer& ui, int width, int height) const
    {
        if (!m_open || width <= 0 || height <= 0)
        {
            return;
        }
        const Layout layout = GetLayout(width, height);
        const float mid = layout.panel.x + layout.panel.w * 0.5f;
        ui.DrawRectangle(0.0f, 0.0f, static_cast<float>(width),
            static_cast<float>(height), Background);
        ui.DrawRectangle(layout.panel.x, layout.panel.y,
            layout.panel.w, layout.panel.h, Panel);
        ui.DrawRectangle(layout.panel.x, layout.panel.y,
            layout.panel.w, 4.0f, Orange);

        const float headingScale = layout.panel.w >= 420.0f ? 1.75f : 1.25f;
        DrawCentered(ui, "CREATE NEW WORLD", mid,
            layout.panel.y + 26.0f, headingScale, White);
        DrawCentered(ui, "Enter a seed or generate a new world",
            mid, layout.panel.y + 70.0f, 1.04f, Muted);

        ui.DrawRectangle(layout.input.x, layout.input.y,
            layout.input.w, layout.input.h, Field);
        ui.DrawRectangle(layout.input.x, layout.input.y,
            3.0f, layout.input.h, Orange);
        ui.DrawText(m_seedText + "_", layout.input.x + 15.0f,
            layout.input.y + 10.0f, 1.40f, White);

        DrawCentered(ui, "Same seed = same terrain, caves, ores, and trees",
            mid, layout.input.y + 62.0f, 0.96f, Muted);
        if (!m_message.empty())
        {
            DrawCentered(ui, m_message, mid,
                layout.input.y + 89.0f, 0.98f, Orange);
        }
        DrawButton(ui, layout.randomize, "RANDOMIZE", Button);
        DrawButton(ui, layout.begin, "CREATE", {0.26f, 0.30f, 0.18f, 1.0f});
        DrawButton(ui, layout.cancel, "CANCEL", Button);
    }
}
