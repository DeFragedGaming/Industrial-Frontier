#pragma once

#include "industrial_frontier/resources/MineralDatabase.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace industrial_frontier
{
    struct GuideProcessingRoute
    {
        std::string id;
        std::string title;
        std::string products;
        std::string process;
        std::string notes;
    };

    class FieldGuideCatalog
    {
    public:
        bool LoadFromFile(const std::filesystem::path& path,
                          const MineralDatabase& minerals,
                          std::string& error);
        const GuideProcessingRoute* FindById(std::string_view id) const;
        const std::vector<GuideProcessingRoute>& All() const;
        std::size_t Count() const;
        std::vector<const GuideProcessingRoute*> Search(std::string_view query) const;

    private:
        std::vector<GuideProcessingRoute> m_routes;
        std::unordered_map<std::string, std::size_t> m_byId;
    };
}
