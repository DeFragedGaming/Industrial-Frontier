#pragma once

#include "industrial_frontier/resources/FieldGuideCatalog.hpp"
#include "industrial_frontier/ui/FieldGuideResourceFilter.hpp"
#include "industrial_frontier/resources/MineralDatabase.hpp"
#include "industrial_frontier/ui/UIRenderer.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <string>
#include <vector>

namespace industrial_frontier
{
    enum class FieldGuideSection
    {
        Resources,
        Survival,
        Metallurgy,
        Processing,
        Machines
    };

    class FieldGuideOverlay
    {
    public:
        void Reset();
        void KeyDown(SDL_Scancode key, const MineralDatabase& minerals,
                     const FieldGuideCatalog& routes);
        void TextInput(const char* text);
        void Wheel(float direction, const MineralDatabase& minerals,
                   const FieldGuideCatalog& routes);
        bool Click(float x, float y, int width, int height,
                   const MineralDatabase& minerals,
                   const FieldGuideCatalog& routes);
        void Render(UIRenderer& ui, const MineralDatabase& minerals,
                    const FieldGuideCatalog& routes,
                    int width, int height) const;
        bool SearchFocused() const;
        const std::string& Query() const;
        FieldGuideResourceFilter ResourceFilter() const;

    private:
        struct Entry
        {
            std::string title;
            const MineralDefinition* mineral = nullptr;
            const GuideProcessingRoute* route = nullptr;
            std::size_t machineIndex = 0;
            std::string metalGroup;
        };

        std::vector<Entry> Filtered(const MineralDatabase& minerals,
                                    const FieldGuideCatalog& routes) const;
        void ChangeSection(FieldGuideSection section);
        void MoveSelection(int amount, const MineralDatabase& minerals,
                           const FieldGuideCatalog& routes);

        FieldGuideSection m_section = FieldGuideSection::Survival;
        FieldGuideResourceFilter m_resourceFilter = FieldGuideResourceFilter::AllResources;
        std::size_t m_selected = 0;
        std::string m_query;
        bool m_searchFocused = false;
    };
}
