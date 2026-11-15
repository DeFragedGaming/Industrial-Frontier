#include "industrial_frontier/resources/PlayerInventory.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"

#include <limits>

#include <algorithm>
#include <cmath>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Tolerance = 0.0005f;

        bool ValidName(const std::string& value, std::size_t maximum)
        {
            return !value.empty() && value.size() <= maximum &&
                value.find('\n') == std::string::npos &&
                value.find('\r') == std::string::npos;
        }

        bool SameFloat(float a, float b)
        {
            return std::abs(a - b) <= Tolerance;
        }

        bool ValidBatch(const InventoryOreBatch& batch)
        {
            if (static_cast<int>(batch.processingStage) < 0 ||
                static_cast<int>(batch.processingStage) > 5 ||
                batch.depositId == 0 ||
                !ValidName(batch.primaryMineralId, 80) ||
                !ValidName(batch.displayName, 100) ||
                !std::isfinite(batch.massKg) || batch.massKg <= 0.0f ||
                batch.massKg > 1000.0f ||
                !std::isfinite(batch.oreMineralFraction) ||
                batch.oreMineralFraction < 0.0f ||
                batch.oreMineralFraction > 1.0f ||
                batch.components.empty() || batch.components.size() > 24)
            {
                return false;
            }

            double fractionSum = 0.0;
            double oreSum = 0.0;
            for (const auto& component : batch.components)
            {
                if (!ValidName(component.mineralId, 80) ||
                    !std::isfinite(component.massFraction) ||
                    component.massFraction < 0.0f ||
                    component.massFraction > 1.0f)
                {
                    return false;
                }
                fractionSum += component.massFraction;
                if (!component.gangue)
                {
                    oreSum += component.massFraction;
                }
            }
            return std::abs(fractionSum - 1.0) <= 0.002 &&
                std::abs(oreSum - batch.oreMineralFraction) <= 0.002;
        }
    }

    void PlayerInventory::Clear()
    {
        m_state = {};
    }

    bool PlayerInventory::Compatible(
        const InventoryOreBatch& a, const InventoryOreBatch& b)
    {
        if (a.depositId != b.depositId ||
            a.primaryMineralId != b.primaryMineralId ||
            a.displayName != b.displayName ||
            a.processingStage != b.processingStage ||
            !SameFloat(a.oreMineralFraction, b.oreMineralFraction) ||
            a.components.size() != b.components.size())
        {
            return false;
        }
        for (std::size_t i = 0; i < a.components.size(); ++i)
        {
            if (a.components[i].mineralId != b.components[i].mineralId ||
                a.components[i].gangue != b.components[i].gangue ||
                !SameFloat(a.components[i].massFraction,
                    b.components[i].massFraction))
            {
                return false;
            }
        }
        return true;
    }

    float PlayerInventory::TotalWeightKg() const
    {
        double total = 0.0;
        for (const auto& batch : m_state.oreBatches)
        {
            total += batch.massKg;
        }
        for (const auto& stack : m_state.items)
        {
            const auto* item = FindPrimitiveItem(stack.itemId);
            if (item)
                total += static_cast<double>(stack.quantity) * item->weightKg;
        }
        return static_cast<float>(total);
    }

    float PlayerInventory::CarryCapacityKg() const
    {
        return BaseCarryCapacityKg + m_state.carryCapacityBonusKg;
    }

    std::size_t PlayerInventory::UsedSlots() const
    {
        return m_state.oreBatches.size() + m_state.items.size();
    }

    const std::vector<InventoryOreBatch>& PlayerInventory::OreBatches() const
    {
        return m_state.oreBatches;
    }

    const std::vector<InventoryItemStack>& PlayerInventory::Items() const
    {
        return m_state.items;
    }

    std::uint32_t PlayerInventory::ItemCount(const std::string& itemId) const
    {
        for (const auto& stack : m_state.items)
        {
            if (stack.itemId == itemId)
                return stack.quantity;
        }
        return 0;
    }

    bool PlayerInventory::CanAcceptItem(
        const std::string& itemId, std::uint32_t quantity) const
    {
        const auto* definition = FindPrimitiveItem(itemId);
        if (!definition || quantity == 0 || quantity > 999 ||
            TotalWeightKg() + static_cast<float>(quantity) * definition->weightKg >
                CarryCapacityKg() + Tolerance)
            return false;
        for (const auto& stack : m_state.items)
        {
            if (stack.itemId == itemId)
                return quantity <= 999 - stack.quantity;
        }
        return UsedSlots() < MaxSlots;
    }

    bool PlayerInventory::AddItem(const std::string& itemId, std::uint32_t quantity)
    {
        if (!CanAcceptItem(itemId, quantity))
            return false;
        for (auto& stack : m_state.items)
        {
            if (stack.itemId == itemId)
            {
                stack.quantity += quantity;
                return true;
            }
        }
        const auto* definition = FindPrimitiveItem(itemId);
        m_state.items.push_back({itemId, quantity,
            static_cast<std::uint16_t>(definition ?
                definition->maximumDurability : 0)});
        return true;
    }

    bool PlayerInventory::AddItemStack(const InventoryItemStack& stack)
    {
        if (!CanAcceptItem(stack.itemId, stack.quantity))
            return false;
        const auto* definition = FindPrimitiveItem(stack.itemId);
        if (!definition || (definition->maximumDurability > 0 &&
            (stack.durability == 0 ||
             stack.durability > definition->maximumDurability)))
            return false;
        if (definition->maximumDurability == 0 && stack.durability != 0)
            return false;
        for (auto& existing : m_state.items)
        {
            if (existing.itemId == stack.itemId)
            {
                existing.quantity += stack.quantity;
                if (definition->maximumDurability > 0)
                    existing.durability = std::min(existing.durability,
                        stack.durability);
                return true;
            }
        }
        m_state.items.push_back(stack);
        return true;
    }

    bool PlayerInventory::HasUsableTool(const std::string& itemId) const
    {
        const auto* def = FindPrimitiveItem(itemId);
        if (!def || def->maximumDurability == 0)
            return false;
        for (const auto& stack : m_state.items)
            if (stack.itemId == itemId && stack.quantity != 0 &&
                stack.durability != 0)
                return true;
        return false;
    }

    bool PlayerInventory::UseTool(const std::string& itemId,
        std::uint16_t cost, bool& broke)
    {
        broke = false;
        if (cost == 0)
            return false;
        const auto* def = FindPrimitiveItem(itemId);
        if (!def || def->maximumDurability == 0)
            return false;
        for (std::size_t i = 0; i < m_state.items.size(); ++i)
        {
            auto& stack = m_state.items[i];
            if (stack.itemId != itemId || stack.durability == 0)
                continue;
            if (cost < stack.durability)
            {
                stack.durability -= cost;
                return true;
            }
            broke = true;
            --stack.quantity;
            if (stack.quantity == 0)
                m_state.items.erase(m_state.items.begin() +
                    static_cast<std::ptrdiff_t>(i));
            else
                stack.durability = def->maximumDurability;
            return true;
        }
        return false;
    }

    bool PlayerInventory::RemoveItem(const std::string& itemId, std::uint32_t quantity)
    {
        if (quantity == 0)
            return false;
        for (std::size_t i = 0; i < m_state.items.size(); ++i)
        {
            auto& stack = m_state.items[i];
            if (stack.itemId != itemId || stack.quantity < quantity)
                continue;
            stack.quantity -= quantity;
            if (stack.quantity == 0)
                m_state.items.erase(m_state.items.begin() +
                    static_cast<std::ptrdiff_t>(i));
            return true;
        }
        return false;
    }

    bool PlayerInventory::RemoveItemSlot(std::size_t slot)
    {
        if (slot >= m_state.items.size())
            return false;
        m_state.items.erase(m_state.items.begin() + static_cast<std::ptrdiff_t>(slot));
        return true;
    }

    bool PlayerInventory::Craft(const std::string& recipeId, std::string& message)
    {
        for (const auto& recipe : PrimitiveRecipes)
        {
            if (recipe.id != recipeId)
                continue;
            for (std::size_t i = 0; i < recipe.ingredientCount; ++i)
            {
                if (ItemCount(std::string(recipe.ingredients[i].itemId)) <
                    recipe.ingredients[i].quantity)
                {
                    message = "Missing " + std::string(recipe.ingredients[i].itemId);
                    return false;
                }
            }
            PlayerInventory proposed = *this;
            for (std::size_t i = 0; i < recipe.ingredientCount; ++i)
            {
                if (!proposed.RemoveItem(std::string(recipe.ingredients[i].itemId),
                    recipe.ingredients[i].quantity))
                    return false;
            }
            if (!proposed.AddItem(std::string(recipe.resultId), recipe.resultQuantity))
            {
                message = "Inventory full or too heavy";
                return false;
            }
            m_state = proposed.m_state;
            message = "Crafted " + std::string(recipe.title);
            return true;
        }
        message = "Unknown recipe";
        return false;
    }

    bool PlayerInventory::CanAccept(const InventoryOreBatch& batch) const
    {
        if (!ValidBatch(batch) ||
            TotalWeightKg() + batch.massKg > CarryCapacityKg() + Tolerance)
        {
            return false;
        }
        for (const auto& existing : m_state.oreBatches)
        {
            if (Compatible(existing, batch))
            {
                return existing.massKg + batch.massKg <= 1000.0f;
            }
        }
        return UsedSlots() < MaxSlots;
    }

    bool PlayerInventory::AddOre(const InventoryOreBatch& batch)
    {
        if (!CanAccept(batch))
        {
            return false;
        }
        for (auto& existing : m_state.oreBatches)
        {
            if (Compatible(existing, batch))
            {
                existing.massKg += batch.massKg;
                return true;
            }
        }
        m_state.oreBatches.push_back(batch);
        return true;
    }

    bool PlayerInventory::RemoveOre(std::size_t slot, float massKg)
    {
        if (slot >= m_state.oreBatches.size() ||
            !std::isfinite(massKg) || massKg <= 0.0f ||
            massKg > m_state.oreBatches[slot].massKg + Tolerance)
        {
            return false;
        }
        auto& batch = m_state.oreBatches[slot];
        batch.massKg -= massKg;
        if (batch.massKg <= Tolerance)
        {
            m_state.oreBatches.erase(m_state.oreBatches.begin() +
                static_cast<std::ptrdiff_t>(slot));
        }
        return true;
    }

    bool PlayerInventory::SetCarryCapacityBonus(float bonusKg)
    {
        if (!std::isfinite(bonusKg) || bonusKg < 0.0f ||
            bonusKg > MaximumCarryCapacityBonusKg ||
            TotalWeightKg() > BaseCarryCapacityKg + bonusKg + Tolerance)
        {
            return false;
        }
        m_state.carryCapacityBonusKg = bonusKg;
        return true;
    }

    bool PlayerInventory::IsValidState(const InventoryState& state)
    {
        if (!std::isfinite(state.carryCapacityBonusKg) ||
            state.carryCapacityBonusKg < 0.0f ||
            state.carryCapacityBonusKg > MaximumCarryCapacityBonusKg ||
            state.oreBatches.size() + state.items.size() > MaxSlots)
        {
            return false;
        }
        double totalMass = 0.0;
        for (std::size_t i = 0; i < state.items.size(); ++i)
        {
            const auto& stack = state.items[i];
            const auto* def = FindPrimitiveItem(stack.itemId);
            if (!def || stack.quantity == 0 || stack.quantity > 999 ||
                (def->maximumDurability > 0 &&
                 stack.durability > def->maximumDurability) ||
                (def->maximumDurability == 0 && stack.durability != 0))
                return false;
            totalMass += static_cast<double>(def->weightKg) * stack.quantity;
            for (std::size_t j = i + 1; j < state.items.size(); ++j)
            {
                if (stack.itemId == state.items[j].itemId)
                    return false;
            }
        }
        for (std::size_t i = 0; i < state.oreBatches.size(); ++i)
        {
            if (!ValidBatch(state.oreBatches[i]))
            {
                return false;
            }
            totalMass += state.oreBatches[i].massKg;
            for (std::size_t j = i + 1; j < state.oreBatches.size(); ++j)
            {
                if (Compatible(state.oreBatches[i], state.oreBatches[j]))
                {
                    return false;
                }
            }
        }
        return totalMass <= BaseCarryCapacityKg +
            state.carryCapacityBonusKg + Tolerance;
    }

    bool PlayerInventory::Restore(const InventoryState& state)
    {
        if (!IsValidState(state))
        {
            return false;
        }
        m_state = state;
        for (auto& stack : m_state.items)
        {
            const auto* item = FindPrimitiveItem(stack.itemId);
            if (item && item->maximumDurability > 0 && stack.durability == 0)
                stack.durability = item->maximumDurability;
        }
        return true;
    }

    InventoryState PlayerInventory::Capture() const
    {
        return m_state;
    }
}
