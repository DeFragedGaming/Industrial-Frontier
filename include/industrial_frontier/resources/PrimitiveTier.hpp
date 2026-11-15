#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace industrial_frontier
{
    enum class MiningTier { None, Stone, Copper };

    inline constexpr std::array<std::string_view, 16> StonePickaxeMinerals{{
        "malachite", "azurite", "cuprite", "native_copper",
        "gypsum", "halite", "calcite", "dolomite",
        "graphite", "native_sulfur", "coal", "lignite",
        "limestone", "sandstone", "shale", "dolostone"
    }};

    inline constexpr std::array<std::string_view, 21> CopperPickaxeAdditionalMinerals{{
        "chalcopyrite", "bornite", "chalcocite", "covellite",
        "chrysocolla", "siderite", "limonite", "bauxite",
        "gibbsite", "galena", "cerussite", "sphalerite",
        "smithsonite", "hemimorphite", "stannite", "pyrolusite",
        "rhodochrosite", "molybdenite", "barite", "fluorite",
        "magnesite"
    }};

    inline constexpr std::array<std::string_view, 4> PrimitiveCopperMinerals{{
        "native_copper", "malachite", "azurite", "cuprite"
    }};

    template <std::size_t N>
    inline bool ContainsMineral(const std::array<std::string_view, N>& ids,
                                std::string_view id)
    {
        for (std::string_view entry : ids)
            if (entry == id)
                return true;
        return false;
    }

    inline bool IsStonePickaxeMineral(std::string_view mineralId)
    {
        return ContainsMineral(StonePickaxeMinerals, mineralId);
    }

    inline bool IsCopperPickaxeMineral(std::string_view mineralId)
    {
        return IsStonePickaxeMineral(mineralId) ||
            ContainsMineral(CopperPickaxeAdditionalMinerals, mineralId);
    }

    inline MiningTier PickaxeTier(std::string_view itemId)
    {
        if (itemId == "stone_pickaxe") return MiningTier::Stone;
        if (itemId == "copper_pickaxe") return MiningTier::Copper;
        return MiningTier::None;
    }

    inline bool CanMineMineral(std::string_view toolId,
                              std::string_view mineralId)
    {
        switch (PickaxeTier(toolId))
        {
            case MiningTier::Stone: return IsStonePickaxeMineral(mineralId);
            case MiningTier::Copper: return IsCopperPickaxeMineral(mineralId);
            default: return false;
        }
    }

    inline bool IsUsableAxeId(std::string_view toolId)
    {
        return toolId == "stone_axe" || toolId == "copper_axe";
    }

    inline bool IsUsableShovelId(std::string_view toolId)
    {
        return toolId == "stone_shovel" || toolId == "copper_shovel";
    }

    inline bool CanExcavateTerrain(std::string_view toolId, bool stone)
    {
        return stone ? PickaxeTier(toolId) != MiningTier::None
                     : IsUsableShovelId(toolId);
    }

    inline bool IsPrimitiveCopperMineral(std::string_view mineralId)
    {
        return ContainsMineral(PrimitiveCopperMinerals, mineralId);
    }
}
