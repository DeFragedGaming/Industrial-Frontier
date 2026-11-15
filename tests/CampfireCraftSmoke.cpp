#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

using namespace industrial_frontier;

namespace
{
    void Require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Campfire crafting test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    PlayerInventory inv;
    Require(inv.AddItem("raw_stone", 6), "add raw stone");
    Require(inv.AddItem("log", 2), "add logs");
    Require(inv.AddItem("stick", 3), "add sticks");
    std::string message;
    Require(inv.Craft("campfire_kit_recipe", message), "craft campfire kit");
    Require(inv.ItemCount("campfire_kit") == 1, "kit output quantity");
    Require(!inv.Craft("campfire_kit_recipe", message), "reject missing ingredients");
    Require(inv.AddItem("charcoal", 2), "add charcoal");
    Require(inv.ItemCount("charcoal") == 2, "charcoal quantity");
    std::cout << "Campfire crafting passed\n";
    return EXIT_SUCCESS;
}
