#include "industrial_frontier/save/SaveManager.hpp"
#include "industrial_frontier/resources/PrimitiveItems.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <unordered_set>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <system_error>

namespace industrial_frontier
{
    namespace
    {
        constexpr std::size_t MaxEdits = 500000;
        constexpr std::size_t MaxDepletedDeposits = 500000;
        constexpr std::size_t MaxGatheredObjects = 500000;
        constexpr std::uintmax_t MaxFileBytes = 64000000;

        std::uint64_t Hash(const std::string& value)
        {
            std::uint64_t result = 14695981039346656037ull;
            for (unsigned char c : value)
            {
                result ^= c;
                result *= 1099511628211ull;
            }
            return result;
        }

        bool FinitePosition(float value)
        {
            return std::isfinite(value) && std::abs(value) < 10000000.0f;
        }

        bool Validate(const WorldSave& world)
        {
            if (world.name.empty() || world.name.size() > 64 ||
                world.seed < 0 || world.edits.size() > MaxEdits ||
                world.depletedDeposits.size() > MaxDepletedDeposits ||
                world.felledTrees.size() > MaxGatheredObjects ||
                world.strippedTrees.size() > MaxGatheredObjects ||
                world.collectedPebbles.size() > MaxGatheredObjects ||
                !CampfireWorld::Validate(world.campfires) ||
                !ClayFurnaceWorld::Validate(world.furnaces) ||
                !BuildWorld::Validate(world.structures) ||
                !OreCrusherWorld::Validate(world.crushers) ||
                !PrimitiveProcessingMachines::Validate(world.processingMachines) ||
                !std::isfinite(world.moldCoolingSeconds) ||
                world.moldCoolingSeconds < 0.0f ||
                world.moldCoolingSeconds > 30.001f ||
                !PlayerInventory::IsValidState(world.inventory) ||
                !PlayerInventory::IsValidState(world.storage) ||
                world.selectedQuickbar >= ItemLayout::BarSize ||
                !FinitePosition(world.x) || !FinitePosition(world.y) ||
                !FinitePosition(world.z) || !FinitePosition(world.yaw) ||
                !FinitePosition(world.pitch) || std::abs(world.pitch) > 90.1f)
                return false;

            for (const VoxelEdit& edit : world.edits)
            {
                if (!FinitePosition(edit.x) || !FinitePosition(edit.y) ||
                    !FinitePosition(edit.z) || !std::isfinite(edit.radius) ||
                    !std::isfinite(edit.strength) || edit.radius <= 0.0f ||
                    edit.radius > 1000.0f || edit.strength <= 0.0f ||
                    edit.strength > 1000.0f)
                    return false;
            }
            const auto validUniqueIds = [](const std::vector<std::uint64_t>& ids)
            {
                std::unordered_set<std::uint64_t> seen;
                for (const auto id : ids)
                {
                    if (id == 0 || !seen.insert(id).second)
                        return false;
                }
                return true;
            };
            const auto validGrid = [](const ItemLayout::Grid& grid)
            {
                std::unordered_set<std::string> ids;
                for (const auto& id : grid)
                {
                    if (id.size() > 120 || id.find('\n') != std::string::npos ||
                        id.find('\r') != std::string::npos ||
                        (!id.empty() && !ids.insert(id).second))
                        return false;
                }
                return true;
            };
            for (const auto& id : world.quickbar)
            {
                if (id.size() > 80 || id.find('\n') != std::string::npos ||
                    id.find('\r') != std::string::npos)
                    return false;
            }
            return validGrid(world.bagSlots) &&
                validGrid(world.storageSlots) &&
                validUniqueIds(world.felledTrees) &&
                validUniqueIds(world.strippedTrees) &&
                validUniqueIds(world.collectedPebbles);
        }

        bool WriteText(const std::filesystem::path& file, const std::string& value)
        {
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            if (!out)
                return false;
            out.write(value.data(), static_cast<std::streamsize>(value.size()));
            out.flush();
            if (!out)
                return false;
            out.close();
            return static_cast<bool>(out);
        }

        std::string SanitizeName(std::string name)
        {
            std::string clean;
            for (unsigned char c : name)
            {
                if (c >= 32 && c < 127 && c != '\\' && c != '"')
                    clean.push_back(static_cast<char>(c));
                if (clean.size() >= 40)
                    break;
            }
            const auto begin = clean.find_first_not_of(' ');
            if (begin == std::string::npos)
                return {};
            const auto end = clean.find_last_not_of(' ');
            return clean.substr(begin, end - begin + 1);
        }
    }

    bool SaveManager::Initialize()
    {
        char* pref = SDL_GetPrefPath("CyberTraceStudios", "IndustrialFrontier");
        if (!pref)
            return false;
        m_directory = std::filesystem::path(pref) / "saves";
        SDL_free(pref);
        std::error_code ec;
        std::filesystem::create_directories(m_directory, ec);
        return !ec && std::filesystem::is_directory(m_directory);
    }

    const std::filesystem::path& SaveManager::GetDirectory() const
    {
        return m_directory;
    }

    bool SaveManager::ValidId(const std::string& id)
    {
        return !id.empty() && id.size() <= 48 &&
            std::all_of(id.begin(), id.end(), [](char c)
            {
                return (c >= 'a' && c <= 'z') ||
                       (c >= '0' && c <= '9') || c == '-';
            });
    }

    std::filesystem::path SaveManager::SlotPath(const std::string& id, int backupIndex) const
    {
        std::filesystem::path path = m_directory / (id + ".ifsave");
        if (backupIndex > 0)
            path += ".bak" + std::to_string(backupIndex);
        return path;
    }

    bool SaveManager::WriteLast(const std::string& id)
    {
        if (!ValidId(id))
            return false;
        const auto destination = m_directory / "last-slot.txt";
        const auto temp = m_directory / "last-slot.tmp";
        if (!WriteText(temp, id + "\n"))
            return false;
        std::error_code ec;
        std::filesystem::rename(temp, destination, ec);
        if (ec)
        {
            std::filesystem::remove(destination, ec);
            ec.clear();
            std::filesystem::rename(temp, destination, ec);
        }
        return !ec;
    }

    std::string SaveManager::ReadLast() const
    {
        std::ifstream in(m_directory / "last-slot.txt");
        std::string id;
        in >> id;
        return ValidId(id) ? id : std::string{};
    }

    bool SaveManager::WriteSlot(const std::string& id, const WorldSave& world,
                                std::string& error)
    {
        if (!ValidId(id) || !Validate(world))
        {
            error = "Invalid save data or save name";
            return false;
        }
        std::ostringstream out;
        out << std::setprecision(std::numeric_limits<float>::max_digits10);
        out << "IFSAVE 14\n";
        out << "NAME " << std::quoted(world.name) << '\n';
        out << "SEED " << world.seed << '\n';
        out << "PLAYER " << world.x << ' ' << world.y << ' ' << world.z
            << ' ' << world.yaw << ' ' << world.pitch << '\n';
        out << "EDITS " << world.edits.size() << '\n';
        for (const VoxelEdit& edit : world.edits)
            out << edit.x << ' ' << edit.y << ' ' << edit.z << ' '
                << edit.radius << ' ' << edit.strength << ' '
                << static_cast<int>(edit.addMaterial) << '\n';
        out << "DEPLETED " << world.depletedDeposits.size() << '\n';
        for (std::uint64_t depletedId : world.depletedDeposits)
        {
            out << depletedId << '\n';
        }
        for (const auto& section : {
            std::pair{"TREES", &world.felledTrees},
            std::pair{"BRANCHES", &world.strippedTrees},
            std::pair{"PEBBLES", &world.collectedPebbles}})
        {
            out << section.first << ' ' << section.second->size() << '\n';
            for (const std::uint64_t id : *section.second)
                out << id << '\n';
        }
        out << "CAMPFIRES " << world.campfires.size() << '\n';
        for (const auto& fire : world.campfires)
            out << "FIRE " << fire.id << ' ' << fire.x << ' ' << fire.y
                << ' ' << fire.z << ' ' << fire.fuelSeconds << ' '
                << fire.burnProgress << ' ' << fire.charcoalReady << ' '
                << static_cast<int>(fire.covered) << ' ' << fire.yawDegrees << '\n';
        out << "FURNACES " << world.furnaces.size() << '\n';
        for (const auto& f : world.furnaces)
            out << "FURNACE " << f.id << ' ' << f.x << ' ' << f.y << ' '
                << f.z << ' ' << f.fuelSeconds << ' ' << f.smeltSeconds << ' '
                << f.chargeCopperKg << ' ' << f.readyCopperKg << ' '
                << std::quoted(f.chargeName) << ' '
                << std::quoted(f.metalId) << ' ' << f.moldCapacity << ' '
                << f.castCount << ' ' << f.castingCoolingSeconds << ' '
                << f.yawDegrees << '\n';
        out << "MOLDCOOLING " << world.moldCoolingSeconds << '\n';
        out << "STRUCTURES " << world.structures.size() << '\n';
        for (const auto& s : world.structures)
        {
            out << "STRUCTURE " << s.id << ' ' << static_cast<int>(s.kind)
                << ' ' << s.x << ' ' << s.y << ' ' << s.z << ' '
                << s.yawDegrees << ' ' << s.contents.carryCapacityBonusKg
                << ' ' << s.contents.oreBatches.size() << ' '
                << s.contents.items.size() << '\n';
            for (const auto& batch : s.contents.oreBatches)
            {
                out << "CHEST_ORE " << batch.depositId << ' '
                    << std::quoted(batch.primaryMineralId) << ' '
                    << std::quoted(batch.displayName) << ' ' << batch.massKg << ' '
                    << batch.oreMineralFraction << ' ' << batch.components.size()
                    << ' ' << static_cast<int>(batch.processingStage) << '\n';
                for (const auto& component : batch.components)
                    out << "CHEST_MINERAL " << std::quoted(component.mineralId)
                        << ' ' << component.massFraction << ' '
                        << static_cast<int>(component.gangue) << '\n';
            }
            for (const auto& item : s.contents.items)
                out << "CHEST_ITEM " << std::quoted(item.itemId) << ' '
                    << item.quantity << ' ' << item.durability << '\n';
        }
        out << "CRUSHERS " << world.crushers.size() << '\n';
        for (const auto& crusher : world.crushers)
        {
            const auto& batch = crusher.batch;
            out << "CRUSHER " << crusher.id << ' ' << crusher.x << ' '
                << crusher.y << ' ' << crusher.z << ' '
                << crusher.yawDegrees << ' ' << crusher.workDone << ' '
                << crusher.workRequired << ' ' << crusher.rotationRadians << ' '
                << (batch.massKg > 0.0f ? 1 : 0) << '\n';
            if (batch.massKg > 0.0f)
            {
                out << "CRUSHER_ORE " << batch.depositId << ' '
                    << std::quoted(batch.primaryMineralId) << ' '
                    << std::quoted(batch.displayName) << ' ' << batch.massKg << ' '
                    << batch.oreMineralFraction << ' ' << batch.components.size()
                    << ' ' << static_cast<int>(batch.processingStage) << '\n';
                for (const auto& component : batch.components)
                    out << "CRUSHER_MINERAL " << std::quoted(component.mineralId)
                        << ' ' << component.massFraction << ' '
                        << static_cast<int>(component.gangue) << '\n';
            }
        }
        out << "MACHINES " << world.processingMachines.size() << '\n';
        for (const auto& m : world.processingMachines)
        {
            out << "MACHINE " << m.id << ' ' << static_cast<int>(m.kind) << ' '
                << m.x << ' ' << m.y << ' ' << m.z << ' '
                << m.yawDegrees << ' ' << m.workDone << ' '
                << m.workRequired << ' ' << m.rotationRadians << ' '
                << (m.feed.massKg>0.0f?1:0) << ' '
                << (m.tailings.massKg>0.0f?1:0) << '\n';
            if(m.feed.massKg>0.0f)
            {
                const auto& b=m.feed;
                out << "MACHINE_FEED " << b.depositId << ' '
                    << std::quoted(b.primaryMineralId) << ' '
                    << std::quoted(b.displayName) << ' ' << b.massKg << ' '
                    << b.oreMineralFraction << ' ' << b.components.size() << ' '
                    << static_cast<int>(b.processingStage) << '\n';
                for(const auto& c:b.components)
                    out << "MACHINE_MINERAL " << std::quoted(c.mineralId) << ' '
                        << c.massFraction << ' ' << static_cast<int>(c.gangue) << '\n';
            }
            if(m.tailings.massKg>0.0f)
            {
                const auto& b=m.tailings;
                out << "MACHINE_TAIL " << b.depositId << ' '
                    << std::quoted(b.primaryMineralId) << ' '
                    << std::quoted(b.displayName) << ' ' << b.massKg << ' '
                    << b.oreMineralFraction << ' ' << b.components.size() << ' '
                    << static_cast<int>(b.processingStage) << '\n';
                for(const auto& c:b.components)
                    out << "MACHINE_MINERAL " << std::quoted(c.mineralId) << ' '
                        << c.massFraction << ' ' << static_cast<int>(c.gangue) << '\n';
            }
        }
        out << "INVENTORY " << world.inventory.carryCapacityBonusKg << ' '
            << world.inventory.oreBatches.size() << '\n';
        for (const auto& batch : world.inventory.oreBatches)
        {
            out << "ORE " << batch.depositId << ' '
                << std::quoted(batch.primaryMineralId) << ' '
                << std::quoted(batch.displayName) << ' '
                << batch.massKg << ' ' << batch.oreMineralFraction << ' '
                << batch.components.size() << ' '
                << static_cast<int>(batch.processingStage) << '\n';
            for (const auto& component : batch.components)
            {
                out << "MINERAL " << std::quoted(component.mineralId) << ' '
                    << component.massFraction << ' '
                    << static_cast<int>(component.gangue) << '\n';
            }
        }
        out << "ITEMS " << world.inventory.items.size() << '\n';
        for (const auto& stack : world.inventory.items)
            out << "ITEM " << std::quoted(stack.itemId) << ' ' << stack.quantity
                << ' ' << stack.durability << '\n';
        out << "STORAGE " << world.storage.carryCapacityBonusKg << ' '
            << world.storage.oreBatches.size() << '\n';
        for (const auto& batch : world.storage.oreBatches)
        {
            out << "ORE " << batch.depositId << ' '
                << std::quoted(batch.primaryMineralId) << ' '
                << std::quoted(batch.displayName) << ' '
                << batch.massKg << ' ' << batch.oreMineralFraction << ' '
                << batch.components.size() << ' '
                << static_cast<int>(batch.processingStage) << '\n';
            for (const auto& component : batch.components)
            {
                out << "MINERAL " << std::quoted(component.mineralId) << ' '
                    << component.massFraction << ' '
                    << static_cast<int>(component.gangue) << '\n';
            }
        }
        out << "STORE_ITEMS " << world.storage.items.size() << '\n';
        for (const auto& stack : world.storage.items)
            out << "STORE_ITEM " << std::quoted(stack.itemId) << ' '
                << stack.quantity << ' ' << stack.durability << '\n';
        out << "BAGSLOTS " << world.bagSlots.size() << '\n';
        for (const auto& key : world.bagSlots)
            out << "SLOT " << std::quoted(key) << '\n';
        out << "STORESLOTS " << world.storageSlots.size() << '\n';
        for (const auto& key : world.storageSlots)
            out << "SLOT " << std::quoted(key) << '\n';
        out << "HOTBAR " << world.selectedQuickbar << ' '
            << world.quickbar.size() << '\n';
        for (const auto& id : world.quickbar)
            out << "QUICK " << std::quoted(id) << '\n';
        const std::string payload = out.str();
        std::ostringstream checksum;
        checksum << std::hex << Hash(payload);
        const std::string bytes = payload + "CHECKSUM " + checksum.str() + "\n";
        const auto path = SlotPath(id, 0);
        const auto temporary = m_directory / (id + ".ifsave.tmp");
        if (!WriteText(temporary, bytes))
        {
            error = "Could not write temporary save file";
            return false;
        }
        std::error_code ec;
        if (std::filesystem::exists(path, ec))
        {
            const auto older = SlotPath(id, 2);
            const auto backup = SlotPath(id, 1);
            std::filesystem::remove(older, ec);
            ec.clear();
            if (std::filesystem::exists(backup, ec))
            {
                std::filesystem::rename(backup, older, ec);
                if (ec)
                {
                    error = "Could not rotate older backup";
                    return false;
                }
            }
            ec.clear();
            std::filesystem::copy_file(path, backup,
                std::filesystem::copy_options::overwrite_existing, ec);
            if (ec)
            {
                error = "Could not preserve previous save backup";
                return false;
            }
            std::filesystem::remove(path, ec);
            if (ec)
            {
                error = "Could not replace previous save";
                return false;
            }
        }
        ec.clear();
        std::filesystem::rename(temporary, path, ec);
        if (ec)
        {
            error = "Could not finish saving; check backup files";
            return false;
        }
        WriteLast(id);
        return true;
    }

    bool SaveManager::SaveNew(const std::string& name, const WorldSave& world,
                              std::string& slotId, std::string& error)
    {
        WorldSave copy = world;
        copy.name = SanitizeName(name);
        if (copy.name.empty())
        {
            error = "Enter a save name";
            return false;
        }
        std::random_device random;
        const auto now = std::chrono::system_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 24; ++attempt)
        {
            std::ostringstream id;
            id << "slot-" << std::hex << static_cast<std::uint64_t>(now)
               << '-' << static_cast<std::uint32_t>(random());
            if (!std::filesystem::exists(SlotPath(id.str(), 0)))
            {
                if (!WriteSlot(id.str(), copy, error))
                    return false;
                slotId = id.str();
                return true;
            }
        }
        error = "Could not allocate a unique save slot";
        return false;
    }

    bool SaveManager::SaveAutosave(const WorldSave& world, std::string& error)
    {
        WorldSave copy = world;
        copy.name = "Autosave";
        return WriteSlot("autosave", copy, error);
    }

    bool SaveManager::DeleteSlot(const std::string& slotId, std::string& error)
    {
        if (!ValidId(slotId))
        {
            error = "Invalid save slot";
            return false;
        }

        const std::filesystem::path files[] =
        {
            SlotPath(slotId, 0),
            SlotPath(slotId, 1),
            SlotPath(slotId, 2),
            m_directory / (slotId + ".ifsave.tmp")
        };

        bool found = false;
        for (const auto& file : files)
        {
            std::error_code ec;
            if (std::filesystem::exists(file, ec))
            {
                if (!std::filesystem::is_regular_file(file, ec))
                {
                    error = "Save path is not a regular file";
                    return false;
                }
                found = true;
            }
            if (ec)
            {
                error = "Could not inspect save files";
                return false;
            }
        }
        if (!found)
        {
            error = "Save slot was not found";
            return false;
        }

        for (const auto& file : files)
        {
            std::error_code ec;
            std::filesystem::remove(file, ec);
            if (ec)
            {
                error = "Could not remove save file; check file permissions";
                return false;
            }
        }

        if (ReadLast() == slotId)
        {
            std::string fallback;
            for (const SaveSlotInfo& slot : List())
            {
                WorldSave world;
                std::string ignored;
                bool recovered = false;
                if (Load(slot.id, 0, world, recovered, ignored))
                {
                    fallback = slot.id;
                    break;
                }
            }
            if (fallback.empty())
            {
                std::error_code ec;
                std::filesystem::remove(m_directory / "last-slot.txt", ec);
                if (ec)
                {
                    error = "Save deleted, but recent-save pointer could not be cleared";
                    return false;
                }
            }
            else if (!WriteLast(fallback))
            {
                error = "Save deleted, but recent-save pointer could not be updated";
                return false;
            }
        }
        return true;
    }

    bool SaveManager::ReadFile(const std::filesystem::path& path, WorldSave& world) const
    {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec))
            return false;
        const std::uintmax_t size = std::filesystem::file_size(path, ec);
        if (ec || size == 0 || size > MaxFileBytes)
            return false;
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return false;
        const std::string bytes((std::istreambuf_iterator<char>(in)),
                                std::istreambuf_iterator<char>());
        const auto marker = bytes.rfind("CHECKSUM ");
        if (marker == std::string::npos || marker == 0 ||
            bytes[marker - 1] != '\n' || bytes.back() != '\n')
            return false;
        const std::string payload = bytes.substr(0, marker);
        std::istringstream check(bytes.substr(marker + 9));
        std::uint64_t expected = 0;
        check >> std::hex >> expected;
        if (!check || expected != Hash(payload))
            return false;
        std::istringstream data(payload);
        std::string tag;
        int version = 0;
        WorldSave loaded;
        if (!(data >> tag >> version) || tag != "IFSAVE" || (version != 1 && version != 2 && version != 3 && version != 4 && version != 5 && version != 6 && version != 7 && version != 8 && version != 9 && version != 10 && version != 11 && version != 12 && version != 13 && version != 14))
            return false;
        if (!(data >> tag) || tag != "NAME" || !(data >> std::quoted(loaded.name)))
            return false;
        if (!(data >> tag) || tag != "SEED" || !(data >> loaded.seed))
            return false;
        if (!(data >> tag) || tag != "PLAYER" ||
            !(data >> loaded.x >> loaded.y >> loaded.z >> loaded.yaw >> loaded.pitch))
            return false;
        std::size_t count = 0;
        if (!(data >> tag >> count) || tag != "EDITS" || count > MaxEdits)
            return false;
        loaded.edits.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            VoxelEdit edit{};
            int add = 0;
            if (!(data >> edit.x >> edit.y >> edit.z >> edit.radius >> edit.strength >> add) ||
                (add != 0 && add != 1))
                return false;
            edit.addMaterial = add != 0;
            loaded.edits.push_back(edit);
        }
        if (version >= 2)
        {
            std::size_t depletedCount = 0;
            if (!(data >> tag >> depletedCount) || tag != "DEPLETED" ||
                depletedCount > MaxDepletedDeposits)
            {
                return false;
            }
            loaded.depletedDeposits.reserve(depletedCount);
            std::unordered_set<std::uint64_t> seen;
            for (std::size_t i = 0; i < depletedCount; ++i)
            {
                std::uint64_t id = 0;
                if (!(data >> id) || id == 0 || !seen.insert(id).second)
                {
                    return false;
                }
                loaded.depletedDeposits.push_back(id);
            }
        }
        if (version >= 5)
        {
            const auto readIds = [&](const char* expectedTag,
                std::vector<std::uint64_t>& ids) -> bool
            {
                std::size_t itemCount = 0;
                if (!(data >> tag >> itemCount) || tag != expectedTag ||
                    itemCount > MaxGatheredObjects)
                    return false;
                std::unordered_set<std::uint64_t> unique;
                for (std::size_t i = 0; i < itemCount; ++i)
                {
                    std::uint64_t id = 0;
                    if (!(data >> id) || id == 0 || !unique.insert(id).second)
                        return false;
                    ids.push_back(id);
                }
                return true;
            };
            if (!readIds("TREES", loaded.felledTrees) ||
                !readIds("BRANCHES", loaded.strippedTrees) ||
                !readIds("PEBBLES", loaded.collectedPebbles))
                return false;
        }
        if (version >= 8)
        {
            std::size_t fireCount = 0;
            if (!(data >> tag >> fireCount) || tag != "CAMPFIRES" ||
                fireCount > CampfireWorld::MaxCampfires)
                return false;
            for (std::size_t i = 0; i < fireCount; ++i)
            {
                CampfireState fire{};
                int covered = -1;
                if (!(data >> tag) || tag != "FIRE" ||
                    !(data >> fire.id >> fire.x >> fire.y >> fire.z >>
                        fire.fuelSeconds >> fire.burnProgress >>
                        fire.charcoalReady >> covered) ||
                    (covered != 0 && covered != 1))
                    return false;
                if (version >= 11 && !(data >> fire.yawDegrees))
                    return false;
                fire.covered = covered == 1;
                loaded.campfires.push_back(fire);
            }
        }
        if (version >= 9)
        {
            std::size_t furnaceCount = 0;
            if (!(data >> tag >> furnaceCount) || tag != "FURNACES" ||
                furnaceCount > ClayFurnaceWorld::MaxFurnaces)
                return false;
            for (std::size_t i = 0; i < furnaceCount; ++i)
            {
                ClayFurnaceState f{};
                if (!(data >> tag) || tag != "FURNACE" ||
                    !(data >> f.id >> f.x >> f.y >> f.z >>
                        f.fuelSeconds >> f.smeltSeconds >>
                        f.chargeCopperKg >> f.readyCopperKg >>
                        std::quoted(f.chargeName)))
                    return false;
                if (version >= 10 &&
                    !(data >> std::quoted(f.metalId) >> f.moldCapacity >>
                        f.castCount >> f.castingCoolingSeconds))
                    return false;
                if (version >= 11 && !(data >> f.yawDegrees))
                    return false;
                loaded.furnaces.push_back(std::move(f));
            }
            if (!(data >> tag >> loaded.moldCoolingSeconds) ||
                tag != "MOLDCOOLING")
                return false;
        }
        if (version >= 12)
        {
            std::size_t structureCount = 0;
            if (!(data >> tag >> structureCount) || tag != "STRUCTURES" ||
                structureCount > BuildWorld::MaxStructures)
                return false;
            loaded.structures.reserve(structureCount);
            for (std::size_t i = 0; i < structureCount; ++i)
            {
                BuildStructure s;
                int kind = -1;
                std::size_t oreCount = 0;
                std::size_t itemCount = 0;
                if (!(data >> tag) || tag != "STRUCTURE" ||
                    !(data >> s.id >> kind >> s.x >> s.y >> s.z >>
                        s.yawDegrees >> s.contents.carryCapacityBonusKg >>
                        oreCount >> itemCount) || kind < 0 || kind > 6 ||
                    oreCount > PlayerInventory::MaxSlots ||
                    itemCount > PlayerInventory::MaxSlots)
                    return false;
                s.kind = static_cast<BuildKind>(kind);
                for (std::size_t j = 0; j < oreCount; ++j)
                {
                    InventoryOreBatch batch;
                    std::size_t mineralCount = 0;
                    int stage = -1;
                    if (!(data >> tag) || tag != "CHEST_ORE" ||
                        !(data >> batch.depositId >> std::quoted(batch.primaryMineralId)
                            >> std::quoted(batch.displayName) >> batch.massKg >>
                            batch.oreMineralFraction >> mineralCount >> stage) ||
                        mineralCount > 24 || stage < 0 || stage > 5)
                        return false;
                    batch.processingStage = static_cast<OreProcessingStage>(stage);
                    for (std::size_t k = 0; k < mineralCount; ++k)
                    {
                        InventoryMineralComponent component;
                        int gangue = -1;
                        if (!(data >> tag) || tag != "CHEST_MINERAL" ||
                            !(data >> std::quoted(component.mineralId) >>
                                component.massFraction >> gangue) ||
                            (gangue != 0 && gangue != 1))
                            return false;
                        component.gangue = gangue != 0;
                        batch.components.push_back(std::move(component));
                    }
                    s.contents.oreBatches.push_back(std::move(batch));
                }
                for (std::size_t j = 0; j < itemCount; ++j)
                {
                    InventoryItemStack stack;
                    unsigned durability = 0;
                    if (!(data >> tag) || tag != "CHEST_ITEM" ||
                        !(data >> std::quoted(stack.itemId) >> stack.quantity >>
                            durability) || durability > 65535)
                        return false;
                    stack.durability = static_cast<std::uint16_t>(durability);
                    s.contents.items.push_back(std::move(stack));
                }
                loaded.structures.push_back(std::move(s));
            }
        }
        if (version >= 13)
        {
            std::size_t count = 0;
            if (!(data >> tag >> count) || tag != "CRUSHERS" ||
                count > OreCrusherWorld::MaxCrushers)
                return false;
            for (std::size_t i = 0; i < count; ++i)
            {
                OreCrusherState crusher;
                int hasBatch = -1;
                if (!(data >> tag) || tag != "CRUSHER" ||
                    !(data >> crusher.id >> crusher.x >> crusher.y >> crusher.z >>
                      crusher.yawDegrees >> crusher.workDone >>
                      crusher.workRequired >> crusher.rotationRadians >> hasBatch) ||
                    (hasBatch != 0 && hasBatch != 1))
                    return false;
                if (hasBatch)
                {
                    auto& batch = crusher.batch;
                    std::size_t minerals = 0;
                    int stage = -1;
                    if (!(data >> tag) || tag != "CRUSHER_ORE" ||
                        !(data >> batch.depositId >> std::quoted(batch.primaryMineralId)
                          >> std::quoted(batch.displayName) >> batch.massKg >>
                          batch.oreMineralFraction >> minerals >> stage) ||
                        minerals > 24 || stage < 0 || stage > 5)
                        return false;
                    batch.processingStage = static_cast<OreProcessingStage>(stage);
                    for (std::size_t j = 0; j < minerals; ++j)
                    {
                        InventoryMineralComponent component;
                        int gangue = -1;
                        if (!(data >> tag) || tag != "CRUSHER_MINERAL" ||
                            !(data >> std::quoted(component.mineralId)
                              >> component.massFraction >> gangue) ||
                            (gangue != 0 && gangue != 1))
                            return false;
                        component.gangue = gangue != 0;
                        batch.components.push_back(std::move(component));
                    }
                }
                loaded.crushers.push_back(std::move(crusher));
            }
        }
        if (version >= 14)
        {
            std::size_t machineCount=0;
            if(!(data >> tag >> machineCount) || tag!="MACHINES" ||
                machineCount>PrimitiveProcessingMachines::MaxMachines)
                return false;
            for(std::size_t i=0;i<machineCount;++i)
            {
                PrimitiveMachineState m;
                int kind=-1,hasFeed=-1,hasTail=-1;
                if(!(data >> tag) || tag!="MACHINE" ||
                    !(data >> m.id >> kind >> m.x >> m.y >> m.z >>
                      m.yawDegrees >> m.workDone >> m.workRequired >>
                      m.rotationRadians >> hasFeed >> hasTail) ||
                    kind<0 || kind>1 || hasFeed<0 || hasFeed>1 ||
                    hasTail<0 || hasTail>1)
                    return false;
                m.kind=static_cast<PrimitiveMachineKind>(kind);
                const auto readBatch=[&](const char* expected, InventoryOreBatch& b)
                {
                    std::size_t count=0;
                    int stage=-1;
                    if(!(data >> tag) || tag!=expected ||
                        !(data >> b.depositId >> std::quoted(b.primaryMineralId) >>
                          std::quoted(b.displayName) >> b.massKg >>
                          b.oreMineralFraction >> count >> stage) ||
                        count>24 || stage<0 || stage>5)
                        return false;
                    b.processingStage=static_cast<OreProcessingStage>(stage);
                    for(std::size_t k=0;k<count;++k)
                    {
                        InventoryMineralComponent c;
                        int gangue=-1;
                        if(!(data >> tag) || tag!="MACHINE_MINERAL" ||
                            !(data >> std::quoted(c.mineralId) >> c.massFraction >> gangue) ||
                            gangue<0 || gangue>1)
                            return false;
                        c.gangue=gangue!=0;
                        b.components.push_back(std::move(c));
                    }
                    return true;
                };
                if(hasFeed && !readBatch("MACHINE_FEED",m.feed))return false;
                if(hasTail && !readBatch("MACHINE_TAIL",m.tailings))return false;
                loaded.processingMachines.push_back(std::move(m));
            }
        }
        if (version >= 3)
        {
            std::size_t slotCount = 0;
            if (!(data >> tag >> loaded.inventory.carryCapacityBonusKg >> slotCount) ||
                tag != "INVENTORY" || slotCount > PlayerInventory::MaxSlots)
            {
                return false;
            }
            loaded.inventory.oreBatches.reserve(slotCount);
            for (std::size_t i = 0; i < slotCount; ++i)
            {
                InventoryOreBatch batch;
                std::size_t mineralCount = 0;
                if (!(data >> tag) || tag != "ORE" ||
                    !(data >> batch.depositId >> std::quoted(batch.primaryMineralId)
                        >> std::quoted(batch.displayName) >> batch.massKg
                        >> batch.oreMineralFraction >> mineralCount) ||
                    mineralCount > 24)
                {
                    return false;
                }
                if (version >= 4)
                {
                    int stage = -1;
                    if (!(data >> stage) || stage < 0 || stage > 5)
                        return false;
                    batch.processingStage = static_cast<OreProcessingStage>(stage);
                }
                batch.components.reserve(mineralCount);
                for (std::size_t j = 0; j < mineralCount; ++j)
                {
                    InventoryMineralComponent component;
                    int gangue = -1;
                    if (!(data >> tag) || tag != "MINERAL" ||
                        !(data >> std::quoted(component.mineralId)
                            >> component.massFraction >> gangue) ||
                        (gangue != 0 && gangue != 1))
                    {
                        return false;
                    }
                    component.gangue = gangue != 0;
                    batch.components.push_back(std::move(component));
                }
                loaded.inventory.oreBatches.push_back(std::move(batch));
            }
        }
        if (version >= 5)
        {
            std::size_t itemCount = 0;
            if (!(data >> tag >> itemCount) || tag != "ITEMS" ||
                itemCount > PlayerInventory::MaxSlots)
                return false;
            for (std::size_t i = 0; i < itemCount; ++i)
            {
                InventoryItemStack stack;
                if (!(data >> tag) || tag != "ITEM" ||
                    !(data >> std::quoted(stack.itemId) >> stack.quantity))
                    return false;
                if (version >= 7)
                {
                    unsigned value = 0;
                    if (!(data >> value) || value > 65535)
                        return false;
                    stack.durability = static_cast<std::uint16_t>(value);
                }
                else if (const auto* item = FindPrimitiveItem(stack.itemId))
                    stack.durability = item->maximumDurability;
                loaded.inventory.items.push_back(std::move(stack));
            }
        }
        if (version >= 6)
        {
            std::size_t oreCount = 0;
            if (!(data >> tag >> loaded.storage.carryCapacityBonusKg >> oreCount) ||
                tag != "STORAGE" || oreCount > PlayerInventory::MaxSlots)
                return false;
            for (std::size_t i = 0; i < oreCount; ++i)
            {
                InventoryOreBatch batch;
                std::size_t mineralCount = 0;
                int stage = -1;
                if (!(data >> tag) || tag != "ORE" ||
                    !(data >> batch.depositId >> std::quoted(batch.primaryMineralId)
                        >> std::quoted(batch.displayName) >> batch.massKg
                        >> batch.oreMineralFraction >> mineralCount >> stage) ||
                    mineralCount > 24 || stage < 0 || stage > 5)
                    return false;
                batch.processingStage = static_cast<OreProcessingStage>(stage);
                for (std::size_t j = 0; j < mineralCount; ++j)
                {
                    InventoryMineralComponent component;
                    int gangue = -1;
                    if (!(data >> tag) || tag != "MINERAL" ||
                        !(data >> std::quoted(component.mineralId)
                            >> component.massFraction >> gangue) ||
                        (gangue != 0 && gangue != 1))
                        return false;
                    component.gangue = gangue != 0;
                    batch.components.push_back(std::move(component));
                }
                loaded.storage.oreBatches.push_back(std::move(batch));
            }
            std::size_t itemCount = 0;
            if (!(data >> tag >> itemCount) || tag != "STORE_ITEMS" ||
                itemCount > PlayerInventory::MaxSlots)
                return false;
            for (std::size_t i = 0; i < itemCount; ++i)
            {
                InventoryItemStack stack;
                if (!(data >> tag) || tag != "STORE_ITEM" ||
                    !(data >> std::quoted(stack.itemId) >> stack.quantity))
                    return false;
                if (version >= 7)
                {
                    unsigned value = 0;
                    if (!(data >> value) || value > 65535)
                        return false;
                    stack.durability = static_cast<std::uint16_t>(value);
                }
                else if (const auto* item = FindPrimitiveItem(stack.itemId))
                    stack.durability = item->maximumDurability;
                loaded.storage.items.push_back(std::move(stack));
            }
            std::size_t gridCount = 0;
            if (!(data >> tag >> gridCount) || tag != "BAGSLOTS" ||
                gridCount != loaded.bagSlots.size())
                return false;
            for (auto& key : loaded.bagSlots)
                if (!(data >> tag) || tag != "SLOT" ||
                    !(data >> std::quoted(key)))
                    return false;
            if (!(data >> tag >> gridCount) || tag != "STORESLOTS" ||
                gridCount != loaded.storageSlots.size())
                return false;
            for (auto& key : loaded.storageSlots)
                if (!(data >> tag) || tag != "SLOT" ||
                    !(data >> std::quoted(key)))
                    return false;
            if (!(data >> tag >> loaded.selectedQuickbar >> gridCount) ||
                tag != "HOTBAR" || gridCount != loaded.quickbar.size())
                return false;
            for (auto& id : loaded.quickbar)
                if (!(data >> tag) || tag != "QUICK" ||
                    !(data >> std::quoted(id)))
                    return false;
        }
        if (!Validate(loaded))
            return false;
        world = std::move(loaded);
        return true;
    }

    bool SaveManager::Load(const std::string& slotId, int backupIndex,
                           WorldSave& world, bool& recovered, std::string& error) const
    {
        if (!ValidId(slotId) || backupIndex < 0 || backupIndex > 2)
        {
            error = "Invalid save slot";
            return false;
        }
        if (ReadFile(SlotPath(slotId, backupIndex), world))
        {
            recovered = backupIndex > 0;
            return true;
        }
        if (backupIndex == 0)
        {
            for (int i = 1; i <= 2; ++i)
            {
                if (ReadFile(SlotPath(slotId, i), world))
                {
                    recovered = true;
                    return true;
                }
            }
        }
        error = "Save damaged or missing; no valid backup found";
        return false;
    }

    std::vector<SaveSlotInfo> SaveManager::List() const
    {
        std::vector<SaveSlotInfo> result;
        std::error_code ec;
        if (!std::filesystem::is_directory(m_directory, ec))
            return result;
        std::unordered_set<std::string> ids;
        for (const auto& entry : std::filesystem::directory_iterator(m_directory, ec))
        {
            if (ec || !entry.is_regular_file())
                continue;
            const std::string filename = entry.path().filename().string();
            const auto marker = filename.find(".ifsave");
            if (marker == std::string::npos)
                continue;
            const std::string suffix = filename.substr(marker);
            if (suffix != ".ifsave" && suffix != ".ifsave.bak1" &&
                suffix != ".ifsave.bak2")
                continue;
            const std::string id = filename.substr(0, marker);
            if (ValidId(id)) ids.insert(id);
        }
        for (const std::string& id : ids)
        {
            SaveSlotInfo slot{};
            slot.id = id;
            const auto primary = SlotPath(id, 0);
            const auto backup1 = SlotPath(id, 1);
            const auto backup2 = SlotPath(id, 2);
            WorldSave world;
            slot.primaryValid = ReadFile(primary, world);
            slot.backupAvailable = std::filesystem::exists(backup1);
            slot.olderBackupAvailable = std::filesystem::exists(backup2);
            if (!slot.primaryValid && slot.backupAvailable)
                ReadFile(backup1, world);
            if (world.name.empty() && slot.olderBackupAvailable)
                ReadFile(backup2, world);
            slot.name = world.name.empty() ? "Damaged Save" : world.name;
            slot.automatic = id == "autosave";
            const auto primaryOrBackup = std::filesystem::exists(primary) ? primary :
                (slot.backupAvailable ? backup1 : backup2);
            slot.modified = std::filesystem::last_write_time(primaryOrBackup, ec);
            result.push_back(std::move(slot));
        }
        std::sort(result.begin(), result.end(), [](const SaveSlotInfo& a,
                                                    const SaveSlotInfo& b)
        {
            return a.modified > b.modified;
        });
        return result;
    }

    void SaveManager::MarkLastUsed(const std::string& id)
    {
        WriteLast(id);
    }

    bool SaveManager::Continue(WorldSave& world, std::string& slotId,
                               bool& recovered, std::string& error) const
    {
        const std::string last = ReadLast();
        if (!last.empty() && Load(last, 0, world, recovered, error))
        {
            slotId = last;
            return true;
        }
        for (const auto& slot : List())
        {
            if (slot.id == last)
                continue;
            if (Load(slot.id, 0, world, recovered, error))
            {
                slotId = slot.id;
                return true;
            }
        }
        error = "No valid saved game was found";
        return false;
    }
}
