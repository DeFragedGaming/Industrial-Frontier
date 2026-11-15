#include "industrial_frontier/resources/GeologyGenerator.hpp"
#include "industrial_frontier/resources/OutcropInteraction.hpp"
#include "industrial_frontier/world/VoxelTerrain.hpp"
#include "industrial_frontier/world/VoxelMesher.hpp"

#include <cmath>
#include <iostream>
#include <string>
#include <unordered_set>

int main(int argc, char** argv)
{
    using namespace industrial_frontier;
    if (argc != 2) return 1;

    std::string error;
    GeologyGenerator geology(12345);
    if (!geology.LoadCatalog(argv[1], error) || geology.MineralCount() != 93)
    {
        std::cerr << error << '\n';
        return 2;
    }
    GeologyGenerator sameSeed(12345);
    if (!sameSeed.LoadCatalog(argv[1], error)) return 3;
    GeologyGenerator changedSeed(54321);
    if (!changedSeed.LoadCatalog(argv[1], error)) return 4;

    const auto originals = geology.FindSurfaceDeposits(-550, 550, -550, 550);
    const auto repeat = sameSeed.FindSurfaceDeposits(-550, 550, -550, 550);
    const auto changed = changedSeed.FindSurfaceDeposits(-550, 550, -550, 550);
    if (originals.empty() || originals.size() != repeat.size() ||
        (changed.size() == originals.size() &&
        !changed.empty() && changed.front().id == originals.front().id))
    {
        return 5;
    }

    std::unordered_set<std::uint64_t> ids;
    bool foundFinite = false;
    bool foundPermanent = false;
    bool mixed = false;
    for (std::size_t i = 0; i < originals.size(); ++i)
    {
        const auto& item = originals[i];
        if (item.id != repeat[i].id || item.mineralId != repeat[i].mineralId ||
            !geology.FindMineral(item.mineralId) || !ids.insert(item.id).second)
        {
            for (std::size_t j = 0; j < i; ++j) if (originals[j].id == item.id) std::cerr << "  prior j=" << j << " type=" << int(originals[j].kind) << " loc=" << originals[j].x << "," << originals[j].z << "\n";
            std::cerr << "  item type=" << int(item.kind) << " loc=" << item.x << "," << item.z << "\n";
            std::cerr << "Mismatch at " << i << " id " << item.id << " repeat " << repeat[i].id << " primary " << item.mineralId << " / " << repeat[i].mineralId << " find " << (geology.FindMineral(item.mineralId)!=nullptr) << " unique " << ids.size() << "\n";
            return 6;
        }
        float sum = 0.0f;
        for (const auto& component : item.components) sum += component.fraction;
        if (std::abs(sum - 1.0f) > 0.0001f) return 7;
        foundFinite |= item.kind == DepositKind::FiniteOutcrop && !item.continuousExtraction;
        foundPermanent |= item.kind == DepositKind::PermanentNode && item.continuousExtraction;
        mixed |= item.components.size() > 1;
    }
    if (!foundFinite || !foundPermanent || !mixed) return 8;

    VoxelTerrain terrain;
    const GeologyGenerator* liveGeology = terrain.GetGeology();
    if (!liveGeology || !liveGeology->IsReady()) return 11;
    const auto veins = liveGeology->FindUndergroundVeins(
        -600, 600, -79, 150, -600, 600);
    if (veins.empty()) return 9;
    for (const auto& vein : veins)
    {
        if (!liveGeology->FindMineral(vein.mineralId) ||
            vein.kind != DepositKind::UndergroundVein ||
            GeologyGenerator::VeinInfluence(vein, vein.x, vein.y, vein.z) < 0.5f)
        {
            return 10;
        }
    }

    const auto undergroundNodes = liveGeology->FindUndergroundNodes(
        -600, 600, -79, 150, -600, 600);
    if (!undergroundNodes.empty()) return 13;

    if (!terrain.GetGeology() || !terrain.GetGeology()->IsReady()) return 11;
    VoxelMesher mesher;
    VoxelMesh output;
    if (!mesher.GenerateChunk(terrain, 0, 0, output) || output.IsEmpty()) return 12;
    auto surfaceTest = terrain.GetGeology()->FindSurfaceDeposits(
        -300.0f, 300.0f, -300.0f, 300.0f);
    bool checkedOutcrop = false;
    for (const auto& site : surfaceTest)
    {
        if (site.kind != DepositKind::FiniteOutcrop) continue;
        const float y = terrain.GetSurfaceHeight(site.x, site.z);
        if (y > 125.0f || y < -50.0f ||
            terrain.SampleDensity(site.x, y - 1.0f, site.z) <= 0.0f) continue;
        const int cx = static_cast<int>(std::floor(site.x / terrain.GetChunkSize()));
        const int cz = static_cast<int>(std::floor(site.z / terrain.GetChunkSize()));
        VoxelMesh before;
        VoxelMesh after;
        if (!mesher.GenerateChunk(terrain, cx, cz, before)) return 15;
        OutcropCollisionShape collider{};
        if (!BuildOutcropCollisionShape(terrain, site, collider)) return 20;
        const auto hasCap = [&](const VoxelMesh& mesh, float capY)
        {
            for (const TerrainVertex& vertex : mesh.vertices)
            {
                if (std::abs(vertex.position[0] - site.x) < 0.01f &&
                    std::abs(vertex.position[2] - site.z) < 0.01f &&
                    std::abs(vertex.position[1] - capY) < 0.01f)
                {
                    return true;
                }
            }
            return false;
        };
        const float bottom = OutcropBottomY(collider);
        const float top = OutcropTopY(collider);
        if (!hasCap(before, bottom) || !hasCap(before, top)) return 21;
        if (!terrain.DigSphere(site.x, y, site.z, site.radius + 2.0f)) return 16;
        if (!mesher.GenerateChunk(terrain, cx, cz, after)) return 17;
        if (terrain.SampleDensity(site.x, y - 1.0f, site.z) >= 0.0f) return 18;
        if (!hasCap(after, bottom) || !hasCap(after, top)) return 22;
        checkedOutcrop = true;
        break;
    }
    if (!checkedOutcrop) return 19;

    bool checkedPermanentPlate = false;
    for (const auto& site : surfaceTest)
    {
        if (site.kind != DepositKind::PermanentNode) continue;
        OutcropCollisionShape plate{};
        if (!BuildOutcropCollisionShape(terrain, site, plate)) continue;
        if (plate.height > plate.radius * 0.30f ||
            plate.height < plate.radius * 0.20f ||
            OutcropTopY(plate) - plate.y > plate.radius * 0.30f ||
            OutcropRingRadiusFactor(plate, 4) < 0.99f ||
            OutcropRingRadiusFactor(plate, 6) > 0.75f ||
            OutcropBottomY(plate) >= plate.y - 0.30f * plate.radius)
        {
            return 23;
        }
        if (!IsInsideOutcrop(plate, plate.x,
            plate.y + plate.height * 0.50f, plate.z))
        {
            return 24;
        }
        if (IsInsideOutcrop(plate, plate.x + plate.radius * 1.5f,
            plate.y + plate.height * 0.50f, plate.z))
        {
            return 25;
        }
        float hitDistance = 0.0f;
        if (!RaycastOutcrop(plate, plate.x - plate.radius * 1.75f,
            plate.y + plate.height * 0.50f, plate.z,
            1.0f, 0.0f, 0.0f, plate.radius * 4.0f, hitDistance))
        {
            return 26;
        }
        const int cx = static_cast<int>(std::floor(site.x / terrain.GetChunkSize()));
        const int cz = static_cast<int>(std::floor(site.z / terrain.GetChunkSize()));
        VoxelMesh plateMesh;
        if (!mesher.GenerateChunk(terrain, cx, cz, plateMesh)) return 27;
        bool hasCenter = false;
        bool hasSeam = false;
        for (const TerrainVertex& vertex : plateMesh.vertices)
        {
            const float dx = vertex.position[0] - plate.x;
            const float dz = vertex.position[2] - plate.z;
            if (dx * dx + dz * dz > plate.radius * plate.radius) continue;
            if (std::abs(vertex.position[0] - plate.x) < 0.001f &&
                std::abs(vertex.position[2] - plate.z) < 0.001f &&
                std::abs(vertex.position[1] - OutcropTopY(plate)) < 0.001f)
            {
                hasCenter = true;
            }
            if (vertex.position[1] > plate.y + plate.height * 0.60f &&
                vertex.color[0] < site.color[0] * 0.40f &&
                vertex.color[1] < site.color[1] * 0.40f &&
                vertex.color[2] < site.color[2] * 0.40f)
            {
                hasSeam = true;
            }
        }
        if (!hasCenter || !hasSeam) return 28;
        checkedPermanentPlate = true;
        break;
    }
    if (!checkedPermanentPlate) return 29;

    std::cout << "Stage 10B: " << originals.size() << " surface deposits, "
        << veins.size() << " nearby underground veins, "
        << output.indices.size() / 3 << " generated triangles\n";
    return 0;
}
