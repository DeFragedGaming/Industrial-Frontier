#include "industrial_frontier/resources/Campfire.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace industrial_frontier;

namespace
{
    void Require(bool condition, std::string_view message)
    {
        if (condition)
            return;
        std::cerr << "Campfire test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    CampfireWorld world;
    Require(world.Place(0.0f, 4.0f, 0.0f), "first placement");
    Require(!world.Place(0.2f, 4.0f, 0.2f), "overlap rejected");
    Require(world.Place(5.0f, 5.0f, 5.0f), "second placement");
    Require(world.Fires().size() == 2, "two campfires placed");

    const auto id = world.Fires().front().id;
    Require(world.AddLog(id), "add first log");
    Require(world.Cover(id), "cover burning campfire");
    Require(!world.AddLog(id), "cannot fuel covered campfire");

    for (int i = 0; i < 179; ++i)
        world.Update(0.25f);
    Require(world.Find(id) != nullptr, "campfire exists after updates");
    Require(world.Find(id)->charcoalReady == 0, "charcoal not early");
    world.Update(0.25f);
    Require(world.Find(id)->charcoalReady == 1, "charcoal after full burn");

    std::uint32_t qty = 0;
    auto original = world.Fires();
    CampfireWorld reloaded;
    Require(reloaded.Restore(original), "restore saved state");
    Require(reloaded.Collect(id, qty) && qty == 1, "collect saved charcoal");
    Require(!reloaded.Collect(id, qty), "cannot collect twice");
    Require(reloaded.AddLog(id), "refuel uncovered campfire");
    Require(reloaded.Find(id) != nullptr, "restored campfire exists");
    Require(reloaded.Find(id)->fuelSeconds == CampfireWorld::SecondsPerLog,
        "refuel duration");

    reloaded.Clear();
    Require(reloaded.Fires().empty(), "clear campfires");
    Require(CampfireWorld::Validate(original), "validate saved state");
    original.front().fuelSeconds = -2.0f;
    Require(!CampfireWorld::Validate(original), "reject invalid fuel");

    std::cout << "Campfire persistence, burn timing and collection passed\n";
    return EXIT_SUCCESS;
}
