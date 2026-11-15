#include "industrial_frontier/player/PlayerController.hpp"
#include "industrial_frontier/resources/OutcropInteraction.hpp"
#include "industrial_frontier/world/VoxelTerrain.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    void Verify(bool condition, const char* expression, int line)
    {
        if (!condition)
        {
            std::cerr << "Test failed at line " << line << ": "
                << expression << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

#define CHECK(expression) Verify((expression), #expression, __LINE__)

using namespace industrial_frontier;

int main()
{
    TerrainSettings settings{};
    VoxelTerrain terrain(settings, 32.0f, 2.0f);
    const GeologyGenerator* geology = terrain.GetGeology();
    CHECK(geology && geology->MineralCount() >= 90);

    const auto deposits = geology->FindSurfaceDeposits(-600.0f, 600.0f,
        -600.0f, 600.0f);
    OutcropCollisionShape shape{};
    bool found = false;
    for (const GeologicalDeposit& deposit : deposits)
    {
        if (deposit.kind == DepositKind::FiniteOutcrop &&
            BuildOutcropCollisionShape(terrain, deposit, shape))
        {
            found = true;
            break;
        }
    }
    CHECK(found);
    CHECK(!shape.mineralName.empty());
    CHECK(IsInsideOutcrop(shape, shape.x, shape.y + shape.height * 0.3f, shape.z));
    CHECK(!IsInsideOutcrop(shape, shape.x + shape.radius * 2.0f,
        shape.y + shape.height * 0.3f, shape.z));
    const float bottom = OutcropBottomY(shape);
    CHECK(bottom < shape.y - 1.0f);
    CHECK(OutcropTopY(shape) > shape.y);
    CHECK(IsInsideOutcrop(shape, shape.x,
        bottom + (shape.y - bottom) * 0.45f, shape.z));
    CHECK(!IsInsideOutcrop(shape, shape.x, bottom - 0.10f, shape.z));
    CHECK(!IsInsideOutcrop(shape, shape.x, OutcropTopY(shape) + 0.10f,
        shape.z));
    const float buriedY = bottom + (shape.y - bottom) * 0.50f;
    float buriedHit = 0.0f;
    CHECK(RaycastOutcrop(shape,
        shape.x - shape.radius * 2.0f, buriedY, shape.z,
        1.0f, 0.0f, 0.0f, shape.radius * 4.0f, buriedHit));
    CHECK(buriedHit > 0.0f);
    CHECK(OutcropRadiusAtHeight(shape, buriedY) > 0.0f);

    const float rayOriginZ = shape.z - shape.radius * 2.0f;
    float distance = 0.0f;
    CHECK(RaycastOutcrop(shape, shape.x,
        shape.y + shape.height * 0.3f, rayOriginZ,
        0.0f, 0.0f, 1.0f, shape.radius * 4.0f, distance));
    CHECK(distance > 0.0f && distance < shape.radius * 3.0f);

    GeologicalDeposit supported{};
    for (const GeologicalDeposit& deposit : deposits)
    {
        if (deposit.id == shape.id)
        {
            supported = deposit;
            break;
        }
    }
    CHECK(!supported.mineralId.empty());
    CHECK(terrain.DigSphere(shape.x, shape.y - 1.0f, shape.z,
        std::min(2.0f, shape.radius * 0.8f)));
    OutcropCollisionShape afterExcavation{};
    CHECK(BuildOutcropCollisionShape(terrain, supported, afterExcavation));
    CHECK(std::abs(afterExcavation.y - shape.y) < 0.001f);

    const auto originalId = shape.id;
    CHECK(terrain.DepleteDeposit(originalId));
    CHECK(!terrain.DepleteDeposit(originalId));
    CHECK(terrain.IsDepositDepleted(originalId));
    const auto exhausted = terrain.GetDepletedDeposits();
    CHECK(exhausted.size() == 1 && exhausted[0] == originalId);
    OutcropCollisionShape depleted{};
    for (const GeologicalDeposit& deposit : deposits)
    {
        if (deposit.id == originalId)
        {
            CHECK(!BuildOutcropCollisionShape(terrain, deposit, depleted));
        }
    }
    VoxelTerrain reloaded(settings, 32.0f, 2.0f);
    reloaded.SetDepletedDeposits(exhausted);
    CHECK(reloaded.IsDepositDepleted(originalId));

    PlayerController player;
    player.Initialize(terrain.GetGenerator());
    player.SetViewRotation(0.0f, 0.0f);
    OutcropCollisionShape artificial{};
    artificial.x = 5.0f;
    artificial.z = 0.0f;
    artificial.y = terrain.GetSurfaceHeight(0.0f, 0.0f) - 2.0f;
    artificial.radius = 3.0f;
    artificial.height = 6.0f;
    std::vector<OutcropCollisionShape> colliders{artificial};
    player.SetOutcropColliders(&colliders);
    PlayerInput input{};
    input.forward = 1.0f;
    for (int i = 0; i < 100; ++i)
    {
        player.Update(0.016f, input, terrain);
    }
    CHECK(player.GetX() < 5.0f);
    std::cout << "Stage 10B ore targeting, depletion, and collision smoke tests passed.\n";
    return 0;
}
