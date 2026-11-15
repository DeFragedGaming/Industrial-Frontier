#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace industrial_frontier
{
    enum class ResourceKind
    {
        Mineral,
        MineralGroup,
        NativeElement,
        Rock,
        FuelRock
    };

    struct MineralDefinition
    {
        std::string id;
        std::string name;
        ResourceKind kind = ResourceKind::Mineral;
        std::string formula;
        std::string commodity;
        std::vector<std::string> targetElements;
        std::vector<std::string> hostGeologies;
        float minimumDepthMeters = 0.0f;
        float maximumDepthMeters = 0.0f;
        float spawnWeight = 0.0f;
        bool hasMohsHardness = false;
        float minimumMohsHardness = 0.0f;
        float maximumMohsHardness = 0.0f;
        std::string processingRouteId;
        std::string processingOverview;
        std::string notes;
    };

    class MineralDatabase
    {
    public:
        bool LoadFromFile(
            const std::filesystem::path& path,
            std::string& error
        );

        const MineralDefinition* FindById(std::string_view id) const;
        const MineralDefinition* FindByName(std::string_view name) const;

        std::vector<const MineralDefinition*> FindByCommodity(
            std::string_view commodity
        ) const;

        std::vector<const MineralDefinition*> Search(
            std::string_view term
        ) const;

        const std::vector<MineralDefinition>& All() const;
        std::size_t Count() const;
        std::size_t CountByKind(ResourceKind kind) const;

    private:
        std::vector<MineralDefinition> m_entries;
        std::unordered_map<std::string, std::size_t> m_byId;
    };

    const char* ResourceKindName(ResourceKind kind);
}
