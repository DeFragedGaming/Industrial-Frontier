#include "industrial_frontier/save/SaveManager.hpp"

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
            return true;
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
        out << "IFSAVE 1\n";
        out << "NAME " << std::quoted(world.name) << '\n';
        out << "SEED " << world.seed << '\n';
        out << "PLAYER " << world.x << ' ' << world.y << ' ' << world.z
            << ' ' << world.yaw << ' ' << world.pitch << '\n';
        out << "EDITS " << world.edits.size() << '\n';
        for (const VoxelEdit& edit : world.edits)
            out << edit.x << ' ' << edit.y << ' ' << edit.z << ' '
                << edit.radius << ' ' << edit.strength << ' '
                << static_cast<int>(edit.addMaterial) << '\n';
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
        if (!(data >> tag >> version) || tag != "IFSAVE" || version != 1)
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
