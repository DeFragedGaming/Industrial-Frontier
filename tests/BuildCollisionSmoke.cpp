#include "industrial_frontier/player/BuildCollision.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace industrial_frontier;

static void Check(bool result, const char* description)
{
    if (!result)
    {
        std::cerr << "FAILED: " << description << '\n';
        std::exit(1);
    }
}

static std::vector<StructureCollider> For(BuildKind kind, float yaw = 0.0f)
{
    BuildStructure s{};
    s.id = 1;
    s.kind = kind;
    s.y = 10.0f;
    s.yawDegrees = yaw;
    std::vector<StructureCollider> result;
    AppendBuildColliders(result, s);
    return result;
}

static bool Blocked(const std::vector<StructureCollider>& c,
    float x, float height, float z)
{
    return StructureBlocksPoint(&c, x, height + 10.0f, z, 0.0f);
}

int main()
{
    const auto chest = For(BuildKind::Chest);
    Check(Blocked(chest,0,0.5f,0), "chest body solid");
    Check(!Blocked(chest,0,1.5f,0), "chest top open");
    Check(Blocked(chest,0.68f,0.5f,0), "chest side reaches model");
    const auto rotatedChest = For(BuildKind::Chest,90.0f);
    Check(Blocked(rotatedChest,0,0.5f,0.68f), "chest rotates with model");
    Check(!Blocked(rotatedChest,0.68f,0.5f,0), "rotated chest narrow side");

    const auto foundation = For(BuildKind::Foundation);
    Check(Blocked(foundation,1.4f,0.30f,1.4f), "foundation solid plank");
    Check(!Blocked(foundation,1.4f,0.55f,1.4f), "foundation walkable surface");

    const auto wall = For(BuildKind::Wall);
    Check(Blocked(wall,1.48f,1.5f,0), "wall full three meter width");
    Check(!Blocked(wall,0,1.5f,0.40f), "wall thin in depth");
    const auto turnedWall = For(BuildKind::Wall,90.0f);
    Check(Blocked(turnedWall,0,1.5f,1.48f), "wall rotation");
    Check(!Blocked(turnedWall,1.48f,1.5f,0), "rotated wall orientation");

    const auto door = For(BuildKind::Doorframe);
    Check(!Blocked(door,0,1.25f,0), "doorway accessible at player height");
    Check(Blocked(door,-1.3f,1.25f,0), "doorframe post solid");
    Check(Blocked(door,0,2.77f,0), "doorframe overhead beam solid");
    const auto turnDoor = For(BuildKind::Doorframe,90.0f);
    Check(!Blocked(turnDoor,0,1.25f,0), "rotated doorway accessible");
    Check(Blocked(turnDoor,0,1.25f,-1.3f), "rotated door post solid");

    const auto window = For(BuildKind::WindowWall);
    Check(Blocked(window,0,0.8f,0), "window sill blocks walking through");
    Check(!Blocked(window,0,1.75f,0), "window center opening");
    Check(Blocked(window,1.3f,1.75f,0), "window jamb solid");
    Check(Blocked(window,0,2.65f,0), "window header solid");

    const auto roof = For(BuildKind::Roof);
    Check(!Blocked(roof,0,1.7f,0), "roof permits walking under");
    Check(Blocked(roof,0.5f,3.5f,0), "roof collision overhead");
    const auto support = For(BuildKind::Support);
    Check(Blocked(support,0.18f,1.5f,0), "support column solid");
    Check(!Blocked(support,0.7f,1.5f,0), "support appropriately narrow");
    std::vector<StructureCollider> original{{0.0f,10.0f,0.0f,0.8f,2.0f}};
    Check(Blocked(original,0.5f,0.5f,0), "existing circular furnace collision");
    std::cout << "Oriented chest, foundation, wall, doorway, window, roof and support collision passed\n";
}
