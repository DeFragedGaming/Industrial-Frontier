#include "industrial_frontier/resources/StorageTransfer.hpp"
#include "industrial_frontier/resources/PrimitiveTier.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace industrial_frontier;

void Require(bool value, const char* label)
{
    if (!value)
    {
        std::cerr << "FAILED: " << label << '\n';
        std::exit(1);
    }
}

int main()
{
    Require(PlayerInventory::BaseCarryCapacityKg == 500.0f, "500kg capacity");
    PlayerInventory bag;
    PlayerInventory storage;
    Require(storage.AddItem("log", 105), "105 log stack");
    Require(TransferInventoryStack(storage, bag, 0, StorageTransferMode::One), "one log");
    Require(bag.ItemCount("log") == 1 && storage.ItemCount("log") == 104, "one split");
    Require(TransferInventoryStack(storage, bag, 0, StorageTransferMode::Five), "five logs");
    Require(TransferInventoryStack(storage, bag, 0, StorageTransferMode::Ten), "ten logs");
    Require(bag.ItemCount("log") == 16 && storage.ItemCount("log") == 89, "count split");
    Require(TransferInventoryStack(storage, bag, 0, StorageTransferMode::Half), "half logs");
    Require(bag.ItemCount("log") == 60 && storage.ItemCount("log") == 45, "floor half");
    Require(TransferInventoryStack(storage, bag, 0, StorageTransferMode::All), "remaining logs");
    Require(bag.ItemCount("log") == 105 && storage.ItemCount("log") == 0, "complete transfer");
    Require(bag.CarryCapacityKg() == 500.0f && bag.TotalWeightKg() == 210.0f,
        "500kg usable");
    Require(bag.AddItem("raw_stone", 192), "fill capacity");
    Require(!bag.CanAcceptItem("log", 20), "weight limit enforced");
    Require(storage.AddItem("stick", 20), "sticks in storage");
    const InventoryState original = storage.Capture();
    Require(!TransferInventoryStack(storage, bag, 0, StorageTransferMode::All),
        "atomic overcapacity rejection");
    Require(storage.Capture().items[0].quantity == original.items[0].quantity,
        "failure leaves source untouched");
    Require(TransferInventoryStack(storage, bag, 0, StorageTransferMode::Five),
        "smaller transfer succeeds");
    Require(storage.ItemCount("stick") == 15, "source remainder");
    Require(bag.ItemCount("stick") == 5, "destination partial");

    PlayerInventory mined;
    PlayerInventory chest;
    Require(chest.SetCarryCapacityBonus(900.0f), "large storage");
    InventoryOreBatch ore;
    ore.depositId = 101;
    ore.primaryMineralId = "malachite";
    ore.displayName = "Malachite ore";
    ore.massKg = 112.0f;
    ore.oreMineralFraction = 1.0f;
    ore.components.push_back({"malachite", 1.0f, false});
    Require(chest.AddOre(ore), "112kg ore");
    Require(TransferInventoryStack(chest, mined, 0, StorageTransferMode::Ten), "10kg ore");
    Require(std::fabs(chest.OreBatches()[0].massKg - 102.0f) < 0.001f, "102 remaining");
    Require(std::fabs(mined.OreBatches()[0].massKg - 10.0f) < 0.001f, "10 transferred");
    Require(TransferInventoryStack(chest, mined, 0, StorageTransferMode::Half), "half ore");
    Require(std::fabs(chest.OreBatches()[0].massKg - 51.0f) < 0.001f, "51 remaining");
    Require(PlayerInventory::IsValidState(mined.Capture()) &&
        PlayerInventory::IsValidState(chest.Capture()), "save states valid");
    Require(IsStonePickaxeMineral("malachite") &&
        IsStonePickaxeMineral("native_copper") &&
        IsStonePickaxeMineral("limestone") &&
        !IsStonePickaxeMineral("chalcopyrite") &&
        !IsStonePickaxeMineral("uraninite") &&
        IsPrimitiveCopperMineral("cuprite") &&
        !IsPrimitiveCopperMineral("bornite"), "primitive mineral tier");
    std::cout << "Storage partial stacks, ore mass, 500kg, primitive tier passed\n";
    return 0;
}
