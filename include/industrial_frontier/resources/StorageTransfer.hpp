#pragma once

#include "industrial_frontier/resources/PlayerInventory.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace industrial_frontier
{
    enum class StorageTransferMode : int
    {
        One = 0,
        Five = 1,
        Ten = 2,
        Half = 3,
        All = 4
    };

    inline constexpr const char* StorageTransferModeName(StorageTransferMode mode)
    {
        switch (mode)
        {
        case StorageTransferMode::One: return "x1";
        case StorageTransferMode::Five: return "x5";
        case StorageTransferMode::Ten: return "x10";
        case StorageTransferMode::Half: return "HALF";
        case StorageTransferMode::All: return "ALL";
        }
        return "ALL";
    }

    inline std::uint32_t SelectItemTransferCount(std::uint32_t available,
        StorageTransferMode mode)
    {
        if (available == 0)
            return 0;
        switch (mode)
        {
        case StorageTransferMode::One: return 1;
        case StorageTransferMode::Five: return std::min(available, 5u);
        case StorageTransferMode::Ten: return std::min(available, 10u);
        case StorageTransferMode::Half: return std::max(1u, available / 2u);
        case StorageTransferMode::All: return available;
        }
        return 0;
    }

    inline float SelectOreTransferKg(float available, StorageTransferMode mode)
    {
        if (!std::isfinite(available) || available <= 0.0f)
            return 0.0f;
        switch (mode)
        {
        case StorageTransferMode::One: return std::min(available, 1.0f);
        case StorageTransferMode::Five: return std::min(available, 5.0f);
        case StorageTransferMode::Ten: return std::min(available, 10.0f);
        case StorageTransferMode::Half: return available * 0.5f;
        case StorageTransferMode::All: return available;
        }
        return 0.0f;
    }

    inline bool TransferInventoryStack(PlayerInventory& source,
        PlayerInventory& destination, std::size_t logicalSlot,
        StorageTransferMode mode)
    {
        if (&source == &destination)
            return false;
        PlayerInventory revisedSource = source;
        PlayerInventory revisedDestination = destination;
        const InventoryState snapshot = revisedSource.Capture();
        if (logicalSlot < snapshot.oreBatches.size())
        {
            InventoryOreBatch moved = snapshot.oreBatches[logicalSlot];
            const float amount = SelectOreTransferKg(moved.massKg, mode);
            if (amount <= 0.0f)
                return false;
            moved.massKg = amount;
            if (!revisedDestination.AddOre(moved) ||
                !revisedSource.RemoveOre(logicalSlot, amount))
                return false;
        }
        else
        {
            const std::size_t index = logicalSlot - snapshot.oreBatches.size();
            if (index >= snapshot.items.size())
                return false;
            InventoryItemStack moved = snapshot.items[index];
            moved.quantity = SelectItemTransferCount(moved.quantity, mode);
            if (moved.quantity == 0 ||
                !revisedDestination.AddItemStack(moved) ||
                !revisedSource.RemoveItem(moved.itemId, moved.quantity))
                return false;
        }
        source = std::move(revisedSource);
        destination = std::move(revisedDestination);
        return true;
    }
}
