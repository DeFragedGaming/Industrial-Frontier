#include "industrial_frontier/resources/FieldGuideCatalog.hpp"
#include "industrial_frontier/resources/MineralDatabase.hpp"
#include "industrial_frontier/ui/FieldGuideOverlay.hpp"
#include "industrial_frontier/ui/UIRenderer.hpp"

#include <SDL3/SDL.h>
#include "industrial_frontier/ui/FieldGuideResourceFilter.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace industrial_frontier
{
    struct UIRenderer::Impl {};

    UIRenderer::UIRenderer() = default;
    UIRenderer::~UIRenderer() = default;

    void UIRenderer::DrawRectangle(float, float, float, float, UIColor) {}
    void UIRenderer::DrawText(std::string_view, float, float, float, UIColor) {}
    float UIRenderer::MeasureText(std::string_view value, float scale)
    {
        return static_cast<float>(value.size()) * 7.0f * scale;
    }
}

namespace
{
    void Check(bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3, "Expected mineral database and guide TSV paths");
        industrial_frontier::MineralDatabase minerals;
        industrial_frontier::FieldGuideCatalog guide;
        std::string error;
        Check(minerals.LoadFromFile(argv[1], error), error);
        Check(minerals.Count() == 93, "Mineral catalog count changed unexpectedly");
        std::size_t oreCount = 0;
        std::size_t copperCount = 0;
        for (const auto& mineral : minerals.All())
        {
            if (industrial_frontier::MatchesGuideResourceFilter(mineral,
                industrial_frontier::FieldGuideResourceFilter::AllOres))
                ++oreCount;
            if (industrial_frontier::MatchesGuideResourceFilter(mineral,
                industrial_frontier::FieldGuideResourceFilter::CopperOres))
                ++copperCount;
        }
        Check(oreCount == 86, "All Ores filter missing geological entries");
        Check(copperCount == 10, "Copper Ores filter missing real minerals");
        Check(industrial_frontier::MatchesGuideResourceFilter(
            *minerals.FindById("limonite"),
            industrial_frontier::FieldGuideResourceFilter::AllOres),
            "Limonite rock ore should appear in All Ores");
        Check(!industrial_frontier::MatchesGuideResourceFilter(
            *minerals.FindById("granite"),
            industrial_frontier::FieldGuideResourceFilter::AllOres),
            "Construction rock should not appear under All Ores");
        Check(guide.LoadFromFile(argv[2], minerals, error), error);
        Check(guide.Count() >= 40, "Missing processing route entries");

        industrial_frontier::FieldGuideOverlay overlay;
        industrial_frontier::UIRenderer headlessUI;
        overlay.Reset();
        overlay.KeyDown(SDL_SCANCODE_END, minerals, guide);
        overlay.Render(headlessUI, minerals, guide, 1280, 800);
        for (int tab = 0; tab < 5; ++tab)
        {
            overlay.KeyDown(SDL_SCANCODE_TAB, minerals, guide);
            overlay.KeyDown(SDL_SCANCODE_END, minerals, guide);
            overlay.Render(headlessUI, minerals, guide, 1280, 800);
        }
        overlay.Reset();
        overlay.Render(headlessUI, minerals, guide, 1280, 800);

        for (const auto& mineral : minerals.All())
        {
            const auto* route = guide.FindById(mineral.processingRouteId);
            Check(route != nullptr, "Missing route for: " + mineral.name);
            Check(!route->products.empty() && !route->process.empty(),
                "Empty route: " + route->id);
        }
        const auto* iron = minerals.FindById("hematite");
        Check(iron != nullptr, "Hematite is missing");
        const auto* ironRoute = guide.FindById(iron->processingRouteId);
        Check(ironRoute != nullptr &&
            ironRoute->products.find("iron") != std::string::npos,
            "Iron processing lookup failed");
        const auto* copper = minerals.FindById("chalcopyrite");
        Check(copper != nullptr, "Chalcopyrite is missing");
        const auto* copperRoute = guide.FindById(copper->processingRouteId);
        Check(copperRoute != nullptr &&
            copperRoute->products.find("copper") != std::string::npos,
            "Copper processing lookup failed");
        Check(!guide.Search("zinc").empty(), "Processing search failed");
        Check(guide.Search("no_such_chemical_route_xx").empty(),
            "Nonmatching search should be empty");

        const std::filesystem::path temporary =
            std::filesystem::temp_directory_path() / "industrial_frontier_guide_bad.tsv";
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            Check(static_cast<bool>(file), "Could not create temporary TSV");
            file << "route_id\ttitle\tproducts\tprocess\tnotes\n";
            file << "iron_oxide\tIron\tIron\tSmelting\tExample\n";
            file << "iron_oxide\tIron\tIron\tSmelting\tDuplicate\n";
        }
        Check(!guide.LoadFromFile(temporary, minerals, error),
            "Duplicate route was accepted");
        Check(guide.Count() >= 40 && guide.FindById("iron_oxide") != nullptr,
            "Failed load must preserve previously loaded guide");
        std::filesystem::remove(temporary);

        std::cout << "Field Guide test passed: " << minerals.Count()
                  << " resources, " << guide.Count()
                  << " planned processing routes\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "Field Guide test FAILED: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
