#pragma once

#include "industrial_frontier/resources/MineralDatabase.hpp"

namespace industrial_frontier
{
    enum class FieldGuideResourceFilter
    {
        AllResources,
        AllOres,
        CopperOres
    };

    inline bool MatchesGuideResourceFilter(const MineralDefinition& mineral,
        FieldGuideResourceFilter filter)
    {
        if (filter == FieldGuideResourceFilter::AllResources)
            return true;
        const bool oreCandidate = mineral.kind == ResourceKind::Mineral ||
            mineral.kind == ResourceKind::MineralGroup ||
            mineral.kind == ResourceKind::NativeElement ||
            mineral.kind == ResourceKind::FuelRock ||
            (mineral.kind == ResourceKind::Rock &&
                mineral.processingRouteId != "industrial_rock");
        if (filter == FieldGuideResourceFilter::AllOres)
            return oreCandidate;
        return oreCandidate && mineral.commodity == "Copper";
    }

}
