#pragma once

#include "industrial_frontier/resources/PlayerInventory.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <utility>

namespace industrial_frontier
{
    class ItemLayout
    {
    public:
        static constexpr std::size_t BarSize = 8;
        using Grid = std::array<std::string, PlayerInventory::MaxSlots>;
        using Bar = std::array<std::string, BarSize>;

        Grid bag{};
        Grid storage{};
        Bar bar{};
        std::size_t selected = 0;

        static std::string ItemKey(const std::string& id)
        {
            return "item:" + id;
        }

        static std::string OreKey(const InventoryOreBatch& batch)
        {
            std::uint64_t hash = 14695981039346656037ull;
            const auto appendByte = [&](std::uint8_t value)
            {
                hash ^= value;
                hash *= 1099511628211ull;
            };
            const auto appendText = [&](const std::string& value)
            {
                for (unsigned char character : value)
                    appendByte(character);
                appendByte(0);
            };
            const auto appendInteger = [&](std::uint64_t value)
            {
                for (int i = 0; i < 8; ++i)
                {
                    appendByte(static_cast<std::uint8_t>(value & 255));
                    value >>= 8;
                }
            };
            appendInteger(batch.depositId);
            appendInteger(static_cast<std::uint64_t>(batch.processingStage));
            appendText(batch.primaryMineralId);
            appendText(batch.displayName);
            appendInteger(std::bit_cast<std::uint32_t>(batch.oreMineralFraction));
            for (const auto& component : batch.components)
            {
                appendText(component.mineralId);
                appendInteger(std::bit_cast<std::uint32_t>(component.massFraction));
                appendInteger(component.gangue ? 1 : 0);
            }
            std::ostringstream out;
            out << "ore:" << std::hex << hash;
            return out.str();
        }

        static std::vector<std::string> Keys(const InventoryState& inventory)
        {
            std::vector<std::string> keys;
            keys.reserve(inventory.oreBatches.size() + inventory.items.size());
            for (const auto& ore : inventory.oreBatches)
                keys.push_back(OreKey(ore));
            for (const auto& item : inventory.items)
                keys.push_back(ItemKey(item.itemId));
            return keys;
        }

        static int Resolve(const InventoryState& inventory, const std::string& key)
        {
            if (key.empty())
                return -1;
            for (std::size_t i = 0; i < inventory.oreBatches.size(); ++i)
            {
                if (OreKey(inventory.oreBatches[i]) == key)
                    return static_cast<int>(i);
            }
            for (std::size_t i = 0; i < inventory.items.size(); ++i)
            {
                if (ItemKey(inventory.items[i].itemId) == key)
                    return static_cast<int>(inventory.oreBatches.size() + i);
            }
            return -1;
        }

        static void ReconcileGrid(Grid& grid, const InventoryState& inventory,
            const std::unordered_set<std::string>& excluded = {})
        {
            std::unordered_set<std::string> valid;
            for (const auto& key : Keys(inventory))
                valid.insert(key);

            std::unordered_set<std::string> assigned;
            for (auto& cell : grid)
            {
                if (cell.empty())
                    continue;
                if (!valid.contains(cell) || excluded.contains(cell) ||
                    !assigned.insert(cell).second)
                    cell.clear();
            }
            for (const auto& key : Keys(inventory))
            {
                if (assigned.contains(key) || excluded.contains(key))
                    continue;
                for (auto& cell : grid)
                {
                    if (cell.empty())
                    {
                        cell = key;
                        assigned.insert(key);
                        break;
                    }
                }
            }
        }

        void Reconcile(const InventoryState& inventory, const InventoryState& chest)
        {
            std::unordered_set<std::string> occupied;
            for (auto& id : bar)
            {
                if (id.empty())
                    continue;
                if (Resolve(inventory, ItemKey(id)) < 0 ||
                    !occupied.insert(id).second)
                    id.clear();
            }
            std::unordered_set<std::string> equipped;
            for (const auto& id : bar)
                if (!id.empty())
                    equipped.insert(ItemKey(id));
            ReconcileGrid(bag, inventory, equipped);
            ReconcileGrid(storage, chest);
            if (selected >= BarSize)
                selected = 0;
        }

        static void Swap(Grid& grid, std::size_t from, std::size_t to)
        {
            if (from < grid.size() && to < grid.size())
                std::swap(grid[from], grid[to]);
        }

        static void Place(Grid& grid, const std::string& key, std::size_t position)
        {
            if (key.empty() || position >= grid.size())
                return;
            for (auto& cell : grid)
            {
                if (cell == key)
                {
                    cell.clear();
                    break;
                }
            }
            if (grid[position].empty())
                grid[position] = key;
            else
            {
                for (auto& cell : grid)
                {
                    if (cell.empty())
                    {
                        cell = grid[position];
                        grid[position] = key;
                        break;
                    }
                }
            }
        }

        bool Bind(std::size_t barSlot, const std::string& key,
            const InventoryState& inventory)
        {
            if (barSlot >= BarSize || key.rfind("item:", 0) != 0 ||
                Resolve(inventory, key) < 0)
                return false;
            const std::string id = key.substr(5);
            for (auto& slot : bar)
            {
                if (slot == id)
                    slot.clear();
            }
            bar[barSlot] = id;
            selected = barSlot;
            return true;
        }
    };
}
