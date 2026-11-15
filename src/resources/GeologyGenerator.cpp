#include "industrial_frontier/resources/GeologyGenerator.hpp"
#include "industrial_frontier/world/TerrainGenerator.hpp"
#include "industrial_frontier/world/VoxelTerrain.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>
#include <utility>

namespace industrial_frontier
{
    namespace
    {
        constexpr float OutcropCellSize = 104.0f;
        constexpr float PermanentCellSize = 272.0f;
        constexpr float CaveCellSize = 256.0f;
        constexpr float CaveOreSearchPadding = 150.0f;
        constexpr float PermanentCaveClearance = 98.0f;

        std::uint32_t Mix(std::uint32_t value)
        {
            value ^= value >> 16;
            value *= 0x7feb352du;
            value ^= value >> 15;
            value *= 0x846ca68bu;
            value ^= value >> 16;
            return value;
        }

        std::uint32_t CellHash(int x, int y, int z, std::uint32_t seed)
        {
            std::uint32_t hash = Mix(seed + 0x9e3779b9u);
            hash = Mix(hash ^ (static_cast<std::uint32_t>(x) * 0x9e3779b9u));
            hash = Mix(hash ^ (static_cast<std::uint32_t>(y) * 0x85ebca6bu));
            return Mix(hash ^ (static_cast<std::uint32_t>(z) * 0xc2b2ae35u));
        }

        float UnitRandom(std::uint32_t value)
        {
            return static_cast<float>(Mix(value) & 0x00ffffffu) /
                16777215.0f;
        }

        std::uint64_t StableId(int x, int y, int z, int seed, DepositKind kind)
        {
            const std::uint32_t salt = static_cast<std::uint32_t>(kind);
            const std::uint32_t a = CellHash(x, y, z,
                static_cast<std::uint32_t>(seed) ^ (salt * 0x1237da35u));
            const std::uint32_t b = CellHash(z, x, y,
                static_cast<std::uint32_t>(seed) ^ 0xca81b4e3u ^ salt);
            return (static_cast<std::uint64_t>(a) << 32) | b;
        }

        void SetOreColor(GeologicalDeposit& deposit,
            const MineralDefinition& mineral)
        {
            std::array<float, 3> color = {0.52f, 0.53f, 0.51f};
            const std::string& commodity = mineral.commodity;
            if (commodity == "Iron") color = {0.62f, 0.31f, 0.24f};
            else if (commodity == "Copper") color = {0.74f, 0.46f, 0.24f};
            else if (commodity == "Gold") color = {0.87f, 0.72f, 0.27f};
            else if (commodity == "Silver") color = {0.76f, 0.78f, 0.79f};
            else if (commodity == "Coal") color = {0.19f, 0.20f, 0.22f};
            else if (commodity == "Fuel") color = {0.18f, 0.19f, 0.20f};
            else if (commodity == "Lithium") color = {0.63f, 0.51f, 0.71f};
            else if (commodity == "Uranium") color = {0.51f, 0.62f, 0.25f};
            else if (commodity == "Aluminum") color = {0.71f, 0.60f, 0.48f};
            else if (commodity == "Lead") color = {0.46f, 0.50f, 0.56f};
            else if (commodity == "Zinc") color = {0.61f, 0.63f, 0.67f};
            else if (commodity == "Tin") color = {0.66f, 0.67f, 0.65f};
            else if (commodity == "Gemstones") color = {0.29f, 0.62f, 0.68f};
            else if (commodity == "Industrial Minerals") color = {0.58f, 0.62f, 0.63f};
            if (mineral.id == "quartz") color = {0.84f, 0.83f, 0.79f};
            else if (mineral.id == "malachite") color = {0.23f, 0.65f, 0.35f};
            else if (mineral.id == "azurite") color = {0.20f, 0.37f, 0.77f};
            else if (mineral.id == "magnetite") color = {0.29f, 0.30f, 0.33f};
            else if (mineral.id == "pyrite") color = {0.77f, 0.69f, 0.36f};
            deposit.color[0] = color[0];
            deposit.color[1] = color[1];
            deposit.color[2] = color[2];
        }

        float DistanceToSegmentSquared(float x, float y, float z,
            float ax, float ay, float az,
            float bx, float by, float bz)
        {
            const float dx = bx - ax;
            const float dy = by - ay;
            const float dz = bz - az;
            const float squaredLength = dx * dx + dy * dy + dz * dz;
            const float t = squaredLength > 1e-5f
                ? std::clamp(((x - ax) * dx + (y - ay) * dy +
                    (z - az) * dz) / squaredLength, 0.0f, 1.0f)
                : 0.0f;
            const float vx = x - (ax + t * dx);
            const float vy = y - (ay + t * dy);
            const float vz = z - (az + t * dz);
            return vx * vx + vy * vy + vz * vz;
        }
    }

    GeologyGenerator::GeologyGenerator(int worldSeed,
        const TerrainGenerator* terrainGenerator,
        const VoxelTerrain* voxelTerrain)
        : m_worldSeed(worldSeed),
          m_terrainGenerator(terrainGenerator),
          m_voxelTerrain(voxelTerrain)
    {
    }

    bool GeologyGenerator::LoadCatalog(
        const std::filesystem::path& path, std::string& error)
    {
        return m_database.LoadFromFile(path, error);
    }

    bool GeologyGenerator::IsReady() const
    {
        return m_database.Count() != 0;
    }

    std::size_t GeologyGenerator::MineralCount() const
    {
        return m_database.Count();
    }

    const MineralDefinition* GeologyGenerator::FindMineral(
        const std::string& id) const
    {
        return m_database.FindById(id);
    }

    void GeologyGenerator::AssignComposition(
        GeologicalDeposit& deposit, std::uint32_t siteHash, float depth) const
    {
        struct Candidate
        {
            const MineralDefinition* definition;
            float weight;
        };

        constexpr std::array<std::string_view, 5> GeologyTypes = {
            "igneous", "sedimentary", "hydrothermal", "metamorphic", "weathered"
        };
        const std::string_view host = GeologyTypes[
            Mix(siteHash ^ 0x3b48a112u) % GeologyTypes.size()];
        std::vector<Candidate> eligible;
        float totalWeight = 0.0f;
        for (const MineralDefinition& mineral : m_database.All())
        {
            if (depth < mineral.minimumDepthMeters ||
                depth > mineral.maximumDepthMeters ||
                mineral.spawnWeight <= 0.0f)
            {
                continue;
            }
            if (mineral.kind == ResourceKind::Rock &&
                mineral.commodity != "Aluminum")
            {
                continue;
            }
            if (mineral.kind != ResourceKind::Rock &&
                mineral.kind != ResourceKind::Mineral &&
                mineral.kind != ResourceKind::MineralGroup &&
                mineral.kind != ResourceKind::NativeElement &&
                mineral.kind != ResourceKind::FuelRock)
            {
                continue;
            }
            const bool hostMatches = std::any_of(
                mineral.hostGeologies.begin(), mineral.hostGeologies.end(),
                [host](const std::string& value) { return value == host; });
            if (!hostMatches)
            {
                continue;
            }
            const float weight = mineral.spawnWeight;
            eligible.push_back({&mineral, weight});
            totalWeight += weight;
        }

        if (eligible.empty())
        {
            return;
        }

        float draw = UnitRandom(siteHash + 0x124u) * totalWeight;
        const MineralDefinition* primary = eligible.back().definition;
        for (const Candidate& candidate : eligible)
        {
            draw -= candidate.weight;
            if (draw <= 0.0f)
            {
                primary = candidate.definition;
                break;
            }
        }

        deposit.mineralId = primary->id;
        deposit.components.clear();
        deposit.gangue.clear();
        deposit.hostGeology = std::string(host);
        deposit.components.push_back({primary->id, 1.0f});
        SetOreColor(deposit, *primary);

        if (UnitRandom(siteHash + 0x22eu) < 0.50f)
        {
            float secondaryTotal = 0.0f;
            std::vector<Candidate> associated;
            const auto IsAssociated = [](std::string_view main,
                std::string_view other)
            {
                if (main == "chalcopyrite")
                    return other == "bornite" || other == "chalcocite" ||
                        other == "pyrite";
                if (main == "bornite")
                    return other == "chalcopyrite" || other == "chalcocite" ||
                        other == "pyrite";
                if (main == "galena")
                    return other == "sphalerite" || other == "pyrite";
                if (main == "sphalerite")
                    return other == "galena" || other == "pyrite";
                if (main == "magnetite")
                    return other == "hematite" || other == "ilmenite";
                if (main == "hematite")
                    return other == "magnetite" || other == "goethite";
                if (main == "cassiterite")
                    return other == "wolframite" || other == "scheelite";
                return false;
            };
            for (const Candidate& candidate : eligible)
            {
                if (candidate.definition->id == primary->id ||
                    (candidate.definition->commodity != primary->commodity &&
                        !IsAssociated(primary->id, candidate.definition->id)))
                {
                    continue;
                }
                associated.push_back(candidate);
                secondaryTotal += candidate.weight;
            }
            if (!associated.empty())
            {
                float select = UnitRandom(siteHash + 0x34du) * secondaryTotal;
                const MineralDefinition* secondary = associated.back().definition;
                for (const Candidate& candidate : associated)
                {
                    select -= candidate.weight;
                    if (select <= 0.0f)
                    {
                        secondary = candidate.definition;
                        break;
                    }
                }
                const float fraction = 0.15f +
                    UnitRandom(siteHash + 0x26bu) * 0.28f;
                deposit.components[0].fraction = 1.0f - fraction;
                deposit.components.push_back({secondary->id, fraction});
            }
        }

        if ((primary->id == "chalcopyrite" || primary->id == "bornite") &&
            UnitRandom(siteHash + 0x36bu) < 0.52f &&
            std::none_of(deposit.components.begin(), deposit.components.end(),
                [](const DepositComponent& component)
                {
                    return component.mineralId == "pyrite";
                }))
        {
            const MineralDefinition* pyrite = m_database.FindById("pyrite");
            if (pyrite && depth >= pyrite->minimumDepthMeters &&
                depth <= pyrite->maximumDepthMeters &&
                std::find(pyrite->hostGeologies.begin(),
                    pyrite->hostGeologies.end(), host) != pyrite->hostGeologies.end())
            {
                const float fraction = 0.06f +
                    0.11f * UnitRandom(siteHash + 0x39fu);
                for (DepositComponent& component : deposit.components)
                {
                    component.fraction *= 1.0f - fraction;
                }
                deposit.components.push_back({"pyrite", fraction});
            }
        }

        const float gangueFraction = 1.0f - deposit.grade;
        if (gangueFraction > 0.0f)
        {
            std::string first = "quartz";
            std::string second = host == "sedimentary" ||
                host == "weathered" ? "calcite" : "dolomite";
            if (primary->id == first ||
                std::any_of(deposit.components.begin(), deposit.components.end(),
                    [&first](const DepositComponent& c)
                    {
                        return c.mineralId == first;
                    }))
            {
                first = "calcite";
                second = "dolomite";
            }
            if (primary->id == second || first == second ||
                std::any_of(deposit.components.begin(), deposit.components.end(),
                    [&second](const DepositComponent& c)
                    {
                        return c.mineralId == second;
                    }))
            {
                second = "fluorite";
            }
            const float firstPart = gangueFraction *
                (0.65f + 0.20f * UnitRandom(siteHash + 0x41bu));
            deposit.gangue.push_back({first, firstPart});
            deposit.gangue.push_back({second, gangueFraction - firstPart});
        }
    }

    GeologicalDeposit GeologyGenerator::GenerateSurfaceSite(
        int cellX, int cellZ, DepositKind kind) const
    {
        GeologicalDeposit site{};
        site.kind = kind;
        site.id = StableId(cellX, 0, cellZ, m_worldSeed, kind);
        if (!IsReady()) return site;

        const float cellSize = kind == DepositKind::FiniteOutcrop
            ? OutcropCellSize : PermanentCellSize;
        const std::uint32_t hash = CellHash(cellX,
            static_cast<int>(kind), cellZ,
            static_cast<std::uint32_t>(m_worldSeed) ^ 0x46cba211u);

        const float chance = kind == DepositKind::FiniteOutcrop ? 0.55f : 0.62f;
        if (UnitRandom(hash + 10u) > chance)
        {
            return site;
        }

        site.x = (static_cast<float>(cellX) + 0.5f) * cellSize +
            (UnitRandom(hash + 11u) - 0.5f) * cellSize * 0.42f;
        site.z = (static_cast<float>(cellZ) + 0.5f) * cellSize +
            (UnitRandom(hash + 12u) - 0.5f) * cellSize * 0.42f;
        site.radius = kind == DepositKind::FiniteOutcrop
            ? 2.3f + 2.0f * UnitRandom(hash + 13u)
            : 4.2f + 3.0f * UnitRandom(hash + 13u);
        if (m_voxelTerrain)
        {
            const float searchRadius = kind == DepositKind::PermanentNode
                ? 205.0f : 40.0f;
            const auto caves = m_voxelTerrain->FindCaveEntrances(
                site.x - searchRadius, site.x + searchRadius,
                site.z - searchRadius, site.z + searchRadius);
            for (const CaveEntrance& cave : caves)
            {
                const float exclusion = kind == DepositKind::PermanentNode
                    ? PermanentCaveClearance + site.radius
                    : 15.0f + site.radius;
                if (DistanceToSegmentSquared(site.x, 0.0f, site.z,
                    cave.x, 0.0f, cave.z,
                    cave.chamberX, 0.0f, cave.chamberZ) < exclusion * exclusion)
                {
                    return GeologicalDeposit{};
                }
            }
        }
        site.length = site.radius * 2.0f;
        site.grade = 0.09f + 0.55f * UnitRandom(hash + 14u);
        site.reserveKg = kind == DepositKind::FiniteOutcrop
            ? 250.0f + UnitRandom(hash + 19u) * 2750.0f
            : std::numeric_limits<float>::infinity();
        site.continuousExtraction = kind == DepositKind::PermanentNode;
        AssignComposition(site, hash + 17u, 0.0f);
        return site;
    }

    std::vector<GeologicalDeposit> GeologyGenerator::FindSurfaceDeposits(
        float minX, float maxX, float minZ, float maxZ) const
    {
        std::vector<GeologicalDeposit> deposits;
        if (!IsReady() || minX >= maxX || minZ >= maxZ) return deposits;
        for (DepositKind kind : {DepositKind::FiniteOutcrop,
            DepositKind::PermanentNode})
        {
            const float cellSize = kind == DepositKind::FiniteOutcrop
                ? OutcropCellSize : PermanentCellSize;
            const int firstX = static_cast<int>(std::floor(minX / cellSize)) - 1;
            const int finalX = static_cast<int>(std::floor(maxX / cellSize)) + 1;
            const int firstZ = static_cast<int>(std::floor(minZ / cellSize)) - 1;
            const int finalZ = static_cast<int>(std::floor(maxZ / cellSize)) + 1;
            for (int z = firstZ; z <= finalZ; ++z)
            {
                for (int x = firstX; x <= finalX; ++x)
                {
                    auto deposit = GenerateSurfaceSite(x, z, kind);
                    if (!deposit.mineralId.empty() &&
                        deposit.x >= minX && deposit.x < maxX &&
                        deposit.z >= minZ && deposit.z < maxZ)
                    {
                        deposits.push_back(std::move(deposit));
                    }
                }
            }
        }
        return deposits;
    }

    std::vector<GeologicalDeposit> GeologyGenerator::FindUndergroundVeins(
        float minX, float maxX,
        float minY, float maxY,
        float minZ, float maxZ) const
    {
        std::vector<GeologicalDeposit> deposits;
        if (!IsReady() || !m_voxelTerrain ||
            !std::isfinite(minX) || !std::isfinite(maxX) ||
            !std::isfinite(minY) || !std::isfinite(maxY) ||
            !std::isfinite(minZ) || !std::isfinite(maxZ) ||
            minX >= maxX || minY >= maxY || minZ >= maxZ)
        {
            return deposits;
        }

        const auto caves = m_voxelTerrain->FindCaveEntrances(
            minX - CaveOreSearchPadding, maxX + CaveOreSearchPadding,
            minZ - CaveOreSearchPadding, maxZ + CaveOreSearchPadding);

        for (const CaveEntrance& cave : caves)
        {
            const int caveCellX = static_cast<int>(std::floor(cave.x / CaveCellSize));
            const int caveCellZ = static_cast<int>(std::floor(cave.z / CaveCellSize));
            const std::uint32_t caveHash = CellHash(caveCellX, 0, caveCellZ,
                static_cast<std::uint32_t>(m_worldSeed) ^ 0x6c3e582bu);
            const float forwardX = cave.chamberX - cave.x;
            const float forwardZ = cave.chamberZ - cave.z;
            const float forwardLength = std::max(
                1.0f, std::sqrt(forwardX * forwardX + forwardZ * forwardZ));
            const float dx = forwardX / forwardLength;
            const float dz = forwardZ / forwardLength;

            for (int index = 0; index < 9; ++index)
            {
                const std::uint32_t hash = Mix(caveHash +
                    static_cast<std::uint32_t>(index) * 0x9e3779b9u);
                if (index >= 5 && UnitRandom(hash + 5u) > 0.86f)
                {
                    continue;
                }

                GeologicalDeposit vein{};
                vein.id = StableId(caveCellX, index + 1, caveCellZ,
                    m_worldSeed, DepositKind::UndergroundVein);
                vein.kind = DepositKind::UndergroundVein;
                vein.radius = 3.6f + UnitRandom(hash + 11u) * 2.0f;
                vein.length = 19.0f + UnitRandom(hash + 12u) * 14.0f;
                vein.grade = 0.07f + UnitRandom(hash + 13u) * 0.63f;
                vein.reserveKg = 1400.0f + UnitRandom(hash + 14u) * 53000.0f;

                if (index <= 5)
                {
                    const float angle =
                        (static_cast<float>(index) +
                            0.16f * UnitRandom(hash + 15u)) *
                        (6.28318530718f / 6.0f);
                    const float nx = dx * std::cos(angle) - dz * std::sin(angle);
                    const float nz = dz * std::cos(angle) + dx * std::sin(angle);
                    const float wallRadius = 17.2f +
                        (UnitRandom(hash + 16u) - 0.5f) * 2.8f;
                    vein.x = cave.chamberX + wallRadius * nx;
                    vein.z = cave.chamberZ + wallRadius * nz;
                    vein.y = cave.chamberY +
                        (static_cast<float>(index % 3) - 1.0f) * 3.0f;
                    vein.directionX = nx;
                    vein.directionY = (UnitRandom(hash + 17u) - 0.5f) * 0.25f;
                    vein.directionZ = nz;
                }
                else if (index == 6)
                {
                    vein.x = cave.chamberX + 4.5f * dx;
                    vein.y = cave.chamberY - 8.0f;
                    vein.z = cave.chamberZ + 4.5f * dz;
                    vein.directionX = dx;
                    vein.directionY = -0.32f;
                    vein.directionZ = dz;
                }
                else if (index == 7)
                {
                    vein.x = cave.chamberX - 3.5f * dz;
                    vein.y = cave.chamberY + 8.0f;
                    vein.z = cave.chamberZ + 3.5f * dx;
                    vein.directionX = -dz;
                    vein.directionY = 0.24f;
                    vein.directionZ = dx;
                }
                else
                {
                    vein.x = cave.x + forwardX * 0.17f - dz * 5.5f;
                    vein.y = cave.y + (cave.chamberY - cave.y) * 0.17f;
                    vein.z = cave.z + forwardZ * 0.17f + dx * 5.5f;
                    vein.directionX = dx;
                    vein.directionY = -0.16f;
                    vein.directionZ = dz;
                }

                const float reach = vein.length * 0.5f + vein.radius + 2.0f;
                if (vein.y < VoxelTerrain::BedrockSurfaceY + 8.0f ||
                    vein.x + reach < minX || vein.x - reach > maxX ||
                    vein.y + reach < minY || vein.y - reach > maxY ||
                    vein.z + reach < minZ || vein.z - reach > maxZ)
                {
                    continue;
                }

                const float depth = m_terrainGenerator
                    ? std::max(0.0f,
                        m_terrainGenerator->GetHeight(vein.x, vein.z) - vein.y)
                    : std::max(0.0f, cave.y - vein.y);
                AssignComposition(vein, hash + 17u, depth);
                if (!vein.mineralId.empty())
                {
                    deposits.push_back(std::move(vein));
                }
            }
        }
        return deposits;
    }

    std::vector<GeologicalDeposit> GeologyGenerator::FindUndergroundNodes(
        float, float, float, float, float, float) const
    {
        return {};
    }

    float GeologyGenerator::VeinInfluence(
        const GeologicalDeposit& deposit,
        float x, float y, float z)
    {
        if (deposit.kind != DepositKind::UndergroundVein)
        {
            return 0.0f;
        }
        const float halfLength = deposit.length * 0.5f;
        const float sx = deposit.directionX * halfLength;
        const float sy = deposit.directionY * halfLength;
        const float sz = deposit.directionZ * halfLength;
        const float distSquared = DistanceToSegmentSquared(x, y, z,
            deposit.x - sx, deposit.y - sy, deposit.z - sz,
            deposit.x + sx, deposit.y + sy, deposit.z + sz);
        const float irregular = 1.0f + 0.18f *
            std::sin(x * 0.25f + z * 0.17f + deposit.x) *
            std::sin(y * 0.28f + z * 0.23f);
        const float radius = deposit.radius * irregular;
        if (distSquared >= radius * radius) return 0.0f;
        return std::clamp(1.0f - std::sqrt(distSquared) / radius, 0.0f, 1.0f);
    }
}
