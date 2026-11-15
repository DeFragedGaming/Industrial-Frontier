#include "industrial_frontier/resources/EarlyMetallurgy.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace industrial_frontier;

static void Require(bool condition, const char* error)
{
    if (!condition)
        throw std::runtime_error(error);
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc == 2, "expected mineral TSV argument");
        MineralDatabase minerals;
        std::string error;
        Require(minerals.LoadFromFile(argv[1], error), "failed to load minerals");

        auto feed = [&](const char* mineralId, float grade, float mass) {
            InventoryOreBatch batch;
            batch.depositId = 125;
            batch.primaryMineralId = mineralId;
            batch.displayName = mineralId;
            batch.massKg = mass;
            batch.oreMineralFraction = grade;
            batch.components = {{mineralId, grade, false}, {"quartz", 1.0f-grade, true}};
            return EvaluatePrimitiveMetalFeed(batch, minerals);
        };
        auto copper = feed("malachite", 0.5f, 112.0f);
        Require(copper.supported && copper.metalId == "copper", "malachite copper feed");
        Require(copper.recoveredMetalFraction > 0.20f, "mixed grade recovery");
        auto tin = feed("cassiterite", 0.7f, 12.0f);
        Require(tin.supported && tin.metalId == "tin", "cassiterite tin feed");
        auto lead = feed("cerussite", 0.7f, 12.0f);
        Require(lead.supported && lead.metalId == "lead", "cerussite lead feed");
        Require(!feed("galena", 0.7f, 12.0f).supported, "sulfide must be rejected");
        Require(!feed("hematite", 0.7f, 12.0f).supported, "iron ore needs advanced furnace");
        Require(!feed("enargite", 0.7f, 12.0f).supported, "arsenic ore unsafe");
        Require(FindPrimitiveItem("multi_ingot_mold") &&
            FindPrimitiveItem("tin_ingot") && FindPrimitiveItem("lead_ingot"),
            "primitive item definitions");
        ClayFurnaceWorld furnace;
        Require(furnace.Place(3, 0, 3), "place furnace");
        const auto id = furnace.Furnaces().front().id;
        const float recovered = 112.0f * copper.recoveredMetalFraction;
        const float duration = 30.0f + 112.0f * 0.6f;
        Require(furnace.AddCharge(id, recovered, "Malachite 112kg", "copper", duration),
            "large charge should load all 112kg in one operation");
        for (int i = 0; i < 3; ++i)
            Require(furnace.AddCharcoal(id), "charcoal charge");
        for (int i = 0; i < 550; ++i)
            furnace.Update(0.25f);
        Require(furnace.Find(id)->chargeCopperKg < 0.0001f &&
            furnace.Find(id)->readyCopperKg > 10.0f, "smelting completes");
        Require(furnace.InstallMold(id, 8), "install multi mold");
        Require(furnace.StartCasting(id) == 8, "cast eight bars");
        Require(furnace.StartCasting(id) == 0, "cannot cast into active mold");
        Require(!furnace.Remove(id), "cannot dismantle with mold");
        const auto snapshot = furnace.Furnaces();
        ClayFurnaceWorld restored;
        Require(restored.Restore(snapshot), "save/restore in-progress eight-cast");
        for (int i=0;i<125;++i)
            restored.Update(0.25f);
        Require(restored.Find(id)->castingCoolingSeconds == 0.0f, "cast cooled");
        Require(restored.CollectCasting(id) == 8, "collect eight bars");
        Require(restored.Find(id)->moldCapacity == 8, "mold stays installed");
        Require(restored.StartCasting(id) == 8, "refill existing mold");
        Require(!restored.AddCharge(id, 4.0f, "Tin", "tin"), "cannot mix tin and copper");
        Require(ClayFurnaceWorld::Validate(restored.Furnaces()), "valid serialized state");
        ClayFurnaceWorld fresh;
        Require(fresh.Place(0, 0, 0), "new furnace");
        const auto tinId = fresh.Furnaces().front().id;
        Require(fresh.AddCharge(tinId, tin.recoveredMetalFraction * 8.0f,
            "Cassiterite", "tin", 34.8f), "tin batch");
        Require(!fresh.AddCharge(tinId, 1.0f, "Cerussite", "lead"), "busy charge");
        std::cout << "Primitive copper/tin/lead batch smelting, persistent eight-cast mold passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "Primitive batching failed: " << error.what() << '\n';
        return 1;
    }
}
