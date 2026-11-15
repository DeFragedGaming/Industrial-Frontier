#include "industrial_frontier/resources/OreProcessing.hpp"

#include <cmath>
#include <iostream>
#include <string>

using namespace industrial_frontier;

namespace
{
    bool Check(bool condition, const std::string& message)
    {
        if (!condition) std::cerr << "FAIL: " << message << '\n';
        return condition;
    }

    InventoryOreBatch ExampleCopper()
    {
        InventoryOreBatch batch;
        batch.depositId = 42;
        batch.primaryMineralId = "chalcopyrite";
        batch.displayName = "Copper-bearing rock";
        batch.massKg = 25.0f;
        batch.oreMineralFraction = 0.40f;
        batch.components = {
            {"chalcopyrite", 0.28f, false},
            {"bornite", 0.08f, false},
            {"pyrite", 0.04f, false},
            {"quartz", 0.40f, true},
            {"calcite", 0.20f, true}
        };
        return batch;
    }
}

int main()
{
    bool success = true;
    const auto raw = ExampleCopper();
    success &= Check(!OreProcessing::Execute(raw, ProcessingOperation::Flotation).success,
        "cannot float raw ore");
    auto crushed = OreProcessing::Execute(raw, ProcessingOperation::Crush);
    success &= Check(crushed.success && crushed.outputs.size() == 1 &&
        crushed.outputs[0].processingStage == OreProcessingStage::Crushed,
        "crushing changes stage");
    auto ground = OreProcessing::Execute(crushed.outputs[0], ProcessingOperation::Grind);
    success &= Check(ground.success && ground.outputs.size() == 1 &&
        ground.outputs[0].processingStage == OreProcessingStage::Ground,
        "grinding changes stage");
    auto floated = OreProcessing::Execute(ground.outputs[0], ProcessingOperation::Flotation);
    success &= Check(floated.success && floated.outputs.size() == 2,
        "flotation creates 2 streams");
    if (floated.success)
    {
        success &= Check(std::abs(floated.concentrateKg + floated.tailingsKg - 25.0f) < .001f,
            "mass conserved in separation");
        success &= Check(floated.outputs[0].oreMineralFraction > raw.oreMineralFraction,
            "concentrate enrichment");
        success &= Check(floated.outputs[0].processingStage == OreProcessingStage::Concentrate &&
            floated.outputs[1].processingStage == OreProcessingStage::Tailings,
            "separation stream stage tags");
        success &= Check(floated.targetRecoveryPercent > 80.0f &&
                         floated.targetRecoveryPercent < 95.0f,
            "copper recovery rate");
        success &= Check(!OreProcessing::Execute(floated.outputs[0], ProcessingOperation::Crush).success,
            "no recycling concentrate as raw feed");
        PlayerInventory inv;
        success &= Check(inv.AddOre(floated.outputs[0]) && inv.AddOre(floated.outputs[1]),
            "both streams can be inventoried");
        success &= Check(PlayerInventory::IsValidState(inv.Capture()),
            "processed inventory validates");
        PlayerInventory restored;
        success &= Check(restored.Restore(inv.Capture()),
            "processed ore stages survive inventory roundtrip");
    }
    InventoryOreBatch iron;
    iron.depositId = 66;
    iron.primaryMineralId = "magnetite";
    iron.displayName = "Magnetite-rich ore";
    iron.massKg = 20.0f;
    iron.oreMineralFraction = .5f;
    iron.components = {{"magnetite", .5f, false}, {"quartz", .5f, true}};
    auto iCrush = OreProcessing::Execute(iron, ProcessingOperation::Crush);
    auto iGrind = OreProcessing::Execute(iCrush.outputs.at(0), ProcessingOperation::Grind);
    auto magnetic = OreProcessing::Execute(iGrind.outputs.at(0),
        ProcessingOperation::MagneticSeparation);
    success &= Check(magnetic.success &&
        std::abs(magnetic.targetRecoveryPercent - 95.0f) < .01f,
        "95 percent magnetite capture");
    success &= Check(!OreProcessing::Execute(iGrind.outputs.at(0),
        ProcessingOperation::Roast).success, "roasting magnetite unsupported");
    InventoryOreBatch siderite = iron;
    siderite.depositId = 67;
    siderite.primaryMineralId = "siderite";
    siderite.components[0].mineralId = "siderite";
    auto sCrushed = OreProcessing::Execute(siderite, ProcessingOperation::Crush);
    auto sGround = OreProcessing::Execute(sCrushed.outputs.at(0), ProcessingOperation::Grind);
    auto roasted = OreProcessing::Execute(sGround.outputs.at(0), ProcessingOperation::Roast);
    success &= Check(roasted.success, "siderite roast pretreatment");
    auto separated = OreProcessing::Execute(roasted.outputs.at(0),
        ProcessingOperation::MagneticSeparation);
    success &= Check(separated.success &&
        separated.targetRecoveryPercent > 91.0f &&
        separated.targetRecoveryPercent < 92.0f,
        "91.5 percent game target siderite capture");
    if (success) std::cout << "Stage 10E ore processing tests passed\n";
    return success ? 0 : 1;
}
