#include "industrial_frontier/resources/CampfireDismantle.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

using namespace industrial_frontier;

namespace
{
    void Require(bool value, std::string_view message)
    {
        if (value)
            return;
        std::cerr << "Campfire dismantle test failed: " << message << '\n';
        std::exit(EXIT_FAILURE);
    }
}

int main()
{
    CampfireWorld world;
    PlayerInventory inventory;
    Require(world.Place(0.0f, 1.0f, 0.0f), "place first fire");
    Require(world.Place(4.0f, 1.0f, 0.0f), "place second fire");
    const std::uint64_t first = world.Fires()[0].id;
    const std::uint64_t second = world.Fires()[1].id;

    Require(world.AddLog(first), "fuel fire");
    Require(DismantleCampfire(world, inventory, first) ==
        CampfireDismantleResult::StillBurning, "cannot dismantle hot fire");
    Require(inventory.UsedSlots() == 0, "no payout while burning");
    Require(world.Cover(first), "cover fire");
    for (int i = 0; i < 180; ++i)
        world.Update(0.25f);
    Require(DismantleCampfire(world, inventory, first) ==
        CampfireDismantleResult::CharcoalRemaining,
        "cannot discard ready charcoal");
    std::uint32_t charcoal = 0;
    Require(world.Collect(first, charcoal) && charcoal == 1,
        "collect charcoal before dismantle");

    Require(DismantleCampfire(world, inventory, first) ==
        CampfireDismantleResult::Success, "reclaim complete campfire");
    Require(world.Find(first) == nullptr, "removed from world");
    Require(world.Fires().size() == 1 && world.Find(second),
        "other fire remains");
    Require(inventory.ItemCount("raw_stone") == 6,
        "reclaim six stones");
    Require(inventory.ItemCount("log") == 2,
        "reclaim two structural logs");
    Require(inventory.ItemCount("stick") == 3,
        "reclaim three sticks");
    Require(DismantleCampfire(world, inventory, first) ==
        CampfireDismantleResult::NotFound, "cannot reclaim twice");
    Require(inventory.ItemCount("raw_stone") == 6,
        "duplicate salvage prohibited");

    PlayerInventory overweight;
    Require(overweight.AddItem("raw_stone", 329), "populate near weight limit");
    const auto before = overweight.Capture();
    Require(DismantleCampfire(world, overweight, second) ==
        CampfireDismantleResult::InventoryFull,
        "insufficient capacity is rejected");
    Require(overweight.ItemCount("raw_stone") == before.items[0].quantity,
        "inventory unchanged on failed dismantle");
    Require(world.Find(second) != nullptr,
        "fire remains if dismantle fails");

    CampfireWorld restored;
    Require(restored.Restore(world.Fires()), "world save roundtrip");
    PlayerInventory recovered;
    Require(recovered.Restore(inventory.Capture()), "inventory save roundtrip");
    Require(restored.Fires().size() == 1, "removed fire stays removed");
    Require(recovered.ItemCount("log") == 2, "recovered materials persist");
    Require(DismantleCampfire(restored, recovered, second) ==
        CampfireDismantleResult::Success, "dismantle remaining fire");
    Require(recovered.ItemCount("raw_stone") == 12,
        "reclaimed materials stack without duplication");
    std::cout << "Campfire dismantle, inventory capacity, and save state passed\n";
    return EXIT_SUCCESS;
}
