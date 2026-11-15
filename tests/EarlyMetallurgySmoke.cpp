#include "industrial_frontier/resources/EarlyMetallurgy.hpp"
#include "industrial_frontier/resources/OreProcessing.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

using namespace industrial_frontier;

static bool Check(bool passed, const char* message)
{
    if (!passed)
        std::cerr << "FAIL: " << message << '\n';
    return passed;
}

int main(int argc, char** argv)
{
    if (argc < 2)
        return 2;
    MineralDatabase minerals;
    std::string error;
    if (!Check(minerals.LoadFromFile(argv[1], error), "load real minerals"))
        return 1;

    InventoryOreBatch malachite;
    malachite.depositId = 42;
    malachite.primaryMineralId = "malachite";
    malachite.displayName = "Malachite";
    malachite.massKg = 1.0f;
    malachite.oreMineralFraction = 0.8f;
    malachite.components = {{"malachite", 0.8f, false},
        {"quartz", 0.2f, true}};

    const auto oxide = EvaluateCopperFeed(malachite, minerals);
    if (!Check(oxide.supported && oxide.copperFraction > 0.44f &&
        oxide.copperFraction < 0.49f && oxide.recoveryFraction > 0.7f,
        "malachite actual copper formula and mass fraction"))
        return 1;

    auto sulfide = malachite;
    sulfide.primaryMineralId = "chalcopyrite";
    sulfide.displayName = "Chalcopyrite";
    sulfide.components[0].mineralId = "chalcopyrite";
    if (!Check(!EvaluateCopperFeed(sulfide, minerals).supported,
        "raw chalcopyrite cannot be direct-reduced"))
        return 1;
    sulfide.processingStage = OreProcessingStage::Roasted;
    if (!Check(!EvaluateCopperFeed(sulfide, minerals).supported,
        "roasted sulfide requires an advanced furnace"))
        return 1;
    sulfide.primaryMineralId = "enargite";
    sulfide.components[0].mineralId = "enargite";
    if (!Check(!EvaluateCopperFeed(sulfide, minerals).supported,
        "arsenic sulfide rejected"))
        return 1;
    sulfide.primaryMineralId = "chrysocolla";
    sulfide.components[0].mineralId = "chrysocolla";
    if (!Check(!EvaluateCopperFeed(sulfide, minerals).supported,
        "variable formula rejected"))
        return 1;

    const auto* furnace = FindPrimitiveItem("furnace_kit");
    const auto* mold = FindPrimitiveItem("ingot_mold");
    if (!Check(furnace && mold && FindPrimitiveItem("copper_ingot"),
        "new primitive item definitions"))
        return 1;

    ClayFurnaceWorld world;
    if (!Check(world.Place(2.0f, 1.0f, 3.0f), "place furnace") ||
        !Check(!world.Place(2.5f, 1.0f, 3.0f), "reject overlapping furnace"))
        return 1;
    const auto id = world.Furnaces().front().id;
    if (!Check(world.AddCharge(id, oxide.copperFraction *
        oxide.recoveryFraction, "Malachite"), "load copper minerals") ||
        !Check(world.AddCharcoal(id), "fuel charcoal"))
        return 1;
    for (int i=0;i<160;++i)
        world.Update(0.25f);
    if (!Check(world.Find(id)->chargeCopperKg == 0.0f &&
        world.Find(id)->readyCopperKg > ClayFurnaceWorld::IngotMassKg,
        "smelt completes with copper available"))
        return 1;
    const auto snapshot = world.Furnaces();
    ClayFurnaceWorld restored;
    if (!Check(restored.Restore(snapshot), "restore smelted furnace") ||
        !Check(restored.Pour(id), "cast one ingot"))
        return 1;
    if (!Check(restored.Find(id)->readyCopperKg < snapshot.front().readyCopperKg,
        "casting removes copper from furnace"))
        return 1;
    if (!Check(!restored.Remove(id), "cannot dismantle a fueled furnace"))
        return 1;

    std::cout << "Early metallurgy and real copper-mineral assay passed\n";
    return 0;
}
