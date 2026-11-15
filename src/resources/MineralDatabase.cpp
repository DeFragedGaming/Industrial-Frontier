#include "industrial_frontier/resources/MineralDatabase.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace industrial_frontier
{
    namespace
    {
        const std::string ExpectedHeader =
            "id\tname\tkind\tformula\tcommodity\ttarget_elements\t"
            "host_geologies\tdepth_min_m\tdepth_max_m\tspawn_weight\t"
            "hardness_min_mohs\thardness_max_mohs\tprocessing_route_id\t"
            "processing_overview\tnotes";

        std::string Trim(std::string text)
        {
            const auto whitespace = [](unsigned char c)
            {
                return c == ' ' || c == '\r' || c == '\n' || c == '\t';
            };

            auto first = std::find_if_not(text.begin(), text.end(), whitespace);
            auto last = std::find_if_not(text.rbegin(), text.rend(), whitespace).base();
            if (first >= last)
            {
                return {};
            }
            return std::string(first, last);
        }

        std::string Lower(std::string_view input)
        {
            std::string result(input);
            for (char& c : result)
            {
                const unsigned char value = static_cast<unsigned char>(c);
                if (value >= 'A' && value <= 'Z')
                {
                    c = static_cast<char>(value - 'A' + 'a');
                }
            }
            return result;
        }

        std::vector<std::string> SplitPreservingEmpty(
            const std::string& text,
            char delimiter)
        {
            std::vector<std::string> pieces;
            std::size_t start = 0;
            while (true)
            {
                const std::size_t end = text.find(delimiter, start);
                if (end == std::string::npos)
                {
                    pieces.push_back(Trim(text.substr(start)));
                    break;
                }
                pieces.push_back(Trim(text.substr(start, end - start)));
                start = end + 1;
            }
            return pieces;
        }

        std::vector<std::string> SplitValues(const std::string& text)
        {
            if (text.empty())
            {
                return {};
            }
            auto result = SplitPreservingEmpty(text, ',');
            result.erase(
                std::remove_if(result.begin(), result.end(),
                    [](const std::string& value) { return value.empty(); }),
                result.end()
            );
            return result;
        }

        bool ParseNumber(const std::string& text, float& result)
        {
            if (text.empty())
            {
                return false;
            }
            try
            {
                std::size_t position = 0;
                const float parsed = std::stof(text, &position);
                if (position != text.size() || !std::isfinite(parsed))
                {
                    return false;
                }
                result = parsed;
                return true;
            }
            catch (const std::exception&)
            {
                return false;
            }
        }

        bool ParseKind(const std::string& value, ResourceKind& kind)
        {
            if (value == "mineral") kind = ResourceKind::Mineral;
            else if (value == "mineral-group") kind = ResourceKind::MineralGroup;
            else if (value == "native-element") kind = ResourceKind::NativeElement;
            else if (value == "rock") kind = ResourceKind::Rock;
            else if (value == "fuel-rock") kind = ResourceKind::FuelRock;
            else return false;
            return true;
        }

        bool ValidIdentifier(std::string_view id)
        {
            if (id.empty()) return false;
            for (const char c : id)
            {
                if ((c >= 'a' && c <= 'z') ||
                    (c >= '0' && c <= '9') || c == '_')
                {
                    continue;
                }
                return false;
            }
            return true;
        }
    }

    const char* ResourceKindName(ResourceKind kind)
    {
        switch (kind)
        {
            case ResourceKind::Mineral: return "Mineral";
            case ResourceKind::MineralGroup: return "Mineral Group";
            case ResourceKind::NativeElement: return "Native Element";
            case ResourceKind::Rock: return "Rock";
            case ResourceKind::FuelRock: return "Fuel Rock";
        }
        return "Unknown";
    }

    bool MineralDatabase::LoadFromFile(
        const std::filesystem::path& path,
        std::string& error)
    {
        error.clear();
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            error = "Unable to open mineral database: " + path.string();
            return false;
        }

        std::string header;
        if (!std::getline(input, header))
        {
            error = "Mineral database is empty: " + path.string();
            return false;
        }

        if (header.size() >= 3 &&
            static_cast<unsigned char>(header[0]) == 0xEF &&
            static_cast<unsigned char>(header[1]) == 0xBB &&
            static_cast<unsigned char>(header[2]) == 0xBF)
        {
            header.erase(0, 3);
        }

        if (Trim(header) != ExpectedHeader)
        {
            error = "Invalid mineral database header: " + path.string();
            return false;
        }

        std::vector<MineralDefinition> loaded;
        std::unordered_map<std::string, std::size_t> index;
        std::string line;
        std::size_t lineNumber = 1;

        while (std::getline(input, line))
        {
            ++lineNumber;
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            if (Trim(line).empty() || line.front() == '#')
            {
                continue;
            }

            const auto columns = SplitPreservingEmpty(line, '\t');
            if (columns.size() != 15)
            {
                error = "Invalid column count on mineral row " +
                    std::to_string(lineNumber);
                return false;
            }

            MineralDefinition entry;
            entry.id = columns[0];
            entry.name = columns[1];
            entry.formula = columns[3];
            entry.commodity = columns[4];
            entry.targetElements = SplitValues(columns[5]);
            entry.hostGeologies = SplitValues(columns[6]);
            entry.processingRouteId = columns[12];
            entry.processingOverview = columns[13];
            entry.notes = columns[14];

            if (!ValidIdentifier(entry.id) || entry.name.empty() ||
                entry.commodity.empty() || entry.processingRouteId.empty() ||
                entry.processingOverview.empty() ||
                !ValidIdentifier(entry.processingRouteId) ||
                entry.hostGeologies.empty() ||
                !ParseKind(columns[2], entry.kind))
            {
                error = "Invalid mineral identity or category on row " +
                    std::to_string(lineNumber);
                return false;
            }

            if (!ParseNumber(columns[7], entry.minimumDepthMeters) ||
                !ParseNumber(columns[8], entry.maximumDepthMeters) ||
                !ParseNumber(columns[9], entry.spawnWeight) ||
                entry.minimumDepthMeters < 0.0f ||
                entry.minimumDepthMeters > entry.maximumDepthMeters ||
                entry.spawnWeight <= 0.0f || entry.spawnWeight > 1000.0f)
            {
                error = "Invalid depth or weight on mineral row " +
                    std::to_string(lineNumber);
                return false;
            }

            if (columns[10].empty() != columns[11].empty())
            {
                error = "Incomplete hardness range on mineral row " +
                    std::to_string(lineNumber);
                return false;
            }

            if (!columns[10].empty())
            {
                if (!ParseNumber(columns[10], entry.minimumMohsHardness) ||
                    !ParseNumber(columns[11], entry.maximumMohsHardness) ||
                    entry.minimumMohsHardness < 0.0f ||
                    entry.maximumMohsHardness > 10.0f ||
                    entry.minimumMohsHardness > entry.maximumMohsHardness)
                {
                    error = "Invalid Mohs hardness on mineral row " +
                        std::to_string(lineNumber);
                    return false;
                }
                entry.hasMohsHardness = true;
            }

            if (!index.emplace(entry.id, loaded.size()).second)
            {
                error = "Duplicate mineral ID '" + entry.id +
                    "' on row " + std::to_string(lineNumber);
                return false;
            }

            loaded.push_back(std::move(entry));
        }

        if (!input.eof() && input.fail())
        {
            error = "Error while reading mineral database: " + path.string();
            return false;
        }

        if (loaded.empty())
        {
            error = "Mineral database has no entries";
            return false;
        }

        m_entries.swap(loaded);
        m_byId.swap(index);
        return true;
    }

    const MineralDefinition* MineralDatabase::FindById(std::string_view id) const
    {
        const auto it = m_byId.find(std::string(id));
        return it == m_byId.end() ? nullptr : &m_entries[it->second];
    }

    const MineralDefinition* MineralDatabase::FindByName(std::string_view name) const
    {
        const std::string key = Lower(name);
        for (const auto& entry : m_entries)
        {
            if (Lower(entry.name) == key)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    std::vector<const MineralDefinition*> MineralDatabase::FindByCommodity(
        std::string_view commodity) const
    {
        const std::string key = Lower(commodity);
        std::vector<const MineralDefinition*> matches;
        for (const auto& entry : m_entries)
        {
            if (Lower(entry.commodity) == key)
            {
                matches.push_back(&entry);
            }
        }
        return matches;
    }

    std::vector<const MineralDefinition*> MineralDatabase::Search(
        std::string_view term) const
    {
        const std::string key = Lower(term);
        std::vector<const MineralDefinition*> matches;
        if (key.empty()) return matches;
        for (const auto& entry : m_entries)
        {
            if (Lower(entry.name).find(key) != std::string::npos ||
                Lower(entry.commodity).find(key) != std::string::npos ||
                Lower(entry.id).find(key) != std::string::npos)
            {
                matches.push_back(&entry);
            }
        }
        return matches;
    }

    const std::vector<MineralDefinition>& MineralDatabase::All() const
    {
        return m_entries;
    }

    std::size_t MineralDatabase::Count() const
    {
        return m_entries.size();
    }

    std::size_t MineralDatabase::CountByKind(ResourceKind kind) const
    {
        return static_cast<std::size_t>(std::count_if(
            m_entries.begin(), m_entries.end(),
            [kind](const MineralDefinition& entry) { return entry.kind == kind; }
        ));
    }
}
