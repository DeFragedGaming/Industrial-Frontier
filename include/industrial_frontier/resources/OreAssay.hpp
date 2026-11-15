#pragma once

#include "industrial_frontier/resources/GeologyGenerator.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace industrial_frontier
{
    struct AssayMineral
    {
        std::string mineralId;
        float massFraction = 0.0f;
        bool gangue = false;
    };

    struct AssayElement
    {
        std::string symbol;
        float massFraction = 0.0f;
        bool complete = true;
    };

    struct OreAssay
    {
        float oreMineralFraction = 0.0f;
        float gangueFraction = 0.0f;
        std::vector<AssayMineral> minerals;
        std::vector<AssayElement> elements;
        bool valid = false;
    };

    enum class OreGradeTier
    {
        Poor,
        Low,
        Moderate,
        Rich,
        Exceptional
    };

    OreGradeTier ClassifyOreGrade(float fraction);
    const char* OreGradeTierName(OreGradeTier tier);

    bool TheoreticalElementFraction(
        std::string_view mineralFormula,
        std::string_view element,
        double& fraction);

    OreAssay AnalyzeOre(
        const GeologicalDeposit& deposit,
        const GeologyGenerator& geology);

    std::string FormatOreAssay(
        const GeologicalDeposit& deposit,
        const GeologyGenerator& geology);
}
