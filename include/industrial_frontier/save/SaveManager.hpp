#pragma once

#include "industrial_frontier/world/VoxelTerrain.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace industrial_frontier
{
    struct WorldSave
    {
        std::string name;
        int seed = 12345;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yaw = -90.0f;
        float pitch = 0.0f;
        std::vector<VoxelEdit> edits;
    };

    struct SaveSlotInfo
    {
        std::string id;
        std::string name;
        bool backupAvailable = false;
        bool olderBackupAvailable = false;
        bool primaryValid = false;
        bool automatic = false;
        std::filesystem::file_time_type modified{};
    };

    class SaveManager
    {
    public:
        bool Initialize();
        bool SaveNew(const std::string& name, const WorldSave& world,
                     std::string& slotId, std::string& error);
        bool SaveAutosave(const WorldSave& world, std::string& error);
        bool DeleteSlot(const std::string& slotId, std::string& error);
        bool Load(const std::string& slotId, int backupIndex,
                  WorldSave& world, bool& recovered, std::string& error) const;
        bool Continue(WorldSave& world, std::string& slotId,
                      bool& recovered, std::string& error) const;
        std::vector<SaveSlotInfo> List() const;
        void MarkLastUsed(const std::string& id);
        const std::filesystem::path& GetDirectory() const;

    private:
        std::filesystem::path m_directory;
        std::filesystem::path SlotPath(const std::string& slotId, int backupIndex) const;
        bool WriteSlot(const std::string& id, const WorldSave& world,
                       std::string& error);
        bool ReadFile(const std::filesystem::path& path, WorldSave& world) const;
        bool WriteLast(const std::string& id);
        std::string ReadLast() const;
        static bool ValidId(const std::string& id);
    };
}
