#include "industrial_frontier/resources/FieldGuideCatalog.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace industrial_frontier
{
    namespace
    {
        std::string Trim(const std::string& value)
        {
            const auto first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
            {
                return {};
            }
            const auto last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, last - first + 1);
        }

        std::vector<std::string> Split(const std::string& line)
        {
            std::vector<std::string> result;
            std::size_t start = 0;
            for (;;)
            {
                const std::size_t end = line.find('\t', start);
                result.push_back(Trim(line.substr(start, end == std::string::npos
                    ? std::string::npos : end - start)));
                if (end == std::string::npos)
                {
                    break;
                }
                start = end + 1;
            }
            return result;
        }

        std::string LowerAscii(std::string_view value)
        {
            std::string result(value);
            for (char& character : result)
            {
                const unsigned char c = static_cast<unsigned char>(character);
                if (c >= 'A' && c <= 'Z')
                {
                    character = static_cast<char>(c - 'A' + 'a');
                }
            }
            return result;
        }

        bool Contains(std::string_view text, std::string_view pattern)
        {
            return LowerAscii(text).find(LowerAscii(pattern)) != std::string::npos;
        }
    }

    bool FieldGuideCatalog::LoadFromFile(const std::filesystem::path& path,
        const MineralDatabase& minerals, std::string& error)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "Could not open Field Guide routes: " + path.string();
            return false;
        }

        std::string line;
        if (!std::getline(input, line) ||
            Trim(line) != "route_id\ttitle\tproducts\tprocess\tnotes")
        {
            error = "Incorrect Field Guide routes header";
            return false;
        }

        std::vector<GuideProcessingRoute> loaded;
        std::unordered_map<std::string, std::size_t> index;
        std::size_t lineNumber = 1;
        while (std::getline(input, line))
        {
            ++lineNumber;
            if (Trim(line).empty() || line.front() == '#')
            {
                continue;
            }
            auto fields = Split(line);
            if (fields.size() != 5 || fields[0].empty() || fields[1].empty() ||
                fields[2].empty() || fields[3].empty())
            {
                error = "Invalid processing route row " + std::to_string(lineNumber);
                return false;
            }
            for (char character : fields[0])
            {
                if ((character < 'a' || character > 'z') &&
                    (character < '0' || character > '9') && character != '_')
                {
                    error = "Invalid route ID on row " + std::to_string(lineNumber);
                    return false;
                }
            }
            if (!index.emplace(fields[0], loaded.size()).second)
            {
                error = "Duplicate processing route: " + fields[0];
                return false;
            }
            loaded.push_back({std::move(fields[0]), std::move(fields[1]),
                std::move(fields[2]), std::move(fields[3]), std::move(fields[4])});
        }
        if (!input.eof() && input.fail())
        {
            error = "Failed reading Field Guide routes";
            return false;
        }
        if (loaded.empty())
        {
            error = "No Field Guide processing routes found";
            return false;
        }
        for (const auto& mineral : minerals.All())
        {
            if (index.find(mineral.processingRouteId) == index.end())
            {
                error = "Missing processing route '" + mineral.processingRouteId +
                    "' referenced by " + mineral.id;
                return false;
            }
        }

        m_routes.swap(loaded);
        m_byId.swap(index);
        error.clear();
        return true;
    }

    const GuideProcessingRoute* FieldGuideCatalog::FindById(std::string_view id) const
    {
        const auto found = m_byId.find(std::string(id));
        if (found == m_byId.end())
        {
            return nullptr;
        }
        return &m_routes[found->second];
    }

    const std::vector<GuideProcessingRoute>& FieldGuideCatalog::All() const
    {
        return m_routes;
    }

    std::size_t FieldGuideCatalog::Count() const
    {
        return m_routes.size();
    }

    std::vector<const GuideProcessingRoute*> FieldGuideCatalog::Search(
        std::string_view query) const
    {
        std::vector<const GuideProcessingRoute*> result;
        for (const auto& route : m_routes)
        {
            if (Contains(route.title, query) || Contains(route.products, query) ||
                Contains(route.process, query) || Contains(route.id, query))
            {
                result.push_back(&route);
            }
        }
        return result;
    }
}
