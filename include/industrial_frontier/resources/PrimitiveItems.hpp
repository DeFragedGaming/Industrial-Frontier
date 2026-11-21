#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace industrial_frontier
{
    struct PrimitiveItemDefinition
    {
        std::string_view id;
        std::string_view name;
        float weightKg;
        std::uint16_t maximumDurability = 0;
    };

    inline constexpr std::array<PrimitiveItemDefinition, 31> PrimitiveItems{{
        {"stick", "Wooden Stick", 0.20f},
        {"pebble", "Loose Pebble", 0.35f},
        {"log", "Wood Log", 2.0f},
        {"raw_stone", "Raw Stone", 1.5f},
        {"clay_soil", "Clay Soil", 1.0f},
        {"stone_axe", "Stone Axe", 1.4f, 100},
        {"stone_pickaxe", "Stone Pickaxe", 1.6f, 120},
        {"stone_shovel", "Stone Shovel", 1.3f, 90},
        {"campfire_kit", "Campfire Kit", 5.0f},
        {"charcoal", "Charcoal", 0.45f},
        {"furnace_kit", "Clay Furnace Kit", 8.0f},
        {"ingot_mold", "Ingot Mold", 2.0f},
        {"hot_copper_mold", "Hot Copper Mold", 2.25f},
        {"cooled_copper_mold", "Cooled Copper Mold", 2.25f},
        {"copper_ingot", "Copper Ingot", 0.25f},
        {"copper_axe", "Copper Axe", 0.9f, 160},
        {"copper_pickaxe", "Copper Pickaxe", 1.15f, 230},
        {"copper_shovel", "Copper Shovel", 0.65f, 145},
        {"multi_ingot_mold", "Eight-Ingot Casting Tray", 6.0f},
        {"tin_ingot", "Tin Ingot", 0.25f},
        {"lead_ingot", "Lead Ingot", 0.25f},
        {"wooden_chest_kit", "Wooden Chest Kit", 6.0f},
        {"log_foundation_kit", "Log Foundation Kit", 8.0f},
        {"log_wall_kit", "Log Wall Kit", 6.0f},
        {"log_doorframe_kit", "Log Doorframe Kit", 5.0f},
        {"log_window_kit", "Log Window Wall Kit", 6.0f},
        {"log_roof_kit", "Log Roof Kit", 7.0f},
        {"log_support_kit", "Log Support Kit", 4.0f},
        {"ore_crusher_kit", "Primitive Edge-Runner Crusher Kit", 22.0f},
        {"hand_mill_kit", "Hand-Powered Ore Mill Kit", 16.0f},
        {"sluice_kit", "Wooden River Sluice Kit", 12.0f}
    }};

    struct PrimitiveIngredient
    {
        std::string_view itemId;
        std::uint32_t quantity;
    };

    struct PrimitiveRecipe
    {
        std::string_view id;
        std::string_view title;
        std::string_view resultId;
        std::uint32_t resultQuantity;
        std::array<PrimitiveIngredient, 4> ingredients;
        std::size_t ingredientCount;
    };

    inline constexpr std::array<PrimitiveRecipe, 20> PrimitiveRecipes{{
        {"stone_axe_recipe", "Stone Axe", "stone_axe", 1,
            {{{"pebble", 2}, {"stick", 2}, {"", 0}, {"", 0}}}, 2},
        {"stone_pickaxe_recipe", "Stone Pickaxe", "stone_pickaxe", 1,
            {{{"pebble", 3}, {"stick", 2}, {"", 0}, {"", 0}}}, 2},
        {"stone_shovel_recipe", "Stone Shovel", "stone_shovel", 1,
            {{{"pebble", 1}, {"stick", 2}, {"", 0}, {"", 0}}}, 2},
        {"campfire_kit_recipe", "Campfire Kit", "campfire_kit", 1,
            {{{"raw_stone", 6}, {"log", 2}, {"stick", 3}, {"", 0}}}, 3},
        {"furnace_kit_recipe", "Clay Furnace Kit", "furnace_kit", 1,
            {{{"clay_soil", 20}, {"raw_stone", 8}, {"stick", 6}, {"log", 4}}}, 4},
        {"ingot_mold_recipe", "Ingot Mold", "ingot_mold", 1,
            {{{"raw_stone", 6}, {"clay_soil", 8}, {"", 0}, {"", 0}}}, 2},
        {"copper_axe_recipe", "Copper Axe", "copper_axe", 1,
            {{{"copper_ingot", 2}, {"stick", 2}, {"", 0}, {"", 0}}}, 2},
        {"copper_pickaxe_recipe", "Copper Pickaxe", "copper_pickaxe", 1,
            {{{"copper_ingot", 3}, {"stick", 2}, {"", 0}, {"", 0}}}, 2},
        {"copper_shovel_recipe", "Copper Shovel", "copper_shovel", 1,
            {{{"copper_ingot", 1}, {"stick", 2}, {"", 0}, {"", 0}}}, 2},
        {"multi_ingot_mold_recipe", "Eight-Ingot Casting Tray", "multi_ingot_mold", 1,
            {{{"raw_stone", 12}, {"clay_soil", 16}, {"stick", 2}, {"", 0}}}, 3},
        {"wooden_chest_recipe", "Wooden Chest", "wooden_chest_kit", 1,
            {{{"log", 8}, {"stick", 6}, {"", 0}, {"", 0}}}, 2},
        {"log_foundation_recipe", "Log Foundation", "log_foundation_kit", 1,
            {{{"log", 6}, {"stick", 4}, {"", 0}, {"", 0}}}, 2},
        {"log_wall_recipe", "Log Wall", "log_wall_kit", 1,
            {{{"log", 7}, {"stick", 3}, {"", 0}, {"", 0}}}, 2},
        {"log_doorframe_recipe", "Log Doorframe", "log_doorframe_kit", 1,
            {{{"log", 5}, {"stick", 4}, {"", 0}, {"", 0}}}, 2},
        {"log_window_recipe", "Log Window Wall", "log_window_kit", 1,
            {{{"log", 6}, {"stick", 4}, {"", 0}, {"", 0}}}, 2},
        {"log_roof_recipe", "Log Roof", "log_roof_kit", 1,
            {{{"log", 6}, {"stick", 4}, {"", 0}, {"", 0}}}, 2},
        {"log_support_recipe", "Log Support", "log_support_kit", 1,
            {{{"log", 3}, {"stick", 3}, {"", 0}, {"", 0}}}, 2},
        {"ore_crusher_recipe", "Primitive Edge-Runner Crusher", "ore_crusher_kit", 1,
            {{{"raw_stone", 12}, {"log", 8}, {"stick", 8}, {"clay_soil", 4}}}, 4},
        {"hand_mill_recipe", "Hand-Powered Ore Mill", "hand_mill_kit", 1,
            {{{"raw_stone", 10}, {"log", 6}, {"stick", 8}, {"clay_soil", 2}}}, 4},
        {"sluice_recipe", "Wooden River Sluice", "sluice_kit", 1,
            {{{"log", 8}, {"stick", 10}, {"raw_stone", 4}, {"", 0}}}, 3}
    }};

    inline const PrimitiveItemDefinition* FindPrimitiveItem(std::string_view id)
    {
        for (const auto& item : PrimitiveItems)
        {
            if (item.id == id)
                return &item;
        }
        return nullptr;
    }
}
