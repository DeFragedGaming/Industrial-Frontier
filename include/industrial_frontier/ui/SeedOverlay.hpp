#pragma once

#include "industrial_frontier/ui/UIRenderer.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>

namespace industrial_frontier
{
    enum class SeedUiAction
    {
        None,
        Create,
        Cancel
    };

    struct SeedUiResult
    {
        SeedUiAction action = SeedUiAction::None;
        int seed = 0;
    };

    class SeedOverlay
    {
    public:
        void Open();
        void Close();
        bool IsOpen() const;
        void TextInput(const char* text);
        SeedUiResult KeyDown(SDL_Scancode key);
        SeedUiResult Click(int width, int height, float x, float y);
        void Render(UIRenderer& ui, int width, int height) const;
        const std::string& GetSeedText() const;

    private:
        bool m_open = false;
        std::string m_seedText;
        std::string m_message;

        void Randomize();
        SeedUiResult TryCreate();
    };
}
