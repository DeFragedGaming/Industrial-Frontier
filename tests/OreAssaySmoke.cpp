#include "industrial_frontier/resources/GeologyGenerator.hpp"
#include "industrial_frontier/resources/OreAssay.hpp"
#include "industrial_frontier/world/VoxelTerrain.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace industrial_frontier;

int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    double massFraction = 0.0;
    if (!TheoreticalElementFraction("Fe2O3", "Fe", massFraction) ||
        std::abs(massFraction - 0.6994) > 0.002) return 2;
    if (!TheoreticalElementFraction("Fe3O4", "Fe", massFraction) ||
        std::abs(massFraction - 0.7236) > 0.002) return 3;
    if (!TheoreticalElementFraction("CuFeS2", "Cu", massFraction) ||
        std::abs(massFraction - 0.3463) > 0.002) return 4;
    if (!TheoreticalElementFraction("Cu5FeS4", "Cu", massFraction) ||
        std::abs(massFraction - 0.6330) > 0.002) return 5;
    if (!TheoreticalElementFraction("FeO(OH)", "Fe", massFraction) ||
        std::abs(massFraction - 0.6285) > 0.003) return 6;
    if (!TheoreticalElementFraction("CaSO4\xC2\xB7" "2H2O", "H", massFraction) ||
        massFraction < 0.02 || massFraction > 0.025) return 7;
    if (TheoreticalElementFraction("(Fe,Ni)9S8", "Ni", massFraction)) return 8;
    if (TheoreticalElementFraction("", "Cu", massFraction)) return 9;

    std::string error;
    GeologyGenerator generator(12345);
    GeologyGenerator repeated(12345);
    if (!generator.LoadCatalog(argv[1], error) ||
        !repeated.LoadCatalog(argv[1], error))
    {
        std::cerr << error << '\n';
        return 10;
    }

    const auto deposits = generator.FindSurfaceDeposits(-1100, 1100, -1100, 1100);
    const auto repeats = repeated.FindSurfaceDeposits(-1100, 1100, -1100, 1100);
    if (deposits.size() != repeats.size() || deposits.empty()) return 11;
    bool foundMixed = false;
    bool foundChemicalGrade = false;
    for (std::size_t i = 0; i < deposits.size(); ++i)
    {
        const auto& d = deposits[i];
        const auto& other = repeats[i];
        if (d.id != other.id || d.components.size() != other.components.size() ||
            d.gangue.size() != other.gangue.size() || d.grade != other.grade ||
            d.hostGeology != other.hostGeology || d.hostGeology.empty()) return 12;
        const OreAssay assay = AnalyzeOre(d, generator);
        if (!assay.valid || assay.oreMineralFraction < 0.0f ||
            assay.oreMineralFraction > 1.0f) return 13;
        double massSum = 0.0;
        for (const auto& mineral : assay.minerals)
        {
            if (!generator.FindMineral(mineral.mineralId)) return 14;
            massSum += mineral.massFraction;
        }
        if (std::abs(massSum - 1.0) > 0.0002) return 15;
        if (std::abs(assay.oreMineralFraction +
            assay.gangueFraction - 1.0f) > 0.0001f) return 16;
        if (d.components.size() > 1) foundMixed = true;
        for (const auto& element : assay.elements)
        {
            if (element.massFraction < -0.00001f ||
                element.massFraction > assay.oreMineralFraction + 0.00001f)
                return 17;
            if (element.complete && element.massFraction > 0.0001f)
                foundChemicalGrade = true;
        }
        if (FormatOreAssay(d, generator).empty()) return 18;
    }
    if (!foundMixed || !foundChemicalGrade) return 19;

    GeologicalDeposit example;
    example.mineralId = "chalcopyrite";
    example.grade = 0.4f;
    example.components = {{"chalcopyrite", 0.70f}, {"bornite", 0.20f},
        {"pyrite", 0.10f}};
    example.gangue = {{"quartz", 0.45f}, {"calcite", 0.15f}};
    const auto assay = AnalyzeOre(example, generator);
    if (!assay.valid || assay.minerals.size() != 5) return 20;
    auto findGrade = [&](const std::string& symbol)
    {
        for (const auto& element : assay.elements)
            if (element.symbol == symbol && element.complete)
                return element.massFraction;
        return -1.0f;
    };
    const float copperGrade = findGrade("Cu");
    if (copperGrade < 0.14f || copperGrade > 0.15f) return 21;
    example.gangue[0].fraction = 0.70f;
    if (AnalyzeOre(example, generator).valid) return 22;

    VoxelTerrain world;
    const GeologyGenerator* live = world.GetGeology();
    if (!live) return 23;
    const auto veins = live->FindUndergroundVeins(
        -600, 600, -79, 150, -600, 600);
    if (veins.empty()) return 23;
    for (const auto& vein : veins)
        if (!AnalyzeOre(vein, *live).valid) return 24;

    std::cout << "Stage 10C: ore mass balance, mixed minerals, gangue and theoretical element grades passed.\n";
    return 0;
}
