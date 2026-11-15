
#include "industrial_frontier/renderer/Renderer.hpp"
#include "industrial_frontier/forest/ForestGenerator.hpp"
#include "industrial_frontier/renderer/CampfireVisuals.hpp"
#include "industrial_frontier/resources/Campfire.hpp"
#include "industrial_frontier/resources/CampfireDismantle.hpp"
#include "industrial_frontier/resources/EarlyMetallurgy.hpp"
#include "industrial_frontier/renderer/ClayFurnaceVisuals.hpp"
#include "industrial_frontier/renderer/BuildStructureVisuals.hpp"
#include "industrial_frontier/resources/BuildStructures.hpp"
#include "industrial_frontier/player/BuildCollision.hpp"
#include "industrial_frontier/resources/HotbarEquipment.hpp"

#include "industrial_frontier/player/PlayerController.hpp"
#include "industrial_frontier/resources/OutcropInteraction.hpp"
#include "industrial_frontier/resources/OreAssay.hpp"
#include "industrial_frontier/renderer/Shader.hpp"
#include "industrial_frontier/world/TerrainWorld.hpp"
#include "industrial_frontier/world/TerrainGenerationWorker.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cctype>
#include <limits>
#include <string>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "industrial_frontier/resources/PrimitiveItems.hpp"
#include "industrial_frontier/resources/PrimitiveTier.hpp"
#include <utility>

namespace industrial_frontier
{
    namespace
    {
        constexpr float Pi = 3.14159265358979323846f;
        constexpr float DegreesToRadians = Pi / 180.0f;

        constexpr int LoadRadius = 5;
        constexpr int UnloadRadius = 8;
        constexpr int MaxUploadsPerFrame = 4;

        struct Vec3
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        struct Mat4
        {
            float values[16]{};
        };

        Vec3 Add(const Vec3& a, const Vec3& b)
        {
            return {
                a.x + b.x,
                a.y + b.y,
                a.z + b.z
            };
        }

        Vec3 Subtract(const Vec3& a, const Vec3& b)
        {
            return {
                a.x - b.x,
                a.y - b.y,
                a.z - b.z
            };
        }

        Vec3 Multiply(const Vec3& value, float scalar)
        {
            return {
                value.x * scalar,
                value.y * scalar,
                value.z * scalar
            };
        }

        float Dot(const Vec3& a, const Vec3& b)
        {
            return
                a.x * b.x +
                a.y * b.y +
                a.z * b.z;
        }

        Vec3 Cross(const Vec3& a, const Vec3& b)
        {
            return {
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x
            };
        }

        Vec3 Normalize(const Vec3& value)
        {
            const float length = std::sqrt(Dot(value, value));

            if (length <= 0.00001f)
            {
                return {};
            }

            return Multiply(value, 1.0f / length);
        }

        float SmoothStep(float value)
        {
            const float t = std::clamp(value, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        float Lerp(float a, float b, float t)
        {
            return a + (b - a) * t;
        }

        Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
        {
            return {
                Lerp(a.x, b.x, t),
                Lerp(a.y, b.y, t),
                Lerp(a.z, b.z, t)
            };
        }

        Mat4 Identity()
        {
            Mat4 result{};

            result.values[0] = 1.0f;
            result.values[5] = 1.0f;
            result.values[10] = 1.0f;
            result.values[15] = 1.0f;

            return result;
        }

        Mat4 MultiplyMatrices(const Mat4& a, const Mat4& b)
        {
            Mat4 result{};

            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    for (int index = 0; index < 4; ++index)
                    {
                        result.values[column * 4 + row] +=
                            a.values[index * 4 + row] *
                            b.values[column * 4 + index];
                    }
                }
            }

            return result;
        }

        Mat4 Perspective(
            float fieldOfView,
            float aspectRatio,
            float nearPlane,
            float farPlane)
        {
            Mat4 result{};

            const float tangent =
                std::tan(fieldOfView * 0.5f);

            if (aspectRatio <= 0.0f || tangent <= 0.0f)
            {
                return Identity();
            }

            result.values[0] =
                1.0f / (aspectRatio * tangent);

            result.values[5] =
                1.0f / tangent;

            result.values[10] =
                -(farPlane + nearPlane) /
                (farPlane - nearPlane);

            result.values[11] = -1.0f;

            result.values[14] =
                -(2.0f * farPlane * nearPlane) /
                (farPlane - nearPlane);

            return result;
        }

        Mat4 LookAt(
            const Vec3& position,
            const Vec3& target,
            const Vec3& worldUp)
        {
            const Vec3 forward = Normalize(
                Subtract(target, position)
            );

            const Vec3 right = Normalize(
                Cross(forward, worldUp)
            );

            const Vec3 up = Cross(right, forward);

            Mat4 result = Identity();

            result.values[0] = right.x;
            result.values[1] = up.x;
            result.values[2] = -forward.x;

            result.values[4] = right.y;
            result.values[5] = up.y;
            result.values[6] = -forward.y;

            result.values[8] = right.z;
            result.values[9] = up.z;
            result.values[10] = -forward.z;

            result.values[12] = -Dot(right, position);
            result.values[13] = -Dot(up, position);
            result.values[14] = Dot(forward, position);

            return result;
        }

        using GenVertexArraysProc =
            void(APIENTRY*)(GLsizei, GLuint*);

        using BindVertexArrayProc =
            void(APIENTRY*)(GLuint);

        using DeleteVertexArraysProc =
            void(APIENTRY*)(GLsizei, const GLuint*);

        using GenBuffersProc =
            void(APIENTRY*)(GLsizei, GLuint*);

        using BindBufferProc =
            void(APIENTRY*)(GLenum, GLuint);

        using BufferDataProc =
            void(APIENTRY*)(
                GLenum,
                GLsizeiptr,
                const void*,
                GLenum
            );

        using DeleteBuffersProc =
            void(APIENTRY*)(GLsizei, const GLuint*);

        using EnableVertexAttribArrayProc =
            void(APIENTRY*)(GLuint);

        using VertexAttribPointerProc =
            void(APIENTRY*)(
                GLuint,
                GLint,
                GLenum,
                GLboolean,
                GLsizei,
                const void*
            );

        using DrawElementsProc =
            void(APIENTRY*)(
                GLenum,
                GLsizei,
                GLenum,
                const void*
            );

        template<typename T>
        bool LoadGLFunction(T& function, const char* name)
        {
            function = reinterpret_cast<T>(
                SDL_GL_GetProcAddress(name)
            );

            if (function == nullptr)
            {
                std::cerr
                    << "Failed to load OpenGL function: "
                    << name
                    << '\n';

                return false;
            }

            return true;
        }
    }

    namespace
    {
        InventoryOreBatch CreateMiningBatch(const GeologicalDeposit& deposit,
            const GeologyGenerator& geology, float massKg)
        {
            InventoryOreBatch batch;
            batch.depositId = deposit.id;
            batch.primaryMineralId = deposit.mineralId;
            const MineralDefinition* primary = geology.FindMineral(deposit.mineralId);
            batch.displayName = primary ? primary->name : deposit.mineralId;
            batch.massKg = massKg;
            const OreAssay assay = AnalyzeOre(deposit, geology);
            if (!assay.valid)
            {
                return {};
            }
            batch.oreMineralFraction = assay.oreMineralFraction;
            for (const auto& mineral : assay.minerals)
            {
                batch.components.push_back({mineral.mineralId,
                    mineral.massFraction, mineral.gangue});
            }
            return batch;
        }
    }

    struct Renderer::Impl
    {
        struct GPUChunk
        {
            GLuint vertexArray = 0;
            GLuint vertexBuffer = 0;
            GLuint indexBuffer = 0;
            GLsizei indexCount = 0;
        };

        GenVertexArraysProc GenVertexArrays = nullptr;
        BindVertexArrayProc BindVertexArray = nullptr;
        DeleteVertexArraysProc DeleteVertexArrays = nullptr;

        GenBuffersProc GenBuffers = nullptr;
        BindBufferProc BindBuffer = nullptr;
        BufferDataProc BufferData = nullptr;
        DeleteBuffersProc DeleteBuffers = nullptr;

        EnableVertexAttribArrayProc EnableVertexAttribArray = nullptr;
        VertexAttribPointerProc VertexAttribPointer = nullptr;
        DrawElementsProc DrawElements = nullptr;

        Shader terrainShader;
        Shader campfireParticleShader;
        CampfireWorld campfires;
        ClayFurnaceWorld furnaces;
        BuildWorld constructions;
        GPUChunk constructionProps{};
        bool constructionsDirty = true;
        BuildKind selectedBuildKind = BuildKind::Chest;
        const MineralDatabase* mineralDatabase = nullptr;
        float moldCoolingSeconds = 0.0f;
        std::size_t selectedFurnaceOre = 0;
        GPUChunk furnaceProps{};
        GPUChunk furnaceFlames{};
        GPUChunk furnaceSmoke{};
        GPUChunk heldMold{};
        bool furnacePropsDirty = true;
        bool furnaceParticlesDirty = true;
        GPUChunk campfireProps{};
        GPUChunk campfireFlames{};
        GPUChunk campfireSmoke{};
        bool campfirePropsDirty = true;
        bool campfireParticlesDirty = true;
        enum class PlacementKind { None, Campfire, Furnace, Construction };
        struct PlacementPose
        {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
            bool valid = false;
        };
        PlacementKind placement = PlacementKind::None;
        float placementYaw = 0.0f;
        PlacementPose lastPreviewPose{};
        float lastPreviewYaw = -1.0f;
        bool lastPreviewValid = false;
        GPUChunk previewMesh{};
        std::vector<StructureCollider> nearbyStructures;
        float fireVisualTime = 0.0f;
        float fireVisualRefresh = 0.0f;
        TerrainWorld terrainWorld;
        TerrainGenerationWorker terrainWorker;
        PlayerController player;
        PlayerInventory inventory;
        std::string equippedItem;
        std::string activeMiningTool;
        std::unique_ptr<VoxelTerrain> visibleCollisionSnapshot;

        std::unordered_map<std::uint64_t, GPUChunk> gpuChunks;
        std::unordered_map<std::uint64_t, GPUChunk> forestChunks;
        std::unordered_map<std::uint64_t, std::vector<ForestTree>> treesByChunk;
        std::unordered_map<std::uint64_t, std::vector<ForestPebble>> pebblesByChunk;
        std::unordered_set<std::uint64_t> felledTreeIds;
        std::unordered_set<std::uint64_t> strippedTreeIds;
        std::unordered_set<std::uint64_t> gatheredPebbleIds;
        std::vector<ForestTree> nearbyTrees;
        std::uint64_t treeHarvestId = 0;
        int treeHarvestHits = 0;
        float treeHarvestElapsed = 0.0f;
        std::string treeHarvestName;
        std::unordered_set<std::uint64_t> emptyChunks;
        std::unordered_set<std::uint64_t> pendingEditKeys;
        std::unordered_map<std::uint64_t, GPUChunk> pendingEditReady;

        Vec3 cameraPosition{};
        Vec3 cameraForward{0.0f, 0.0f, -1.0f};
        Vec3 cameraTarget{};

        Vec3 playerCameraPosition{};
        Vec3 playerCameraForward{0.0f, 0.0f, -1.0f};
        Vec3 lastSafePlayerCameraPosition{};
        Vec3 lastSafePlayerFeetPosition{};
        Vec3 checkedFeetPosition{};
        Vec3 checkedCameraPosition{};
        std::uint64_t checkedTerrainRevision = 0;
        bool hasSafePlayerCameraPosition = false;
        bool hasSafePlayerFeetPosition = false;
        bool hasCheckedCameraPosition = false;

        Vec3 cinematicPosition{};
        Vec3 cinematicTarget{};

        float cinematicTime = 0.0f;

        int currentChunkX = 0;
        int currentChunkZ = 0;
        int prefetchDirectionX = 0;
        int prefetchDirectionZ = 0;
        Vec3 previousStreamingPosition{};
        bool hasPreviousStreamingPosition = false;

        bool cinematicMode = false;
        bool initialized = false;
        std::uint64_t targetLastUpdate = 0;
        bool cachedTargetHit = false;
        TerrainMaterial cachedTargetMaterial = TerrainMaterial::None;
        float cachedTargetDistance = 0.0f;
        std::string cachedOreName;
        std::string cachedOreAssay;
        bool cachedOutcropTargetHit = false;
        float cachedOutcropDistance = 0.0f;
        std::string cachedVeinName;
        std::string cachedVeinAssay;
        std::string cachedVeinMineralId;
        Vec3 cachedVeinPoint{};
        std::uint64_t cachedVeinId = 0;
        std::uint64_t cachedOreId = 0;
        std::vector<OutcropCollisionShape> nearbyOutcrops;
        float lastOutcropX = std::numeric_limits<float>::max();
        float lastOutcropZ = std::numeric_limits<float>::max();
        std::uint64_t lastOutcropRevision = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t miningId = 0;
        bool miningIsVein = false;
        Vec3 miningPoint{};
        std::string miningName;
        GeologicalDeposit miningDeposit{};
        bool miningHasDeposit = false;
        float miningElapsed = 0.0f;
        int miningHits = 0;
        int miningHitsRequired = 0;
        float completedFeedbackTimer = 0.0f;
        std::string completedFeedback;

        void RefreshNearbyTrees()
        {
            nearbyTrees.clear();
            const float x = player.GetX();
            const float z = player.GetZ();
            const float radiusSq = 27.0f * 27.0f;
            for (const auto& entry : treesByChunk)
            {
                const int cx = static_cast<std::int32_t>(entry.first >> 32);
                const int cz = static_cast<std::int32_t>(entry.first & 0xffffffffu);
                if (std::abs(cx - currentChunkX) > 2 ||
                    std::abs(cz - currentChunkZ) > 2)
                    continue;
                for (const auto& tree : entry.second)
                {
                    const float dx = tree.x - x;
                    const float dz = tree.z - z;
                    if (dx * dx + dz * dz < radiusSq)
                        nearbyTrees.push_back(tree);
                }
            }
            player.SetTreeColliders(&nearbyTrees);
        }

        const ForestTree* FindTree(std::uint64_t id) const
        {
            for (const auto& entry : treesByChunk)
            {
                for (const auto& tree : entry.second)
                {
                    if (tree.id == id)
                        return &tree;
                }
            }
            return nullptr;
        }

        const ForestPebble* FindPebble(std::uint64_t id) const
        {
            for (const auto& entry : pebblesByChunk)
            {
                for (const auto& stone : entry.second)
                {
                    if (stone.id == id)
                        return &stone;
                }
            }
            return nullptr;
        }

        const CampfireState* TraceCampfire(float reach) const
        {
            const Vec3 ray = Normalize(playerCameraForward);
            const Vec3 origin = playerCameraPosition;
            const CampfireState* selected = nullptr;
            float nearest = reach;
            for (const auto& fire : campfires.Fires())
            {
                const Vec3 center{fire.x, fire.y + 0.40f, fire.z};
                const Vec3 offset = Subtract(center, origin);
                const float t = Dot(offset, ray);
                if (t <= 0.1f || t > nearest)
                    continue;
                const Vec3 closest = Add(origin, Multiply(ray, t));
                const Vec3 difference = Subtract(center, closest);
                if (Dot(difference,difference) < 0.92f * 0.92f)
                {
                    nearest = t;
                    selected = &fire;
                }
            }
            return selected;
        }

        const BuildStructure* TraceConstruction(float reach, bool chestsOnly = false) const
        {
            const Vec3 ray = Normalize(playerCameraForward);
            const Vec3 origin = playerCameraPosition;
            const BuildStructure* found = nullptr;
            float nearest = std::min(reach, TraceVoxelDistance(reach) + 0.35f);
            for (const auto& structure : constructions.Structures())
            {
                if (chestsOnly && structure.kind != BuildKind::Chest)
                    continue;
                const float height = structure.kind == BuildKind::Chest ? 0.53f :
                    structure.kind == BuildKind::Roof ? 3.55f :
                    structure.kind == BuildKind::Foundation ? 0.32f : 1.6f;
                const Vec3 center{structure.x, structure.y + height, structure.z};
                const Vec3 delta = Subtract(center,origin);
                const float along = Dot(delta,ray);
                if (along <= 0.05f || along > nearest)
                    continue;
                const Vec3 projected = Add(origin,Multiply(ray,along));
                const Vec3 offset = Subtract(center,projected);
                const float radius = structure.kind == BuildKind::Chest ? 0.93f : 2.15f;
                if (Dot(offset,offset) < radius*radius)
                {
                    nearest = along;
                    found = &structure;
                }
            }
            return found;
        }

        const ClayFurnaceState* TraceFurnace(float reach) const
        {
            const Vec3 ray = Normalize(playerCameraForward);
            const Vec3 origin = playerCameraPosition;
            const ClayFurnaceState* chosen = nullptr;
            float nearest = std::min(reach, TraceVoxelDistance(reach) + 0.35f);
            for (const auto& furnace : furnaces.Furnaces())
            {
                const Vec3 center{furnace.x,furnace.y+1.0f,furnace.z};
                const Vec3 delta = Subtract(center,origin);
                const float t=Dot(delta,ray);
                if (t <= 0.1f || t > nearest)
                    continue;
                const Vec3 projected=Add(origin,Multiply(ray,t));
                const Vec3 separation=Subtract(center,projected);
                if (Dot(separation,separation) <= 0.9f*0.9f)
                {
                    chosen=&furnace;
                    nearest=t;
                }
            }
            return chosen;
        }

        void RefreshStructureColliders()
        {
            nearbyStructures.clear();
            const float px = player.GetX();
            const float pz = player.GetZ();
            for (const auto& fire : campfires.Fires())
            {
                const float dx = fire.x - px;
                const float dz = fire.z - pz;
                if (dx * dx + dz * dz < 32.0f * 32.0f)
                    nearbyStructures.push_back({fire.x, fire.y, fire.z,
                        fire.covered ? 0.76f : 0.73f,
                        fire.covered ? 1.15f : 0.70f});
            }
            for (const auto& kiln : furnaces.Furnaces())
            {
                const float dx = kiln.x - px;
                const float dz = kiln.z - pz;
                if (dx * dx + dz * dz < 32.0f * 32.0f)
                    nearbyStructures.push_back({kiln.x, kiln.y, kiln.z,
                        0.89f, 2.08f});
            }
            for (const auto& s : constructions.Structures())
            {
                const float dx = s.x - px;
                const float dz = s.z - pz;
                if (dx * dx + dz * dz > 32.0f * 32.0f)
                    continue;
                AppendBuildColliders(nearbyStructures, s);
            }
            player.SetStructureColliders(&nearbyStructures);
        }

        PlacementPose ComputePlacementPose(PlacementKind kind) const
        {
            PlacementPose pose{};
            const auto terrain = terrainWorker.GetVoxelTerrain();
            if (!terrain || kind == PlacementKind::None)
                return pose;
            const float forwardLength = std::sqrt(
                playerCameraForward.x * playerCameraForward.x +
                playerCameraForward.z * playerCameraForward.z);
            if (forwardLength < 0.01f)
                return pose;
            const bool kiln = kind == PlacementKind::Furnace;
            const bool building = kind == PlacementKind::Construction;
            const float forwardReach = kiln ? 3.2f : building ? 3.6f : 2.35f;
            pose.x = player.GetX() + forwardReach *
                playerCameraForward.x / forwardLength;
            pose.z = player.GetZ() + forwardReach *
                playerCameraForward.z / forwardLength;
            pose.y = terrain->GetSurfaceHeight(pose.x, pose.z) +
                (kiln ? 0.04f : 0.035f);
            if (building)
            {
                const auto* foundation = constructions.NearestFoundation(pose.x,pose.z,4.0f);
                if (selectedBuildKind == BuildKind::Foundation && foundation)
                {
                    pose.x = foundation->x +
                        std::round((pose.x - foundation->x)/3.0f)*3.0f;
                    pose.z = foundation->z +
                        std::round((pose.z - foundation->z)/3.0f)*3.0f;
                    pose.y = terrain->GetSurfaceHeight(pose.x,pose.z)+0.04f;
                }
                else if (selectedBuildKind != BuildKind::Chest &&
                    selectedBuildKind != BuildKind::Foundation)
                {
                    if (!foundation)
                        return pose;
                    pose.x = foundation->x;
                    pose.z = foundation->z;
                    pose.y = foundation->y;
                    if (selectedBuildKind == BuildKind::Wall ||
                        selectedBuildKind == BuildKind::Doorframe ||
                        selectedBuildKind == BuildKind::WindowWall)
                    {
                        const float radians = placementYaw * Pi / 180.0f;
                        pose.x += 1.50f * std::sin(radians);
                        pose.z += 1.50f * std::cos(radians);
                    }
                    else if (selectedBuildKind == BuildKind::Support)
                    {
                        const float radians = placementYaw * Pi / 180.0f;
                        pose.x += 1.42f * (std::cos(radians) + std::sin(radians));
                        pose.z += 1.42f * (std::cos(radians) - std::sin(radians));
                    }
                }
            }
            if (!std::isfinite(pose.x) || !std::isfinite(pose.y) ||
                !std::isfinite(pose.z) ||
                std::abs(pose.y - player.GetY()) > (building ? 2.4f : kiln ? 1.2f : 1.3f) ||
                terrain->SampleDensity(pose.x, pose.y + 0.65f, pose.z) > 0.0f ||
                terrain->SampleDensity(pose.x, pose.y +
                    (kiln ? 2.2f : 1.3f), pose.z) > 0.0f)
                return pose;
            const float footprint = building ? 1.35f : kiln ? 0.75f : 0.70f;
            if (!building || selectedBuildKind == BuildKind::Foundation ||
                selectedBuildKind == BuildKind::Chest)
            for (const auto [dx,dz] : {
                std::pair{-footprint,0.0f}, std::pair{footprint,0.0f},
                std::pair{0.0f,-footprint}, std::pair{0.0f,footprint}})
            {
                if (std::abs(terrain->GetSurfaceHeight(
                    pose.x + dx,pose.z + dz) - pose.y) > 0.40f)
                    return pose;
            }
            const float playerDx = pose.x - player.GetX();
            const float playerDz = pose.z - player.GetZ();
            if (playerDx * playerDx + playerDz * playerDz <
                (building && selectedBuildKind != BuildKind::Chest ? 0.25f : 2.0f))
                return pose;
            for (const auto& tree : nearbyTrees)
            {
                const float dx = tree.x - pose.x;
                const float dz = tree.z - pose.z;
                if (dx * dx + dz * dz < (kiln ? 9.0f : 4.0f))
                    return pose;
            }
            for (const auto& fire : campfires.Fires())
            {
                const float dx = fire.x - pose.x;
                const float dz = fire.z - pose.z;
                if (dx * dx + dz * dz < (kiln ? 6.25f : 3.24f))
                    return pose;
            }
            for (const auto& f : furnaces.Furnaces())
            {
                const float dx = f.x - pose.x;
                const float dz = f.z - pose.z;
                if (dx * dx + dz * dz < (kiln ? 6.25f : 5.0f))
                    return pose;
            }
            if (building)
            {
                if (!constructions.CanPlace(selectedBuildKind,pose.x,pose.y,
                    pose.z,placementYaw))
                    return pose;
                if (selectedBuildKind == BuildKind::Foundation ||
                    selectedBuildKind == BuildKind::Chest)
                {
                    for (const auto& other : constructions.Structures())
                    {
                        const float dx = other.x-pose.x;
                        const float dz = other.z-pose.z;
                        if (other.kind == BuildKind::Chest && dx*dx+dz*dz<3.2f)
                            return pose;
                    }
                }
            }
            else if ((kiln && furnaces.Furnaces().size() >= ClayFurnaceWorld::MaxFurnaces) ||
                (!kiln && campfires.Fires().size() >= CampfireWorld::MaxCampfires))
                return pose;
            pose.valid = true;
            return pose;
        }

        void UpdatePreviewGeometry()
        {
            if (placement == PlacementKind::None)
            {
                if (previewMesh.indexCount > 0)
                {
                    VoxelMesh empty;
                    UploadCampfireMesh(empty, previewMesh);
                }
                return;
            }
            const PlacementPose pose = ComputePlacementPose(placement);
            if (std::abs(pose.x-lastPreviewPose.x) < 0.04f &&
                std::abs(pose.y-lastPreviewPose.y) < 0.04f &&
                std::abs(pose.z-lastPreviewPose.z) < 0.04f &&
                std::abs(placementYaw-lastPreviewYaw) < 0.01f &&
                pose.valid == lastPreviewValid && previewMesh.indexCount > 0)
                return;
            VoxelMesh mesh;
            if (placement == PlacementKind::Construction)
            {
                BuildStructure s{};
                s.kind = selectedBuildKind;
                s.x = pose.x; s.y = pose.y; s.z = pose.z;
                s.yawDegrees = placementYaw;
                BuildStructureVisuals::Build({s},mesh);
            }
            else if (placement == PlacementKind::Furnace)
            {
                ClayFurnaceState f{};
                f.x = pose.x; f.y = pose.y; f.z = pose.z;
                f.yawDegrees = placementYaw;
                ClayFurnaceVisuals::BuildBodies({f},mesh);
            }
            else
            {
                CampfireState fire{};
                fire.x = pose.x; fire.y = pose.y; fire.z = pose.z;
                fire.yawDegrees = placementYaw;
                CampfireVisuals::BuildPit({fire},mesh);
            }
            for (auto& vertex : mesh.vertices)
            {
                vertex.color[0] = pose.valid ? 0.09f : 0.95f;
                vertex.color[1] = pose.valid ? 0.90f : 0.12f;
                vertex.color[2] = pose.valid ? 0.83f : 0.12f;
            }
            if (UploadCampfireMesh(mesh,previewMesh))
            {
                lastPreviewPose = pose;
                lastPreviewYaw = placementYaw;
                lastPreviewValid = pose.valid;
            }
        }

        bool UploadCampfireMesh(const VoxelMesh& geometry, GPUChunk& target)
        {
            GPUChunk updated{};
            if (!geometry.IsEmpty() &&
                !BuildGPUMesh(geometry.vertices, geometry.indices, updated))
                return false;
            DeleteChunk(target);
            target = updated;
            return true;
        }

        void UpdateCampfireMeshes()
        {
            if (campfirePropsDirty)
            {
                VoxelMesh mesh;
                CampfireVisuals::BuildPit(campfires.Fires(),mesh);
                if (UploadCampfireMesh(mesh,campfireProps))
                    campfirePropsDirty = false;
            }
            if (campfireParticlesDirty)
            {
                VoxelMesh flames;
                VoxelMesh smoke;
                CampfireVisuals::BuildParticles(campfires.Fires(),
                    fireVisualTime,flames,smoke);
                if (UploadCampfireMesh(flames,campfireFlames) &&
                    UploadCampfireMesh(smoke,campfireSmoke))
                    campfireParticlesDirty = false;
            }
        }

        void UpdateConstructionMeshes()
        {
            if (!constructionsDirty)
                return;
            VoxelMesh mesh;
            BuildStructureVisuals::Build(constructions.Structures(),mesh);
            if (UploadCampfireMesh(mesh,constructionProps))
                constructionsDirty = false;
        }

        void UpdateFurnaceMeshes()
        {
            if (furnacePropsDirty)
            {
                VoxelMesh mesh;
                ClayFurnaceVisuals::BuildBodies(furnaces.Furnaces(),mesh);
                if (UploadCampfireMesh(mesh,furnaceProps))
                    furnacePropsDirty=false;
            }
            if (furnaceParticlesDirty)
            {
                VoxelMesh flame;
                VoxelMesh smoke;
                ClayFurnaceVisuals::BuildEffects(furnaces.Furnaces(),
                    fireVisualTime,flame,smoke);
                if (UploadCampfireMesh(flame,furnaceFlames) &&
                    UploadCampfireMesh(smoke,furnaceSmoke))
                    furnaceParticlesDirty=false;
            }
        }

        static std::string TreeName(TreeSpecies type)
        {
            switch (type)
            {
                case TreeSpecies::Pine: return "PINE TREE";
                case TreeSpecies::Broadleaf: return "BROADLEAF TREE";
                case TreeSpecies::Birch: return "BIRCH TREE";
            }
            return "TREE";
        }

        bool TraceTree(float reach, ForestTree& selected, float& distance) const
        {
            bool found = false;
            distance = std::min(reach, TraceVoxelDistance(reach));
            const Vec3 ray = Normalize(playerCameraForward);
            const Vec3 origin = playerCameraPosition;
            for (const auto& entry : treesByChunk)
            {
                for (const auto& tree : entry.second)
                {
                    const float dx = origin.x - tree.x;
                    const float dz = origin.z - tree.z;
                    if (dx * dx + dz * dz > (reach + tree.crownRadius) *
                        (reach + tree.crownRadius))
                        continue;
                    const float trunkRadius = tree.crownRadius *
                        (tree.species == TreeSpecies::Pine ? 0.18f : 0.23f) * 1.25f;
                    const float a = ray.x * ray.x + ray.z * ray.z;
                    if (a > 0.00001f)
                    {
                        const float b = 2.0f * (dx * ray.x + dz * ray.z);
                        const float c = dx * dx + dz * dz - trunkRadius * trunkRadius;
                        const float discriminant = b * b - 4.0f * a * c;
                        if (discriminant >= 0.0f)
                        {
                            const float root = std::sqrt(discriminant);
                            for (float t : {(-b - root) / (2.0f * a),
                                (-b + root) / (2.0f * a)})
                            {
                                const float y = origin.y + ray.y * t;
                                if (t > 0.05f && t < distance &&
                                    y >= tree.y - 0.25f &&
                                    y < tree.y + tree.height * 0.8f)
                                {
                                    distance = t;
                                    selected = tree;
                                    found = true;
                                }
                            }
                        }
                    }
                    const Vec3 canopy{tree.x,
                        tree.y + tree.height * 0.77f, tree.z};
                    const Vec3 offset = Subtract(origin, canopy);
                    const float b = Dot(offset, ray);
                    const float c = Dot(offset, offset) -
                        tree.crownRadius * tree.crownRadius * 0.66f;
                    const float disc = b * b - c;
                    if (disc >= 0.0f)
                    {
                        const float t = -b - std::sqrt(disc);
                        if (t > 0.05f && t < distance)
                        {
                            distance = t;
                            selected = tree;
                            found = true;
                        }
                    }
                }
            }
            return found;
        }

        bool TracePebble(float reach, ForestPebble& selected, float& distance) const
        {
            bool found = false;
            distance = std::min(reach, TraceVoxelDistance(reach));
            const Vec3 ray = Normalize(playerCameraForward);
            for (const auto& entry : pebblesByChunk)
            {
                for (const auto& stone : entry.second)
                {
                    const Vec3 offset = Subtract(playerCameraPosition,
                        {stone.x, stone.y + 0.19f, stone.z});
                    const float b = Dot(offset, ray);
                    const float c = Dot(offset, offset) -
                        (stone.radius + 0.12f) * (stone.radius + 0.12f);
                    const float discriminant = b * b - c;
                    if (discriminant < 0.0f)
                        continue;
                    const float t = -b - std::sqrt(discriminant);
                    if (t > 0.05f && t < distance)
                    {
                        distance = t;
                        selected = stone;
                        found = true;
                    }
                }
            }
            return found;
        }

        bool OreBlocks(float distance)
        {
            float oreDistance = 0.0f;
            return TraceOutcrop(distance, oreDistance) != nullptr &&
                oreDistance + 0.04f < distance;
        }

        void Notify(const std::string& value)
        {
            completedFeedback = value;
            completedFeedbackTimer = 3.0f;
        }

        void RefreshNearbyOutcrops()
        {
            const auto terrain = terrainWorker.GetVoxelTerrain();
            if (!terrain || !terrain->GetGeology())
            {
                nearbyOutcrops.clear();
                player.SetOutcropColliders(&nearbyOutcrops);
                return;
            }
            const float x = player.GetX();
            const float z = player.GetZ();
            const float dx = x - lastOutcropX;
            const float dz = z - lastOutcropZ;
            const std::uint64_t revision = terrain->GetRevision();
            if (revision == lastOutcropRevision && dx * dx + dz * dz < 25.0f)
            {
                return;
            }
            lastOutcropX = x;
            lastOutcropZ = z;
            lastOutcropRevision = revision;
            nearbyOutcrops.clear();
            const auto deposits = terrain->GetGeology()->FindSurfaceDeposits(
                x - 24.0f, x + 24.0f, z - 24.0f, z + 24.0f);
            for (const GeologicalDeposit& deposit : deposits)
            {
                OutcropCollisionShape shape{};
                if (BuildOutcropCollisionShape(*terrain, deposit, shape))
                {
                    nearbyOutcrops.push_back(std::move(shape));
                }
            }
            player.SetOutcropColliders(&nearbyOutcrops);
        }

        float TraceVoxelDistance(float maximumDistance) const
        {
            const auto terrain = terrainWorker.GetVoxelTerrain();
            if (!terrain)
            {
                return maximumDistance;
            }
            const Vec3 origin = playerCameraPosition;
            const Vec3 direction = Normalize(playerCameraForward);
            float previous = terrain->SampleDensity(origin.x, origin.y, origin.z);
            for (float distance = 0.12f; distance <= maximumDistance;
                 distance += 0.12f)
            {
                const Vec3 point = Add(origin, Multiply(direction, distance));
                const float density = terrain->SampleDensity(
                    point.x, point.y, point.z);
                if (previous <= 0.0f && density > 0.0f)
                {
                    return distance;
                }
                previous = density;
            }
            return maximumDistance;
        }

        const OutcropCollisionShape* TraceOutcrop(
            float maximumDistance, float& hitDistance)
        {
            RefreshNearbyOutcrops();
            const Vec3 origin = playerCameraPosition;
            const Vec3 direction = Normalize(playerCameraForward);
            const OutcropCollisionShape* result = nullptr;
            hitDistance = std::min(maximumDistance, TraceVoxelDistance(maximumDistance));
            for (const OutcropCollisionShape& shape : nearbyOutcrops)
            {
                float candidate = 0.0f;
                if (RaycastOutcrop(shape, origin.x, origin.y, origin.z,
                    direction.x, direction.y, direction.z,
                    hitDistance, candidate) && candidate < hitDistance)
                {
                    hitDistance = candidate;
                    result = &shape;
                }
            }
            return result;
        }

        void ResetMining()
        {
            miningId = 0;
            miningName.clear();
            miningDeposit = {};
            miningHasDeposit = false;
            miningElapsed = 0.0f;
            miningHits = 0;
            miningHitsRequired = 0;
            completedFeedbackTimer = 0.0f;
            completedFeedback.clear();
            cachedOreId = 0;
            cachedOreName.clear();
            cachedOreAssay.clear();
            cachedOutcropTargetHit = false;
            cachedOutcropDistance = 0.0f;
            cachedVeinName.clear();
            cachedVeinAssay.clear();
            cachedVeinMineralId.clear();
            cachedVeinId = 0;
            miningIsVein = false;
            nearbyOutcrops.clear();
            lastOutcropRevision = std::numeric_limits<std::uint64_t>::max();
            player.SetOutcropColliders(&nearbyOutcrops);
        }

        bool TraceExposedVein(float reach,
            Vec3& hitPoint, GeologicalDeposit& selected)
        {
            const auto terrain = terrainWorker.GetVoxelTerrain();
            if (!terrain || !terrain->GetGeology())
            {
                return false;
            }
            const float distance = TraceVoxelDistance(reach);
            if (distance >= reach)
            {
                return false;
            }
            const Vec3 point = Add(playerCameraPosition,
                Multiply(Normalize(playerCameraForward), distance));
            const float depth = terrain->GetSurfaceHeight(point.x, point.z) - point.y;
            if (depth <= 2.5f || point.y <= VoxelTerrain::BedrockSurfaceY + 2.0f)
            {
                return false;
            }
            const auto veins = terrain->GetGeology()->FindUndergroundVeins(
                point.x - 0.25f, point.x + 0.25f,
                point.y - 0.25f, point.y + 0.25f,
                point.z - 0.25f, point.z + 0.25f);
            float strongest = 0.04f;
            bool found = false;
            for (const GeologicalDeposit& vein : veins)
            {
                const float strength = GeologyGenerator::VeinInfluence(
                    vein, point.x, point.y, point.z);
                if (strength > strongest)
                {
                    strongest = strength;
                    selected = vein;
                    found = true;
                }
            }
            if (found)
            {
                hitPoint = point;
            }
            return found;
        }

        bool ExcavateAtPoint(const Vec3& point, float radius)
        {
            if (!visibleCollisionSnapshot)
            {
                const auto currentTerrain = terrainWorker.GetVoxelTerrain();
                if (!currentTerrain)
                {
                    return false;
                }
                visibleCollisionSnapshot = std::make_unique<VoxelTerrain>(
                    terrainWorld.GetSettings(), terrainWorld.GetChunkSize(),
                    terrainWorld.GetVertexSpacing());
                visibleCollisionSnapshot->SetEdits(currentTerrain->GetEdits());
                visibleCollisionSnapshot->SetDepletedDeposits(
                    currentTerrain->GetDepletedDeposits());
                hasCheckedCameraPosition = false;
            }
            if (!terrainWorker.DigSphere(point.x, point.y, point.z, radius))
            {
                if (pendingEditKeys.empty())
                {
                    visibleCollisionSnapshot.reset();
                    hasCheckedCameraPosition = false;
                }
                return false;
            }
            const float chunkSize = terrainWorld.GetChunkSize();
            const float extent = radius + 2.0f;
            const int minX = static_cast<int>(std::floor((point.x - extent) / chunkSize));
            const int maxX = static_cast<int>(std::floor((point.x + extent) / chunkSize));
            const int minZ = static_cast<int>(std::floor((point.z - extent) / chunkSize));
            const int maxZ = static_cast<int>(std::floor((point.z + extent) / chunkSize));
            for (int z = minZ; z <= maxZ; ++z)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    if (std::abs(x - currentChunkX) > UnloadRadius ||
                        std::abs(z - currentChunkZ) > UnloadRadius)
                    {
                        continue;
                    }
                    const std::uint64_t key = MakeChunkKey(x, z);
                    pendingEditKeys.insert(key);
                    const auto prepared = pendingEditReady.find(key);
                    if (prepared != pendingEditReady.end())
                    {
                        DeleteChunk(prepared->second);
                        pendingEditReady.erase(prepared);
                    }
                    terrainWorker.RegenerateChunk(x, z);
                }
            }
            return true;
        }

        void RegenerateOutcropArea(const OutcropCollisionShape& shape)
        {
            const float chunkSize = terrainWorld.GetChunkSize();
            const float extent = shape.radius + 3.0f;
            const int minX = static_cast<int>(std::floor((shape.x - extent) / chunkSize));
            const int maxX = static_cast<int>(std::floor((shape.x + extent) / chunkSize));
            const int minZ = static_cast<int>(std::floor((shape.z - extent) / chunkSize));
            const int maxZ = static_cast<int>(std::floor((shape.z + extent) / chunkSize));
            for (int z = minZ; z <= maxZ; ++z)
            {
                for (int x = minX; x <= maxX; ++x)
                {
                    if (std::abs(x - currentChunkX) > UnloadRadius ||
                        std::abs(z - currentChunkZ) > UnloadRadius)
                    {
                        continue;
                    }
                    const std::uint64_t key = MakeChunkKey(x, z);
                    pendingEditKeys.insert(key);
                    const auto prepared = pendingEditReady.find(key);
                    if (prepared != pendingEditReady.end())
                    {
                        DeleteChunk(prepared->second);
                        pendingEditReady.erase(prepared);
                    }
                    terrainWorker.RegenerateChunk(x, z);
                }
            }
        }

        bool LoadFunctions()
        {
            bool success = true;

            success &= LoadGLFunction(
                GenVertexArrays,
                "glGenVertexArrays"
            );

            success &= LoadGLFunction(
                BindVertexArray,
                "glBindVertexArray"
            );

            success &= LoadGLFunction(
                DeleteVertexArrays,
                "glDeleteVertexArrays"
            );

            success &= LoadGLFunction(
                GenBuffers,
                "glGenBuffers"
            );

            success &= LoadGLFunction(
                BindBuffer,
                "glBindBuffer"
            );

            success &= LoadGLFunction(
                BufferData,
                "glBufferData"
            );

            success &= LoadGLFunction(
                DeleteBuffers,
                "glDeleteBuffers"
            );

            success &= LoadGLFunction(
                EnableVertexAttribArray,
                "glEnableVertexAttribArray"
            );

            success &= LoadGLFunction(
                VertexAttribPointer,
                "glVertexAttribPointer"
            );

            success &= LoadGLFunction(
                DrawElements,
                "glDrawElements"
            );

            return success;
        }

        std::uint64_t MakeChunkKey(int x, int z) const
        {
            return
                (
                    static_cast<std::uint64_t>(
                        static_cast<std::uint32_t>(x)
                    ) << 32
                ) |
                static_cast<std::uint32_t>(z);
        }

        void DeleteChunk(GPUChunk& chunk)
        {
            if (chunk.indexBuffer != 0)
            {
                DeleteBuffers(1, &chunk.indexBuffer);
                chunk.indexBuffer = 0;
            }

            if (chunk.vertexBuffer != 0)
            {
                DeleteBuffers(1, &chunk.vertexBuffer);
                chunk.vertexBuffer = 0;
            }

            if (chunk.vertexArray != 0)
            {
                DeleteVertexArrays(1, &chunk.vertexArray);
                chunk.vertexArray = 0;
            }

            chunk.indexCount = 0;
        }

        bool BuildGPUMesh(
            const std::vector<TerrainVertex>& vertices,
            const std::vector<unsigned int>& indices,
            GPUChunk& gpuChunk)
        {
            if (vertices.empty() || indices.empty())
            {
                return false;
            }

            gpuChunk = {};

            GenVertexArrays(1, &gpuChunk.vertexArray);
            GenBuffers(1, &gpuChunk.vertexBuffer);
            GenBuffers(1, &gpuChunk.indexBuffer);

            if (gpuChunk.vertexArray == 0 ||
                gpuChunk.vertexBuffer == 0 ||
                gpuChunk.indexBuffer == 0)
            {
                DeleteChunk(gpuChunk);
                return false;
            }

            BindVertexArray(gpuChunk.vertexArray);

            BindBuffer(
                GL_ARRAY_BUFFER,
                gpuChunk.vertexBuffer
            );

            BufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    vertices.size() * sizeof(TerrainVertex)
                ),
                vertices.data(),
                GL_STATIC_DRAW
            );

            BindBuffer(
                GL_ELEMENT_ARRAY_BUFFER,
                gpuChunk.indexBuffer
            );

            BufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(
                    indices.size() * sizeof(unsigned int)
                ),
                indices.data(),
                GL_STATIC_DRAW
            );

            const GLsizei stride =
                static_cast<GLsizei>(sizeof(TerrainVertex));

            EnableVertexAttribArray(0);

            VertexAttribPointer(
                0,
                3,
                GL_FLOAT,
                GL_FALSE,
                stride,
                reinterpret_cast<const void*>(
                    offsetof(TerrainVertex, position)
                )
            );

            EnableVertexAttribArray(1);

            VertexAttribPointer(
                1,
                3,
                GL_FLOAT,
                GL_FALSE,
                stride,
                reinterpret_cast<const void*>(
                    offsetof(TerrainVertex, normal)
                )
            );

            EnableVertexAttribArray(2);

            VertexAttribPointer(
                2,
                3,
                GL_FLOAT,
                GL_FALSE,
                stride,
                reinterpret_cast<const void*>(
                    offsetof(TerrainVertex, color)
                )
            );

            BindVertexArray(0);

            gpuChunk.indexCount =
                static_cast<GLsizei>(indices.size());

            return true;
        }

        bool BuildGPUChunk(const TerrainChunk& chunk, GPUChunk& gpuChunk)
        {
            return BuildGPUMesh(chunk.GetVertices(), chunk.GetIndices(), gpuChunk);
        }

        void RefreshForestChunk(int chunkX, int chunkZ)
        {
            const std::uint64_t key = MakeChunkKey(chunkX, chunkZ);
            const auto terrain = terrainWorker.GetVoxelTerrain();
            if (!terrain)
            {
                return;
            }

            auto& trees = treesByChunk[key];
            ForestGenerator::GenerateTrees(*terrain, chunkX, chunkZ, trees);
            std::erase_if(trees, [&](const ForestTree& tree)
            {
                return felledTreeIds.contains(tree.id);
            });
            auto& pebbles = pebblesByChunk[key];
            ForestGenerator::GeneratePebbles(*terrain, chunkX, chunkZ, pebbles);
            std::erase_if(pebbles, [&](const ForestPebble& pebble)
            {
                return gatheredPebbleIds.contains(pebble.id);
            });
            VoxelMesh forestMesh;
            ForestGenerator::BuildMesh(trees, forestMesh);
            VoxelMesh looseMesh;
            ForestGenerator::BuildPebbleMesh(pebbles, looseMesh);
            const unsigned int offset = static_cast<unsigned int>(forestMesh.vertices.size());
            forestMesh.vertices.insert(forestMesh.vertices.end(),
                looseMesh.vertices.begin(), looseMesh.vertices.end());
            for (const auto index : looseMesh.indices)
                forestMesh.indices.push_back(index + offset);

            GPUChunk replacement{};
            if (!forestMesh.IsEmpty() &&
                !BuildGPUMesh(forestMesh.vertices, forestMesh.indices, replacement))
            {
                std::cerr << "Failed to upload forest mesh for ("
                    << chunkX << ", " << chunkZ << ").\n";
                return;
            }

            const auto existing = forestChunks.find(key);
            if (existing != forestChunks.end())
            {
                DeleteChunk(existing->second);
                forestChunks.erase(existing);
            }
            if (replacement.indexCount > 0)
            {
                forestChunks.emplace(key, replacement);
            }
        }

        bool InstallChunk(const TerrainChunk& chunk)
        {
            const std::uint64_t key = MakeChunkKey(
                chunk.GetChunkX(), chunk.GetChunkZ());

            GPUChunk replacement{};
            const bool hasGeometry = !chunk.GetIndices().empty();
            if (hasGeometry && !BuildGPUChunk(chunk, replacement))
            {
                return false;
            }

            auto existing = gpuChunks.find(key);
            if (existing != gpuChunks.end())
            {
                DeleteChunk(existing->second);
                gpuChunks.erase(existing);
            }

            if (hasGeometry)
            {
                gpuChunks.emplace(key, replacement);
                emptyChunks.erase(key);
            }
            else
            {
                emptyChunks.insert(key);
            }

            RefreshForestChunk(chunk.GetChunkX(), chunk.GetChunkZ());
            return true;
        }

        bool CommitEditedChunks()
        {
            if (pendingEditKeys.empty())
            {
                if (visibleCollisionSnapshot)
                {
                    visibleCollisionSnapshot.reset();
                    hasCheckedCameraPosition = false;
                }
                return false;
            }

            if (pendingEditReady.size() != pendingEditKeys.size())
            {
                return false;
            }

            for (const std::uint64_t key : pendingEditKeys)
            {
                auto existing = gpuChunks.find(key);
                if (existing != gpuChunks.end())
                {
                    DeleteChunk(existing->second);
                    gpuChunks.erase(existing);
                }

                auto prepared = pendingEditReady.find(key);
                if (prepared != pendingEditReady.end() &&
                    prepared->second.indexCount > 0)
                {
                    gpuChunks.emplace(key, prepared->second);
                    emptyChunks.erase(key);
                }
                else
                {
                    emptyChunks.insert(key);
                }

                const int chunkX = static_cast<std::int32_t>(key >> 32);
                const int chunkZ = static_cast<std::int32_t>(key & 0xffffffffu);
                RefreshForestChunk(chunkX, chunkZ);
            }

            pendingEditKeys.clear();
            pendingEditReady.clear();
            visibleCollisionSnapshot.reset();
            hasCheckedCameraPosition = false;
            return true;
        }

        void ClearGPUChunks()
        {
            DeleteChunk(campfireProps);
            DeleteChunk(campfireFlames);
            DeleteChunk(campfireSmoke);
            DeleteChunk(furnaceProps);
            DeleteChunk(constructionProps);
            constructionsDirty = true;
            DeleteChunk(furnaceFlames);
            DeleteChunk(furnaceSmoke);
            DeleteChunk(heldMold);
            DeleteChunk(previewMesh);
            placement = PlacementKind::None;
            lastPreviewYaw = -1.0f;
            nearbyStructures.clear();
            player.SetStructureColliders(&nearbyStructures);
            furnacePropsDirty = true;
            furnaceParticlesDirty = true;
            campfirePropsDirty = true;
            campfireParticlesDirty = true;
            fireVisualTime = 0.0f;
            fireVisualRefresh = 0.0f;
            for (auto& entry : gpuChunks)
            {
                DeleteChunk(entry.second);
            }

            gpuChunks.clear();
            for (auto& entry : forestChunks)
            {
                DeleteChunk(entry.second);
            }
            forestChunks.clear();
            treesByChunk.clear();
            pebblesByChunk.clear();
            nearbyTrees.clear();
            treeHarvestId = 0;
            player.SetTreeColliders(&nearbyTrees);
            for (auto& entry : pendingEditReady)
            {
                DeleteChunk(entry.second);
            }
            emptyChunks.clear();
            pendingEditKeys.clear();
            pendingEditReady.clear();
            visibleCollisionSnapshot.reset();
            hasCheckedCameraPosition = false;
            hasPreviousStreamingPosition = false;
            prefetchDirectionX = 0;
            prefetchDirectionZ = 0;
        }

        void UpdatePlayerCameraPose()
        {
            float forwardX = 0.0f;
            float forwardY = 0.0f;
            float forwardZ = 0.0f;

            player.GetForward(forwardX, forwardY, forwardZ);
            playerCameraForward = Normalize({
                forwardX,
                forwardY,
                forwardZ
            });

            const Vec3 feet{
                player.GetX(),
                player.GetY(),
                player.GetZ()
            };

            const auto voxelTerrain = terrainWorker.GetVoxelTerrain();
            const VoxelTerrain* visibleTerrain = visibleCollisionSnapshot
                ? visibleCollisionSnapshot.get() : voxelTerrain.get();

            if (visibleTerrain)
            {
                const std::uint64_t revision = visibleTerrain->GetRevision();
                const Vec3 offset = Subtract(feet, checkedFeetPosition);
                const bool mustCheck = !hasCheckedCameraPosition ||
                    Dot(offset, offset) > 0.00000001f ||
                    revision != checkedTerrainRevision;

                if (!mustCheck)
                {
                    playerCameraPosition = checkedCameraPosition;
                }
                else
                {
                    Vec3 safe{};
                    bool valid = player.GetCollisionSafeCameraPosition(
                        *visibleTerrain, safe.x, safe.y, safe.z);

                    if (!valid && hasSafePlayerFeetPosition)
                    {
                        const Vec3 previousFeet = lastSafePlayerFeetPosition;
                        player.SetPosition(
                            previousFeet.x,
                            previousFeet.y,
                            previousFeet.z);

                        valid = player.GetCollisionSafeCameraPosition(
                            *visibleTerrain, safe.x, safe.y, safe.z);
                    }

                    if (!valid)
                    {
                        const float baseX = player.GetX();
                        const float baseZ = player.GetZ();
                        const float surfaceY =
                            visibleTerrain->GetGenerator().GetHeight(baseX, baseZ);
                        const float minimumY = std::max(
                            player.GetY(), surfaceY + 0.2f);

                        for (float rise : {
                                 0.0f, 0.25f, 0.5f, 1.0f,
                                 2.0f, 3.5f, 5.0f, 8.0f,
                                 12.0f, 20.0f, 32.0f})
                        {
                            player.SetPosition(
                                baseX, minimumY + rise, baseZ);
                            if (player.GetCollisionSafeCameraPosition(
                                    *visibleTerrain, safe.x, safe.y, safe.z))
                            {
                                valid = true;
                                break;
                            }
                        }
                    }

                    if (valid)
                    {
                        playerCameraPosition = safe;
                        lastSafePlayerCameraPosition = safe;
                        hasSafePlayerCameraPosition = true;
                        lastSafePlayerFeetPosition = {
                            player.GetX(), player.GetY(), player.GetZ()};
                        hasSafePlayerFeetPosition = true;
                        checkedFeetPosition = lastSafePlayerFeetPosition;
                        checkedCameraPosition = safe;
                        checkedTerrainRevision = revision;
                        hasCheckedCameraPosition = true;
                    }
                    else
                    {
                        hasCheckedCameraPosition = false;
                        if (hasSafePlayerCameraPosition)
                        {
                            playerCameraPosition = lastSafePlayerCameraPosition;
                        }
                        else
                        {
                            player.GetFirstPersonCameraPosition(
                                playerCameraPosition.x,
                                playerCameraPosition.y,
                                playerCameraPosition.z);
                        }
                    }
                }
            }
            else
            {
                player.GetFirstPersonCameraPosition(
                    playerCameraPosition.x,
                    playerCameraPosition.y,
                    playerCameraPosition.z);
            }

            if (!cinematicMode)
            {
                cameraPosition = playerCameraPosition;
                cameraForward = playerCameraForward;
                cameraTarget = Add(cameraPosition, cameraForward);
            }
        }

        void UpdateActiveCamera()
        {
            if (cinematicMode)
            {
                cameraPosition = cinematicPosition;
                cameraTarget = cinematicTarget;
                cameraForward = Normalize(
                    Subtract(cameraTarget, cameraPosition)
                );
            }
            else
            {
                cameraPosition = playerCameraPosition;
                cameraForward = playerCameraForward;
                cameraTarget = Add(
                    cameraPosition,
                    cameraForward
                );
            }
        }

        void UpdateCinematicPose(
            float deltaTime,
            bool introduction,
            float progress)
        {
            cinematicTime += std::max(deltaTime, 0.0f);

            const Vec3 landingPosition = playerCameraPosition;

            if (introduction)
            {
                const float t = SmoothStep(progress);

                const Vec3 startingPosition{
                    landingPosition.x - 145.0f,
                    landingPosition.y + 185.0f,
                    landingPosition.z + 145.0f
                };

                const Vec3 approachPosition{
                    landingPosition.x - 42.0f,
                    landingPosition.y + 36.0f,
                    landingPosition.z + 48.0f
                };

                const Vec3 finalPosition{
                    landingPosition.x - 5.0f,
                    landingPosition.y + 5.0f,
                    landingPosition.z + 7.0f
                };

                if (t < 0.75f)
                {
                    cinematicPosition = Lerp(
                        startingPosition,
                        approachPosition,
                        SmoothStep(t / 0.75f)
                    );
                }
                else
                {
                    cinematicPosition = Lerp(
                        approachPosition,
                        finalPosition,
                        SmoothStep((t - 0.75f) / 0.25f)
                    );
                }

                const Vec3 distantTarget{
                    landingPosition.x + 28.0f,
                    landingPosition.y,
                    landingPosition.z - 28.0f
                };

                cinematicTarget = Lerp(
                    distantTarget,
                    landingPosition,
                    t
                );
            }
            else
            {
                const float angle =
                    cinematicTime * 0.075f;

                const float radius = 95.0f;

                cinematicPosition = {
                    landingPosition.x +
                        std::cos(angle) * radius,
                    landingPosition.y +
                        72.0f +
                        std::sin(cinematicTime * 0.18f) * 7.0f,
                    landingPosition.z +
                        std::sin(angle) * radius
                };

                cinematicTarget = {
                    landingPosition.x,
                    landingPosition.y - 4.0f,
                    landingPosition.z
                };
            }

            UpdateActiveCamera();
        }

        void SetLighting()
        {
            terrainShader.SetVec3(
                "uLightDirection",
                -0.45f,
                -1.0f,
                -0.35f
            );

            terrainShader.SetVec3(
                "uLightColor",
                1.0f,
                0.96f,
                0.88f
            );

            terrainShader.SetVec3(
                "uAmbientColor",
                0.42f,
                0.48f,
                0.58f
            );

            terrainShader.SetFloat(
                "uLightIntensity",
                1.15f
            );

            terrainShader.SetFloat(
                "uAmbientIntensity",
                0.65f
            );

            terrainShader.SetVec3(
                "uViewPosition",
                cameraPosition.x,
                cameraPosition.y,
                cameraPosition.z
            );
        }

        void UpdateStreamingCenter()
        {
            const float chunkSize = terrainWorld.GetChunkSize();
            if (chunkSize <= 0.0f)
            {
                return;
            }

            const Vec3 center = cinematicMode
                ? cinematicTarget
                : playerCameraPosition;

            if (hasPreviousStreamingPosition)
            {
                const float deltaX = center.x - previousStreamingPosition.x;
                const float deltaZ = center.z - previousStreamingPosition.z;
                const float planarDistanceSquared = deltaX * deltaX + deltaZ * deltaZ;

                if (planarDistanceSquared > 0.0001f &&
                    planarDistanceSquared < chunkSize * chunkSize * 4.0f)
                {
                    prefetchDirectionX = deltaX > 0.01f ? 1 :
                        (deltaX < -0.01f ? -1 : 0);
                    prefetchDirectionZ = deltaZ > 0.01f ? 1 :
                        (deltaZ < -0.01f ? -1 : 0);
                }
                else if (planarDistanceSquared >= chunkSize * chunkSize * 4.0f)
                {
                    prefetchDirectionX = 0;
                    prefetchDirectionZ = 0;
                }
            }

            previousStreamingPosition = center;
            hasPreviousStreamingPosition = true;

            currentChunkX = static_cast<int>(std::floor(center.x / chunkSize));
            currentChunkZ = static_cast<int>(std::floor(center.z / chunkSize));
            terrainWorker.SetCenter(currentChunkX, currentChunkZ);
        }

        bool IsOutsideUnloadRadius(std::uint64_t key) const
        {
            const int chunkX = static_cast<std::int32_t>(key >> 32);
            const int chunkZ = static_cast<std::int32_t>(key & 0xFFFFFFFFu);
            return std::abs(chunkX - currentChunkX) > UnloadRadius ||
                std::abs(chunkZ - currentChunkZ) > UnloadRadius;
        }

        void RemoveDistantChunks()
        {
            for (auto iterator = gpuChunks.begin(); iterator != gpuChunks.end();)
            {
                if (IsOutsideUnloadRadius(iterator->first))
                {
                    DeleteChunk(iterator->second);
                    iterator = gpuChunks.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }

            for (auto iterator = forestChunks.begin(); iterator != forestChunks.end();)
            {
                if (IsOutsideUnloadRadius(iterator->first))
                {
                    DeleteChunk(iterator->second);
                    iterator = forestChunks.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }

            for (auto iterator = treesByChunk.begin(); iterator != treesByChunk.end();)
            {
                if (IsOutsideUnloadRadius(iterator->first))
                    iterator = treesByChunk.erase(iterator);
                else
                    ++iterator;
            }
            for (auto iterator = pebblesByChunk.begin(); iterator != pebblesByChunk.end();)
            {
                if (IsOutsideUnloadRadius(iterator->first))
                    iterator = pebblesByChunk.erase(iterator);
                else
                    ++iterator;
            }
            for (auto iterator = emptyChunks.begin(); iterator != emptyChunks.end();)
            {
                if (IsOutsideUnloadRadius(*iterator))
                {
                    iterator = emptyChunks.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }

            for (auto iterator = pendingEditKeys.begin();
                 iterator != pendingEditKeys.end();)
            {
                if (IsOutsideUnloadRadius(*iterator))
                {
                    auto prepared = pendingEditReady.find(*iterator);
                    if (prepared != pendingEditReady.end())
                    {
                        DeleteChunk(prepared->second);
                        pendingEditReady.erase(prepared);
                    }
                    iterator = pendingEditKeys.erase(iterator);
                }
                else
                {
                    ++iterator;
                }
            }
        }

        void RequestRegion(int centerX, int centerZ)
        {
            for (int z = -LoadRadius; z <= LoadRadius; ++z)
            {
                for (int x = -LoadRadius; x <= LoadRadius; ++x)
                {
                    const int chunkX = centerX + x;
                    const int chunkZ = centerZ + z;
                    const std::uint64_t key = MakeChunkKey(chunkX, chunkZ);

                    if (gpuChunks.find(key) != gpuChunks.end() ||
                        emptyChunks.find(key) != emptyChunks.end() ||
                        pendingEditKeys.find(key) != pendingEditKeys.end())
                    {
                        continue;
                    }

                    terrainWorker.RequestChunk(chunkX, chunkZ);
                }
            }
        }

        void RequestNearbyChunks()
        {
            RequestRegion(currentChunkX, currentChunkZ);

            if (!cinematicMode &&
                (prefetchDirectionX != 0 || prefetchDirectionZ != 0))
            {
                constexpr int LookAheadChunks = 2;
                RequestRegion(
                    currentChunkX + prefetchDirectionX * LookAheadChunks,
                    currentChunkZ + prefetchDirectionZ * LookAheadChunks);
            }
        }

        void UploadCompletedChunks()
        {
            TerrainChunk chunk;
            int consumed = 0;

            while (consumed < MaxUploadsPerFrame &&
                   terrainWorker.TryGetCompletedChunk(chunk))
            {
                ++consumed;

                const std::uint64_t key = MakeChunkKey(
                    chunk.GetChunkX(), chunk.GetChunkZ());

                if (IsOutsideUnloadRadius(key))
                {
                    continue;
                }

                if (pendingEditKeys.find(key) != pendingEditKeys.end())
                {
                    GPUChunk prepared{};
                    if (chunk.GetIndices().empty() ||
                        BuildGPUChunk(chunk, prepared))
                    {
                        auto oldPrepared = pendingEditReady.find(key);
                        if (oldPrepared != pendingEditReady.end())
                        {
                            DeleteChunk(oldPrepared->second);
                        }
                        pendingEditReady.insert_or_assign(key, prepared);
                    }
                    else
                    {
                        std::cerr << "Failed to prepare edited chunk ("
                            << chunk.GetChunkX() << ", " << chunk.GetChunkZ()
                            << "). Retaining the previous mesh.\n";
                        terrainWorker.RegenerateChunk(
                            chunk.GetChunkX(), chunk.GetChunkZ());
                    }
                    continue;
                }

                if (!InstallChunk(chunk))
                {
                    std::cerr << "Failed to upload terrain chunk ("
                        << chunk.GetChunkX() << ", " << chunk.GetChunkZ()
                        << "). The old mesh was retained.\n";
                }
            }

            CommitEditedChunks();
        }

        void StreamTerrain()
        {
            UpdateStreamingCenter();
            RemoveDistantChunks();
            UploadCompletedChunks();
            RequestNearbyChunks();
        }

        bool StartTerrainGeneration()
        {
            terrainWorker.Start(
                terrainWorld.GetSettings(),
                terrainWorld.GetChunkSize(),
                terrainWorld.GetVertexSpacing()
            );

            if (!terrainWorker.IsRunning())
            {
                std::cerr
                    << "Failed to start terrain generation worker.\n";

                return false;
            }

            UpdateStreamingCenter();
            RequestNearbyChunks();

            std::cout
                << "Background terrain generation started. "
                << "Load radius: "
                << LoadRadius
                << ", target chunks: "
                << (LoadRadius * 2 + 1) *
                   (LoadRadius * 2 + 1)
                << ".\n";

            return true;
        }

        void StopTerrainGeneration()
        {
            terrainWorker.Stop();
        }
    };

    Renderer::Renderer()
        : m_impl(std::make_unique<Impl>())
    {
    }

    Renderer::~Renderer()
    {
        Shutdown();
    }

    bool Renderer::Initialize()
    {
        if (!m_impl)
        {
            m_impl = std::make_unique<Impl>();
        }

        if (m_impl->initialized)
        {
            return true;
        }

        if (!m_impl->LoadFunctions())
        {
            return false;
        }

        if (!m_impl->terrainShader.LoadFromFiles(
                "assets/shaders/terrain.vert",
                "assets/shaders/terrain.frag"))
        {
            std::cerr
                << "Failed to load terrain shaders.\n";

            return false;
        }

        if (!m_impl->campfireParticleShader.LoadFromFiles(
                "assets/shaders/campfire.vert",
                "assets/shaders/campfire.frag"))
        {
            m_impl->terrainShader.Shutdown();
            std::cerr << "Failed to load procedural fire shaders.\n";
            return false;
        }

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        glClearColor(
            0.48f,
            0.62f,
            0.78f,
            1.0f
        );

        m_impl->player.Initialize(
            m_impl->terrainWorld.GetGenerator()
        );

        m_impl->UpdatePlayerCameraPose();

        m_impl->cinematicPosition =
            m_impl->playerCameraPosition;

        m_impl->cinematicTarget = Add(
            m_impl->cinematicPosition,
            m_impl->playerCameraForward
        );

        if (!m_impl->StartTerrainGeneration())
        {
            m_impl->terrainShader.Shutdown();
            m_impl->campfireParticleShader.Shutdown();
            return false;
        }

        m_impl->initialized = true;

        std::cout
            << "Renderer initialized in first-person mode "
            << "with independent cinematic camera "
            << "and terrain streaming.\n";

        return true;
    }

    void Renderer::UpdatePlayer(
        float deltaTime,
        float forwardInput,
        float rightInput,
        bool sprint,
        bool jumpPressed)
    {
        if (!m_impl || !m_impl->initialized)
        {
            return;
        }

        PlayerInput input{};

        input.forward = forwardInput;
        input.right = rightInput;
        input.sprint = sprint;
        input.jumpPressed = jumpPressed;

        const std::shared_ptr<const VoxelTerrain> voxelTerrain =
            m_impl->terrainWorker.GetVoxelTerrain();
        const VoxelTerrain* collisionTerrain = m_impl->visibleCollisionSnapshot
            ? m_impl->visibleCollisionSnapshot.get() : voxelTerrain.get();

        if (collisionTerrain)
        {
            if (m_impl->player.GetY() < VoxelTerrain::BedrockSurfaceY - 1.0f)
            {
                const float x = m_impl->player.GetX();
                const float z = m_impl->player.GetZ();
                const float surfaceY = voxelTerrain
                    ? voxelTerrain->GetSurfaceHeight(x, z)
                    : collisionTerrain->GetSurfaceHeight(x, z);
                m_impl->player.SetPosition(x, surfaceY + 3.0f, z);
                m_impl->hasCheckedCameraPosition = false;
                m_impl->hasSafePlayerFeetPosition = false;
                m_impl->hasSafePlayerCameraPosition = false;
            }

            m_impl->RefreshNearbyOutcrops();
            m_impl->RefreshNearbyTrees();
            m_impl->RefreshStructureColliders();
            m_impl->player.Update(
                deltaTime,
                input,
                *collisionTerrain
            );
        }

        m_impl->UpdatePlayerCameraPose();
        m_impl->UpdateActiveCamera();
    }

    void Renderer::RotatePlayerCamera(
        float deltaX,
        float deltaY)
    {
        if (!m_impl || !m_impl->initialized)
        {
            return;
        }

        m_impl->player.Rotate(
            deltaX,
            deltaY
        );

        m_impl->UpdatePlayerCameraPose();
        m_impl->UpdateActiveCamera();
    }

    void Renderer::SetMineralDatabase(const MineralDatabase* catalog)
    {
        if (m_impl)
            m_impl->mineralDatabase=catalog;
    }

    void Renderer::SetEquippedItem(const std::string& itemId)
    {
        if (!m_impl)
            return;
        if (IsEquippableHotbarItem(m_impl->inventory, itemId))
            m_impl->equippedItem = itemId;
        else
            m_impl->equippedItem.clear();
        if (m_impl->placement != Impl::PlacementKind::None &&
            ((m_impl->placement == Impl::PlacementKind::Furnace &&
                m_impl->equippedItem != "furnace_kit") ||
             (m_impl->placement == Impl::PlacementKind::Campfire &&
                m_impl->equippedItem != "campfire_kit") ||
             (m_impl->placement == Impl::PlacementKind::Construction &&
                m_impl->equippedItem != BuildKitId(m_impl->selectedBuildKind))))
        {
            m_impl->placement = Impl::PlacementKind::None;
        }
    }

    bool Renderer::HasPlacementPreview() const
    {
        return m_impl && m_impl->placement != Impl::PlacementKind::None;
    }

    bool Renderer::CancelPlacementPreview()
    {
        if (!HasPlacementPreview())
            return false;
        m_impl->placement = Impl::PlacementKind::None;
        m_impl->Notify("PLACEMENT CANCELLED");
        return true;
    }

    bool Renderer::RotatePlacementPreview(int direction)
    {
        if (!HasPlacementPreview())
            return false;
        const int index = static_cast<int>(m_impl->placementYaw / 45.0f);
        m_impl->placementYaw = static_cast<float>(
            ((index + (direction >= 0 ? 1 : 7)) % 8) * 45);
        m_impl->Notify("PREVIEW ROTATED - PRESS F TO PLACE");
        return true;
    }

    bool Renderer::TogglePlacementPreview()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        if (m_impl->placement == Impl::PlacementKind::None)
        {
            if (m_impl->equippedItem == "furnace_kit" &&
                m_impl->inventory.ItemCount("furnace_kit") > 0)
                m_impl->placement = Impl::PlacementKind::Furnace;
            else if (m_impl->equippedItem == "campfire_kit" &&
                m_impl->inventory.ItemCount("campfire_kit") > 0)
                m_impl->placement = Impl::PlacementKind::Campfire;
            else if (BuildKitKind(m_impl->equippedItem, m_impl->selectedBuildKind) &&
                m_impl->inventory.ItemCount(m_impl->equippedItem) > 0)
                m_impl->placement = Impl::PlacementKind::Construction;
            else
            {
                m_impl->Notify("EQUIP A PLACEABLE KIT IN YOUR HOTBAR");
                return false;
            }
            m_impl->placementYaw = 0.0f;
            m_impl->lastPreviewYaw = -1.0f;
            m_impl->Notify("PREVIEW ACTIVE | R/Q ROTATE | F PLACE | X CANCEL");
            return true;
        }
        const auto pose = m_impl->ComputePlacementPose(m_impl->placement);
        if (!pose.valid)
        {
            m_impl->Notify("PREVIEW RED - MOVE TO LEVEL, UNOBSTRUCTED GROUND");
            return false;
        }
        const bool placed = m_impl->placement == Impl::PlacementKind::Construction
            ? PlaceBuildStructure() :
            m_impl->placement == Impl::PlacementKind::Furnace
            ? PlaceClayFurnace() : PlaceCampfire();
        if (placed)
            m_impl->placement = Impl::PlacementKind::None;
        return placed;
    }

    bool Renderer::PlaceBuildStructure()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        BuildKind kind{};
        if (!BuildKitKind(m_impl->equippedItem,kind) ||
            m_impl->inventory.ItemCount(m_impl->equippedItem) < 1)
            return false;
        const auto pose = m_impl->ComputePlacementPose(Impl::PlacementKind::Construction);
        if (!pose.valid)
        {
            m_impl->Notify("PREVIEW RED - INVALID BUILD LOCATION OR MISSING FOUNDATION");
            return false;
        }
        const auto id=m_impl->constructions.Place(kind,pose.x,pose.y,pose.z,
            m_impl->placementYaw);
        if (!id)
            return false;
        m_impl->inventory.RemoveItem(std::string(BuildKitId(kind)),1);
        m_impl->constructionsDirty = true;
        m_impl->Notify(std::string(BuildPieceName(kind)) + " PLACED");
        return true;
    }

    bool Renderer::FindChestToOpen(std::uint64_t& id, InventoryState& contents) const
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        const auto* s = m_impl->TraceConstruction(5.0f,true);
        if (!s)
            return false;
        id = s->id;
        contents = s->contents;
        return true;
    }

    bool Renderer::SaveChestContents(std::uint64_t id, const InventoryState& contents)
    {
        return m_impl && m_impl->constructions.UpdateChest(id,contents);
    }

    bool Renderer::DismantleBuildStructureAtCrosshair()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        const auto* s = m_impl->TraceConstruction(5.8f);
        if (!s)
            return false;
        const auto id = s->id;
        const BuildKind kind = s->kind;
        if (kind == BuildKind::Chest &&
            (!s->contents.items.empty() || !s->contents.oreBatches.empty()))
        {
            m_impl->Notify("EMPTY THE CHEST BEFORE DISMANTLING IT");
            return true;
        }
        PlayerInventory inventory=m_impl->inventory;
        if (!inventory.AddItem(std::string(BuildKitId(kind)),1))
        {
            m_impl->Notify("NO ROOM TO RECOVER BUILDING KIT");
            return true;
        }
        if (!m_impl->constructions.Remove(id))
            return true;
        m_impl->inventory=std::move(inventory);
        m_impl->constructionsDirty=true;
        m_impl->Notify(std::string(BuildPieceName(kind))+" DISMANTLED");
        return true;
    }

    void Renderer::ReportNoDismantleTarget()
    {
        if (m_impl && m_impl->initialized)
            m_impl->Notify("AIM AT A STRUCTURE TO DISMANTLE - SEE G");
    }

    bool Renderer::GatherTarget()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        ForestPebble stone{};
        float stoneDistance = 0.0f;
        ForestTree tree{};
        float treeDistance = 0.0f;
        const bool hasStone = m_impl->TracePebble(5.0f, stone, stoneDistance) &&
            !m_impl->OreBlocks(stoneDistance);
        const bool hasTree = m_impl->TraceTree(5.0f, tree, treeDistance) &&
            !m_impl->OreBlocks(treeDistance);
        if (hasStone && (!hasTree || stoneDistance < treeDistance))
        {
            if (m_impl->inventory.AddItem("pebble", 1))
            {
                m_impl->gatheredPebbleIds.insert(stone.id);
                const float chunkSize = m_impl->terrainWorld.GetChunkSize();
                m_impl->RefreshForestChunk(
                    static_cast<int>(std::floor(stone.x / chunkSize)),
                    static_cast<int>(std::floor(stone.z / chunkSize)));
                m_impl->Notify("+1 LOOSE PEBBLE");
            }
            else
                m_impl->Notify("INVENTORY FULL - PRESS I");
            return true;
        }
        if (hasTree)
        {
            if (m_impl->strippedTreeIds.contains(tree.id))
            {
                m_impl->Notify("BRANCHES ALREADY COLLECTED");
                return true;
            }
            if (m_impl->inventory.AddItem("stick", 2))
            {
                m_impl->strippedTreeIds.insert(tree.id);
                m_impl->Notify("+2 WOODEN STICKS");
            }
            else
                m_impl->Notify("INVENTORY FULL - PRESS I");
            return true;
        }
        return false;
    }

    bool Renderer::BeginTreeHarvest()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        ForestTree tree{};
        float distance = 0.0f;
        if (!m_impl->TraceTree(6.0f, tree, distance) ||
            m_impl->OreBlocks(distance))
            return false;
        if (!IsUsableAxeId(m_impl->equippedItem) ||
            !m_impl->inventory.HasUsableTool(m_impl->equippedItem))
        {
            m_impl->Notify("STONE AXE REQUIRED - PRESS C TO CRAFT");
            return true;
        }
        if (m_impl->treeHarvestId == tree.id)
            return true;
        m_impl->treeHarvestId = tree.id;
        m_impl->treeHarvestName = m_impl->TreeName(tree.species);
        m_impl->treeHarvestHits = 0;
        m_impl->treeHarvestElapsed = 0.0f;
        m_impl->miningId = 0;
        return true;
    }

    void Renderer::UpdateTreeHarvest(float deltaTime)
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode ||
            m_impl->treeHarvestId == 0)
            return;
        if (!IsUsableAxeId(m_impl->equippedItem) ||
            !m_impl->inventory.HasUsableTool(m_impl->equippedItem))
        {
            m_impl->treeHarvestId = 0;
            return;
        }
        const ForestTree* tree = m_impl->FindTree(m_impl->treeHarvestId);
        if (!tree)
        {
            m_impl->treeHarvestId = 0;
            return;
        }
        const float dx = tree->x - m_impl->player.GetX();
        const float dz = tree->z - m_impl->player.GetZ();
        if (dx * dx + dz * dz > 64.0f)
        {
            m_impl->treeHarvestId = 0;
            return;
        }
        m_impl->treeHarvestElapsed += std::clamp(deltaTime, 0.0f, 0.1f);
        const float chopInterval = m_impl->equippedItem == "copper_axe" ? 0.48f : 0.65f;
        if (m_impl->treeHarvestElapsed < chopInterval)
            return;
        m_impl->treeHarvestElapsed -= chopInterval;
        ++m_impl->treeHarvestHits;
        bool broke = false;
        const std::string axe = m_impl->equippedItem;
        if (!m_impl->inventory.UseTool(axe, 1, broke))
        {
            m_impl->treeHarvestId = 0;
            return;
        }
        if (broke)
        {
            m_impl->equippedItem.clear();
            m_impl->Notify("AXE BROKE");
        }
        if (m_impl->treeHarvestHits < (axe == "copper_axe" ? 3 : 4))
        {
            if (broke)
                m_impl->treeHarvestId = 0;
            return;
        }
        if (!m_impl->inventory.AddItem("log", 3))
        {
            m_impl->Notify("INVENTORY FULL - PRESS I");
            m_impl->treeHarvestId = 0;
            return;
        }
        const float x = tree->x;
        const float z = tree->z;
        m_impl->felledTreeIds.insert(tree->id);
        m_impl->treeHarvestId = 0;
        const float chunkSize = m_impl->terrainWorld.GetChunkSize();
        m_impl->RefreshForestChunk(
            static_cast<int>(std::floor(x / chunkSize)),
            static_cast<int>(std::floor(z / chunkSize)));
        m_impl->RefreshNearbyTrees();
        m_impl->Notify(std::string("+3 WOOD LOGS") +
            (broke ? " | AXE BROKE" : ""));
    }

    bool Renderer::PlaceCampfire()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        if (m_impl->equippedItem != "campfire_kit" ||
            m_impl->inventory.ItemCount("campfire_kit") == 0)
        {
            m_impl->Notify("SELECT A CAMPFIRE KIT ON THE HOTBAR");
            return false;
        }
        const auto pose = m_impl->ComputePlacementPose(Impl::PlacementKind::Campfire);
        if (!pose.valid)
        {
            m_impl->Notify("CAMPFIRE NEEDS LEVEL, CLEAR GROUND");
            return false;
        }
        const float x = pose.x;
        const float y = pose.y;
        const float z = pose.z;
        for (const auto& tree : m_impl->nearbyTrees)
        {
            const float dx=tree.x-x;
            const float dz=tree.z-z;
            if (dx*dx+dz*dz<2.0f*2.0f)
            {
                m_impl->Notify("CAMPFIRE TOO CLOSE TO A TREE");
                return false;
            }
        }
        if (!m_impl->campfires.Place(x,y,z,m_impl->placementYaw))
        {
            m_impl->Notify("NOT ENOUGH SPACE FOR A CAMPFIRE");
            return false;
        }
        m_impl->inventory.RemoveItem("campfire_kit",1);
        m_impl->campfirePropsDirty=true;
        m_impl->campfireParticlesDirty=true;
        m_impl->Notify("CAMPFIRE PLACED - E TO IGNITE");
        return true;
    }

    bool Renderer::PlaceClayFurnace()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        if (m_impl->equippedItem != "furnace_kit" ||
            m_impl->inventory.ItemCount("furnace_kit") == 0)
            return false;
        const auto pose = m_impl->ComputePlacementPose(Impl::PlacementKind::Furnace);
        if (!pose.valid)
        {
            m_impl->Notify("FURNACE NEEDS LEVEL, CLEAR GROUND");
            return false;
        }
        const float x = pose.x;
        const float y = pose.y;
        const float z = pose.z;
        for (const auto& tree:m_impl->nearbyTrees)
        {
            const float dx=tree.x-x, dz=tree.z-z;
            if (dx*dx+dz*dz<9.0f)
            {
                m_impl->Notify("FURNACE IS TOO CLOSE TO A TREE");
                return false;
            }
        }
        for (const auto& fire:m_impl->campfires.Fires())
        {
            const float dx=fire.x-x, dz=fire.z-z;
            if (dx*dx+dz*dz<6.25f)
            {
                m_impl->Notify("MOVE AWAY FROM THE CAMPFIRE");
                return false;
            }
        }
        if (!m_impl->furnaces.Place(x,y,z,m_impl->placementYaw))
        {
            m_impl->Notify("CANNOT PLACE ANOTHER FURNACE HERE");
            return false;
        }
        m_impl->inventory.RemoveItem("furnace_kit",1);
        m_impl->furnacePropsDirty=true;
        m_impl->constructionsDirty=true;
        m_impl->furnaceParticlesDirty=true;
        m_impl->Notify("CLAY FURNACE PLACED | E LOAD ORE | R CHARCOAL");
        return true;
    }

    bool Renderer::CycleFurnaceOre()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode ||
            !m_impl->TraceFurnace(6.0f))
            return false;
        const auto& ores = m_impl->inventory.OreBatches();
        if (ores.empty())
        {
            m_impl->Notify("NO ORE BATCHES IN INVENTORY");
            return true;
        }
        m_impl->selectedFurnaceOre = (m_impl->selectedFurnaceOre + 1) % ores.size();
        const auto& selected = ores[m_impl->selectedFurnaceOre];
        m_impl->Notify("FURNACE FEED SELECTED: " + selected.displayName + " (" +
            std::to_string(static_cast<int>(selected.massKg)) + " KG)");
        return true;
    }

    bool Renderer::DiscardFurnaceResidue()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        const auto* target = m_impl->TraceFurnace(6.0f);
        if (!target)
            return false;
        if (!m_impl->furnaces.DiscardResidue(target->id))
        {
            m_impl->Notify("ONLY COLD, UNCASTABLE RESIDUE BELOW 250 G CAN BE DISCARDED");
            return true;
        }
        m_impl->furnacePropsDirty = true;
        m_impl->Notify("DISCARDED SMALL METAL RESIDUE - NEW METAL MAY BE LOADED");
        return true;
    }

    bool Renderer::InteractClayFurnace()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        const auto* target = m_impl->TraceFurnace(6.0f);
        if (!target)
            return false;
        const auto id = target->id;
        if (target->castCount != 0)
        {
            m_impl->Notify(target->castingCoolingSeconds > 0.0f ?
                "CAST COOLING - PRESS T WHEN READY" :
                "INGOTS READY - PRESS T TO COLLECT");
            return true;
        }
        if (target->readyCopperKg + 0.0001f >= ClayFurnaceWorld::IngotMassKg)
        {
            if (target->moldCapacity == 0)
            {
                const bool multiple = m_impl->inventory.ItemCount("multi_ingot_mold") > 0;
                const std::string mold = multiple ? "multi_ingot_mold" : "ingot_mold";
                if (m_impl->inventory.ItemCount(mold) == 0)
                {
                    m_impl->Notify("NEED AN INGOT MOLD OR EIGHT-INGOT CASTING TRAY");
                    return true;
                }
                PlayerInventory updated = m_impl->inventory;
                if (!updated.RemoveItem(mold, 1) ||
                    !m_impl->furnaces.InstallMold(id, multiple ? 8u : 1u))
                {
                    m_impl->Notify("CANNOT INSTALL CASTING MOLD");
                    return true;
                }
                m_impl->inventory = updated;
            }
            const auto cast = m_impl->furnaces.StartCasting(id);
            if (cast != 0)
            {
                m_impl->furnacePropsDirty = true;
                m_impl->Notify("POURED " + std::to_string(cast) + " " +
                    PrimitiveMetalName(target->metalId) +
                    " INGOTS - COOL 30 SECONDS | T TO COLLECT");
            }
            return true;
        }
        if (target->chargeCopperKg > 0.0f)
        {
            m_impl->Notify("FURNACE IS SMELTING - R TO ADD CHARCOAL");
            return true;
        }
        const auto* catalog = m_impl->mineralDatabase;
        if (!catalog)
        {
            m_impl->Notify("GEOLOGICAL CATALOG NOT READY");
            return true;
        }
        const auto& ores = m_impl->inventory.OreBatches();
        if (ores.empty())
        {
            m_impl->Notify("NO ORE IN INVENTORY - V CYCLES ORE BATCHES");
            return true;
        }
        const std::size_t i = m_impl->selectedFurnaceOre % ores.size();
        const auto& batch = ores[i];
        const auto feed = EvaluatePrimitiveMetalFeed(batch, *catalog);
        if (!feed.supported)
        {
            m_impl->Notify(feed.message + " | PRESS V TO SELECT ANOTHER ORE");
            return true;
        }
        if (target->readyCopperKg > 0.00001f && target->metalId != feed.metalId)
        {
            m_impl->Notify("CAST REMAINING " +
                std::string(PrimitiveMetalName(target->metalId)) +
                " BEFORE SWITCHING METALS");
            return true;
        }
        const float available = ClayFurnaceWorld::MaximumReadyCopperKg - target->readyCopperKg;
        const float mass = std::min(batch.massKg,
            available / std::max(0.00001f, feed.recoveredMetalFraction));
        if (mass < 0.01f)
        {
            m_impl->Notify("FURNACE METAL RESERVOIR IS FULL - CAST INGOTS");
            return true;
        }
        const float recovered = mass * feed.recoveredMetalFraction;
        const float duration = std::min(ClayFurnaceWorld::MaximumSmeltSeconds,
            30.0f + mass * 0.6f);
        PlayerInventory updated = m_impl->inventory;
        if (!updated.RemoveOre(i, mass))
        {
            m_impl->Notify("CANNOT REMOVE SELECTED ORE BATCH");
            return true;
        }
        if (!m_impl->furnaces.AddCharge(id, recovered, batch.displayName,
            feed.metalId, duration))
        {
            m_impl->Notify("FURNACE CANNOT ACCEPT THIS ORE CHARGE");
            return true;
        }
        m_impl->inventory = updated;
        m_impl->Notify("LOADED " + std::to_string(static_cast<int>(mass)) +
            " KG " + batch.displayName + " | YIELD " +
            std::to_string(static_cast<int>(recovered * 1000.0f)) +
            " G " + PrimitiveMetalName(feed.metalId) + " | R: CHARCOAL");
        return true;
    }

    bool Renderer::FuelClayFurnace()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        const auto* target=m_impl->TraceFurnace(6.0f);
        if (!target)
            return false;
        if (m_impl->inventory.ItemCount("charcoal")<1)
        {
            m_impl->Notify("NEED CHARCOAL FROM A COVERED CAMPFIRE");
            return true;
        }
        if (!m_impl->furnaces.AddCharcoal(target->id))
        {
            m_impl->Notify("FURNACE FUEL CHAMBER ALREADY FULL");
            return true;
        }
        m_impl->inventory.RemoveItem("charcoal",1);
        m_impl->furnaceParticlesDirty=true;
        m_impl->Notify("FURNACE HEATING: +45 SECONDS CHARCOAL");
        return true;
    }

    bool Renderer::CollectCooledCopper()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        const auto* target = m_impl->TraceFurnace(6.0f);
        if (target)
        {
            const auto id = target->id;
            if (target->castCount > 0)
            {
                if (target->castingCoolingSeconds > 0.0f)
                {
                    m_impl->Notify("INGOT CASTS STILL COOLING");
                    return true;
                }
                const auto amount = target->castCount;
                const std::string metal = target->metalId;
                PlayerInventory updated = m_impl->inventory;
                if (!updated.AddItem(PrimitiveIngotId(metal), amount))
                {
                    m_impl->Notify("INVENTORY FULL - FREE SPACE FOR CAST INGOTS");
                    return true;
                }
                if (m_impl->furnaces.CollectCasting(id) != amount)
                    return true;
                m_impl->inventory = updated;
                m_impl->furnacePropsDirty = true;
                m_impl->Notify("COLLECTED " + std::to_string(amount) + " " +
                    PrimitiveMetalName(metal) + " INGOTS | MOLD STAYS INSTALLED");
                return true;
            }
            if (target->moldCapacity > 0)
            {
                if (target->fuelSeconds > 0.0f || target->chargeCopperKg > 0.0f)
                {
                    m_impl->Notify("LET THE FURNACE COOL BEFORE REMOVING THE MOLD");
                    return true;
                }
                const auto capacity = target->moldCapacity;
                PlayerInventory updated = m_impl->inventory;
                if (!updated.AddItem(capacity == 8 ? "multi_ingot_mold" : "ingot_mold", 1))
                {
                    m_impl->Notify("INVENTORY FULL - CANNOT RECOVER CASTING MOLD");
                    return true;
                }
                if (m_impl->furnaces.UninstallMold(id) == 0)
                    return true;
                m_impl->inventory = updated;
                m_impl->Notify("REMOVED REUSABLE CASTING MOLD");
                return true;
            }
            m_impl->Notify("NO CAST INGOTS OR INSTALLED MOLD TO COLLECT");
            return true;
        }
        if (m_impl->inventory.ItemCount("cooled_copper_mold") < 1)
            return false;
        PlayerInventory updated = m_impl->inventory;
        if (!updated.RemoveItem("cooled_copper_mold", 1) ||
            !updated.AddItem("copper_ingot", 1) ||
            !updated.AddItem("ingot_mold", 1))
        {
            m_impl->Notify("NEED INVENTORY SPACE FOR INGOT AND MOLD");
            return true;
        }
        m_impl->inventory = updated;
        m_impl->moldCoolingSeconds = 0.0f;
        m_impl->Notify("+1 COPPER INGOT | LEGACY MOLD RECOVERED");
        return true;
    }

    void Renderer::UpdateClayFurnaces(float dt)
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return;
        const bool hadHot=m_impl->inventory.ItemCount("hot_copper_mold")>0;
        const auto states=m_impl->furnaces.Furnaces();
        m_impl->furnaces.Update(dt);
        if (states.size()==m_impl->furnaces.Furnaces().size())
        {
            for (std::size_t i=0;i<states.size();++i)
                if (states[i].chargeCopperKg!=m_impl->furnaces.Furnaces()[i].chargeCopperKg ||
                    states[i].readyCopperKg!=m_impl->furnaces.Furnaces()[i].readyCopperKg ||
                    states[i].castCount!=m_impl->furnaces.Furnaces()[i].castCount ||
                    (states[i].castingCoolingSeconds>0.0f &&
                     m_impl->furnaces.Furnaces()[i].castingCoolingSeconds<=0.0f))
                {
                    m_impl->furnacePropsDirty=true;
                    break;
                }
        }
        m_impl->furnaceParticlesDirty=true;
        if (!hadHot)
            return;
        m_impl->moldCoolingSeconds=std::max(0.0f,
            m_impl->moldCoolingSeconds-std::max(0.0f,dt));
        if (m_impl->moldCoolingSeconds>0.0f)
            return;
        PlayerInventory updated=m_impl->inventory;
        if (updated.RemoveItem("hot_copper_mold",1) &&
            updated.AddItem("cooled_copper_mold",1))
        {
            m_impl->inventory=updated;
            m_impl->Notify("COPPER COOLED - PRESS T TO TAKE INGOT");
        }
    }

    bool Renderer::DismantleClayFurnaceAtCrosshair()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        const auto* target=m_impl->TraceFurnace(6.0f);
        if (!target)
            return false;
        if (target->fuelSeconds>0.0f || target->chargeCopperKg>0.0f ||
            target->moldCapacity != 0 || target->castCount != 0 ||
            target->readyCopperKg+0.0001f>=ClayFurnaceWorld::IngotMassKg)
        {
            m_impl->Notify("COOL FURNACE, RECOVER MOLD WITH T, AND CAST ITS METAL");
            return true;
        }
        PlayerInventory updated=m_impl->inventory;
        if (!updated.AddItem("clay_soil",20) ||
            !updated.AddItem("raw_stone",8) ||
            !updated.AddItem("stick",6) ||
            !updated.AddItem("log",4))
        {
            m_impl->Notify("NOT ENOUGH INVENTORY SPACE TO RECOVER FURNACE");
            return true;
        }
        const bool residueLost=target->readyCopperKg>0.0001f;
        if (!m_impl->furnaces.Remove(target->id))
            return true;
        m_impl->inventory=updated;
        m_impl->furnacePropsDirty=true;
        m_impl->furnaceParticlesDirty=true;
        m_impl->Notify(residueLost ?
            "FURNACE DISMANTLED | SMALL COPPER RESIDUE LOST" :
            "CLAY FURNACE DISMANTLED INTO MATERIALS");
        return true;
    }

    bool Renderer::DismantleCampfireAtCrosshair()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        const CampfireState* target = m_impl->TraceCampfire(5.5f);
        if (!target)
            return false;

        const auto result = industrial_frontier::DismantleCampfire(
            m_impl->campfires, m_impl->inventory, target->id);
        switch (result)
        {
        case CampfireDismantleResult::Success:
            m_impl->campfirePropsDirty = true;
            m_impl->campfireParticlesDirty = true;
            m_impl->Notify("RECOVERED 6 RAW STONE, 2 WOOD LOGS, 3 STICKS");
            break;
        case CampfireDismantleResult::StillBurning:
            m_impl->Notify("WAIT FOR THE FIRE TO BURN OUT BEFORE DISMANTLING");
            break;
        case CampfireDismantleResult::CharcoalRemaining:
            m_impl->Notify("COLLECT THE CHARCOAL WITH R BEFORE DISMANTLING");
            break;
        case CampfireDismantleResult::StillCovered:
            m_impl->Notify("UNCOVER THE COLD FIRE WITH R FIRST");
            break;
        case CampfireDismantleResult::InventoryFull:
            m_impl->Notify("NEED 3 MATERIAL STACKS AND 13.6 KG FREE TO DISMANTLE");
            break;
        case CampfireDismantleResult::NotFound:
            break;
        }
        return true;
    }

    bool Renderer::InteractCampfire()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        const CampfireState* target=m_impl->TraceCampfire(5.5f);
        if (!target)
            return false;
        const std::uint64_t id=target->id;
        if (target->covered)
        {
            m_impl->Notify("CAMPFIRE COVERED - WAIT FOR CHARCOAL");
            return true;
        }
        const bool unlit=target->fuelSeconds<=0.0f;
        if (m_impl->inventory.ItemCount("log")<1 ||
            (unlit && m_impl->inventory.ItemCount("stick")<1))
        {
            m_impl->Notify(unlit ?
                "TO LIGHT: 1 WOOD LOG + 1 WOODEN STICK" :
                "NEED A WOOD LOG TO ADD FUEL");
            return true;
        }
        if (!m_impl->campfires.AddLog(id))
            return true;
        m_impl->inventory.RemoveItem("log",1);
        if (unlit)
            m_impl->inventory.RemoveItem("stick",1);
        m_impl->campfireParticlesDirty=true;
        m_impl->Notify(unlit ? "CAMPFIRE IGNITED - 45 SECOND BURN" :
            "+45 SECONDS OF FIREWOOD FUEL");
        return true;
    }

    bool Renderer::CollectCampfireCharcoal()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return false;
        const auto* target=m_impl->TraceCampfire(5.5f);
        if (!target)
            return false;
        const auto id=target->id;
        if (!target->covered && target->fuelSeconds>0.0f)
        {
            if (m_impl->inventory.ItemCount("clay_soil")<1)
            {
                m_impl->Notify("NEED 1 CLAY SOIL TO COVER FIRE FOR CHARCOAL");
                return true;
            }
            if (m_impl->campfires.Cover(id))
            {
                m_impl->inventory.RemoveItem("clay_soil",1);
                m_impl->campfirePropsDirty=true;
                m_impl->campfireParticlesDirty=true;
                m_impl->Notify("COVERED WITH CLAY - WOOD NOW SMOULDERING");
            }
            return true;
        }
        const std::uint32_t quantity=target->charcoalReady;
        if (quantity==0)
        {
            if (target->covered && target->fuelSeconds<=0.0f &&
                m_impl->campfires.Uncover(id))
            {
                m_impl->campfirePropsDirty=true;
                m_impl->Notify("CAMPFIRE UNCOVERED");
            }
            else
                m_impl->Notify(target->covered ?
                    "CHARCOAL IS STILL SMOULDERING" :
                    "LIGHT THE FIRE WITH E FIRST");
            return true;
        }
        if (!m_impl->inventory.CanAcceptItem("charcoal",quantity))
        {
            m_impl->Notify("NOT ENOUGH INVENTORY SPACE FOR CHARCOAL");
            return true;
        }
        std::uint32_t obtained=0;
        if (m_impl->campfires.Collect(id,obtained) &&
            m_impl->inventory.AddItem("charcoal",obtained))
        {
            m_impl->campfirePropsDirty=true;
            m_impl->Notify("+"+std::to_string(obtained)+" CHARCOAL");
        }
        return true;
    }

    void Renderer::UpdateCampfires(float deltaTime)
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
            return;
        m_impl->campfires.Update(deltaTime);
        m_impl->fireVisualTime+=std::clamp(deltaTime,0.0f,0.1f);
        m_impl->fireVisualRefresh+=std::clamp(deltaTime,0.0f,0.1f);
        if (m_impl->fireVisualRefresh>=0.11f)
        {
            m_impl->fireVisualRefresh=0.0f;
            m_impl->campfireParticlesDirty=true;
        }
    }

    bool Renderer::CraftPrimitive(const std::string& recipeId,
        std::string& message)
    {
        return m_impl && m_impl->initialized &&
            m_impl->inventory.Craft(recipeId, message);
    }

    bool Renderer::BeginOreMining()
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
        {
            return false;
        }
        float hitDistance = 0.0f;
        const OutcropCollisionShape* shape = m_impl->TraceOutcrop(8.0f, hitDistance);
        if (shape && m_impl->TraceVoxelDistance(8.0f) + 0.04f < hitDistance)
        {
            shape = nullptr;
        }
        GeologicalDeposit vein{};
        Vec3 veinPoint{};
        const bool foundVein = !shape && m_impl->TraceExposedVein(
            8.0f, veinPoint, vein);
        if (!shape && !foundVein)
        {
            return false;
        }
        if (PickaxeTier(m_impl->equippedItem) == MiningTier::None ||
            !m_impl->inventory.HasUsableTool(m_impl->equippedItem))
        {
            m_impl->Notify("EQUIP A PICKAXE TO MINE ORE");
            return true;
        }
        const std::uint64_t id = shape ? shape->id : vein.id;
        if (m_impl->miningId == id)
        {
            return true;
        }
        const std::string mineralId = shape ? shape->mineralId : vein.mineralId;
        if (!CanMineMineral(m_impl->equippedItem, mineralId))
        {
            m_impl->Notify("MINERAL ABOVE EQUIPPED PICKAXE TIER - CHECK FIELD GUIDE");
            return true;
        }
        const auto terrain = m_impl->terrainWorker.GetVoxelTerrain();
        const GeologyGenerator* geology = terrain ? terrain->GetGeology() : nullptr;
        const MineralDefinition* mineral = geology
            ? geology->FindMineral(mineralId) : nullptr;
        float hardness = 4.0f;
        if (mineral && mineral->hasMohsHardness)
        {
            hardness = 0.5f * (mineral->minimumMohsHardness +
                mineral->maximumMohsHardness);
        }
        m_impl->treeHarvestId = 0;
        m_impl->miningId = id;
        m_impl->activeMiningTool = m_impl->equippedItem;
        m_impl->miningIsVein = foundVein;
        m_impl->miningPoint = veinPoint;
        m_impl->miningName = shape ? shape->mineralName :
            (mineral ? mineral->name : mineralId);
        m_impl->miningHasDeposit = false;
        if (foundVein)
        {
            m_impl->miningDeposit = vein;
            m_impl->miningHasDeposit = true;
        }
        else if (shape && geology)
        {
            for (const auto& deposit : geology->FindSurfaceDeposits(
                shape->x - 1.0f, shape->x + 1.0f,
                shape->z - 1.0f, shape->z + 1.0f))
            {
                if (deposit.id == id)
                {
                    m_impl->miningDeposit = deposit;
                    m_impl->miningHasDeposit = true;
                    break;
                }
            }
        }
        const float difficulty = m_impl->activeMiningTool == "copper_pickaxe"
            ? 2.5f + hardness * 0.49f : 3.0f + hardness * 0.65f;
        m_impl->miningHitsRequired = std::clamp(
            static_cast<int>(std::ceil(difficulty)), 3, 10);
        m_impl->miningHits = 0;
        m_impl->miningElapsed = 0.0f;
        m_impl->completedFeedback.clear();
        m_impl->completedFeedbackTimer = 0.0f;
        return true;
    }

    void Renderer::UpdateOreMining(float deltaTime)
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode)
        {
            return;
        }
        m_impl->completedFeedbackTimer = std::max(0.0f,
            m_impl->completedFeedbackTimer - std::max(0.0f, deltaTime));
        if (m_impl->miningId == 0)
        {
            return;
        }
        if (m_impl->equippedItem != m_impl->activeMiningTool ||
            PickaxeTier(m_impl->activeMiningTool) == MiningTier::None ||
            !m_impl->inventory.HasUsableTool(m_impl->activeMiningTool))
        {
            m_impl->miningId = 0;
            m_impl->Notify("PICKAXE UNEQUIPPED OR BROKEN");
            return;
        }
        m_impl->RefreshNearbyOutcrops();
        const OutcropCollisionShape* selected = nullptr;
        if (!m_impl->miningIsVein)
        {
            for (const OutcropCollisionShape& shape : m_impl->nearbyOutcrops)
            {
                if (shape.id == m_impl->miningId)
                {
                    selected = &shape;
                    break;
                }
            }
            if (!selected)
            {
                m_impl->miningId = 0;
                return;
            }
        }
        const float targetX = m_impl->miningIsVein
            ? m_impl->miningPoint.x : selected->x;
        const float targetZ = m_impl->miningIsVein
            ? m_impl->miningPoint.z : selected->z;
        const float range = m_impl->miningIsVein
            ? 9.0f : 8.0f + selected->radius;
        const float dx = m_impl->player.GetX() - targetX;
        const float dz = m_impl->player.GetZ() - targetZ;
        if (dx * dx + dz * dz > range * range)
        {
            m_impl->miningId = 0;
            return;
        }
        m_impl->miningElapsed += std::clamp(deltaTime, 0.0f, 0.1f);
        const float strikeInterval = m_impl->activeMiningTool == "copper_pickaxe"
            ? 0.54f : 0.72f;
        if (m_impl->miningElapsed < strikeInterval)
        {
            return;
        }
        m_impl->miningElapsed -= strikeInterval;
        bool broke = false;
        if (!m_impl->inventory.UseTool(m_impl->activeMiningTool, 1, broke))
        {
            m_impl->miningId = 0;
            return;
        }
        ++m_impl->miningHits;
        if (broke)
        {
            m_impl->equippedItem.clear();
            m_impl->Notify("PICKAXE BROKE");
        }
        if (m_impl->miningHits < m_impl->miningHitsRequired)
        {
            if (broke)
                m_impl->miningId = 0;
            return;
        }
        const auto oreTerrain = m_impl->terrainWorker.GetVoxelTerrain();
        const GeologyGenerator* oreGeology = oreTerrain
            ? oreTerrain->GetGeology() : nullptr;
        const InventoryOreBatch reward = m_impl->miningHasDeposit && oreGeology
            ? CreateMiningBatch(m_impl->miningDeposit, *oreGeology,
                m_impl->miningIsVein ? 4.0f : 3.0f)
            : InventoryOreBatch{};
        if (!m_impl->inventory.CanAccept(reward))
        {
            m_impl->completedFeedback = reward.depositId == 0
                ? "ORE ASSAY UNAVAILABLE" : "INVENTORY FULL - PRESS I";
            m_impl->completedFeedbackTimer = 3.0f;
            m_impl->miningId = 0;
            m_impl->miningElapsed = 0.0f;
            m_impl->miningHits = 0;
            m_impl->miningHitsRequired = 0;
            return;
        }
        if (m_impl->miningIsVein)
        {
            const bool success = m_impl->ExcavateAtPoint(
                m_impl->miningPoint, 1.55f);
            if (success)
            {
                m_impl->inventory.AddOre(reward);
            }
            m_impl->completedFeedback = success
                ? "+4 KG ORE COLLECTED" : "COULD NOT EXCAVATE VEIN";
            m_impl->completedFeedbackTimer = 3.0f;
            m_impl->miningId = 0;
            m_impl->miningIsVein = false;
            m_impl->miningElapsed = 0.0f;
            m_impl->miningHits = 0;
            m_impl->miningHitsRequired = 0;
            return;
        }
        const OutcropCollisionShape mined = *selected;
        bool collected = false;
        if (mined.kind == DepositKind::PermanentNode)
        {
            collected = m_impl->inventory.AddOre(reward);
        }
        else if (m_impl->terrainWorker.DepleteDeposit(mined.id))
        {
            collected = m_impl->inventory.AddOre(reward);
            m_impl->RegenerateOutcropArea(mined);
            m_impl->lastOutcropRevision = std::numeric_limits<std::uint64_t>::max();
            m_impl->RefreshNearbyOutcrops();
        }
        m_impl->completedFeedback = collected
            ? "+3 KG ORE COLLECTED" : "EXTRACTION FAILED";
        m_impl->completedFeedbackTimer = 3.0f;
        m_impl->miningId = 0;
        m_impl->miningElapsed = 0.0f;
        m_impl->miningHits = 0;
        m_impl->miningHitsRequired = 0;
    }

    bool Renderer::DigTerrain(float radius, float reach)
    {
        if (!m_impl || !m_impl->initialized || m_impl->cinematicMode ||
            radius <= 0.0f || reach <= 0.0f)
        {
            return false;
        }
        ForestTree tree{};
        float treeDistance = 0.0f;
        if (m_impl->TraceTree(reach, tree, treeDistance) &&
            !m_impl->OreBlocks(treeDistance))
            return false;
        float oreDistance = 0.0f;
        if (m_impl->TraceOutcrop(reach, oreDistance) &&
            oreDistance <= m_impl->TraceVoxelDistance(reach) + 0.04f)
        {
            return false;
        }
        const Vec3 origin = m_impl->playerCameraPosition;
        const Vec3 direction = Normalize(m_impl->playerCameraForward);
        const float step = 0.20f;
        float previousDensity = m_impl->terrainWorker.SampleDensity(
            origin.x, origin.y, origin.z);
        for (float distance = step; distance <= reach; distance += step)
        {
            const Vec3 point = Add(origin, Multiply(direction, distance));
            const float density = m_impl->terrainWorker.SampleDensity(
                point.x, point.y, point.z);
            if (previousDensity <= 0.0f && density > 0.0f)
            {
                const auto terrain = m_impl->terrainWorker.GetVoxelTerrain();
                if (!terrain)
                    return false;
                const float normalY = 1.0f;
                const auto material = terrain->GetMaterialAt(
                    point.x, point.y - 0.10f, point.z, normalY);
                const std::string itemId = material == TerrainMaterial::Stone
                    ? "raw_stone" :
                    (material == TerrainMaterial::DirtClay ||
                        material == TerrainMaterial::Grass ||
                        material == TerrainMaterial::Snow ? "clay_soil" : "");
                const std::string equipped = m_impl->equippedItem;
                const bool toolApplied = !itemId.empty() &&
                    CanExcavateTerrain(equipped, itemId == "raw_stone") &&
                    m_impl->inventory.HasUsableTool(equipped);
                if (material == TerrainMaterial::Bedrock)
                {
                    m_impl->Notify("BEDROCK CANNOT BE EXCAVATED");
                    return false;
                }
                if (!toolApplied || itemId.empty())
                {
                    m_impl->Notify(itemId == "raw_stone" || itemId.empty()
                        ? "EQUIP A STONE OR COPPER PICKAXE TO MINE ROCK"
                        : "EQUIP A STONE OR COPPER SHOVEL TO DIG SOIL");
                    return false;
                }
                const std::uint32_t count =
                    (equipped == "copper_pickaxe" || equipped == "copper_shovel") ? 3u : 2u;
                if (!m_impl->inventory.CanAcceptItem(itemId, count))
                {
                    m_impl->Notify("INVENTORY FULL - PRESS I");
                    return false;
                }
                if (!m_impl->ExcavateAtPoint(point, radius))
                    return false;
                m_impl->inventory.AddItem(itemId, count);
                bool toolBroke = false;
                if (toolApplied &&
                    m_impl->inventory.UseTool(equipped, 1, toolBroke) &&
                    toolBroke)
                    m_impl->equippedItem.clear();
                m_impl->Notify("+" + std::to_string(count) +
                    (itemId == "raw_stone" ? " RAW STONE" : " CLAY SOIL") +
                    (toolBroke ? " | TOOL BROKE" : ""));
                return true;
            }
            previousDensity = density;
        }
        return false;
    }

    bool Renderer::GetHUDWorldState(HUDWorldState& state) const
    {
        if (!m_impl || !m_impl->initialized)
        {
            return false;
        }

        state.targetAssay.clear();
        state.x = m_impl->player.GetX();
        state.y = m_impl->player.GetY();
        state.z = m_impl->player.GetZ();
        state.grounded = m_impl->player.IsGrounded();
        state.terrainUpdating = !m_impl->pendingEditKeys.empty();
        state.loadedChunks = m_impl->gpuChunks.size();

        if (!m_impl->cinematicMode)
        {
            if (m_impl->placement != Impl::PlacementKind::None)
            {
                const auto pose = m_impl->ComputePlacementPose(m_impl->placement);
                state.hasTarget = true;
                state.targetIsOre = true;
                state.targetMaterial = m_impl->placement == Impl::PlacementKind::Furnace
                    ? "CLAY FURNACE PREVIEW" :
                    m_impl->placement == Impl::PlacementKind::Campfire
                    ? "CAMPFIRE PREVIEW" :
                    std::string(BuildPieceName(m_impl->selectedBuildKind)) + " PREVIEW";
                state.targetAssay = pose.valid
                    ? "VALID PLACEMENT | " : "BLOCKED PLACEMENT | ";
                state.targetAssay += std::to_string(static_cast<int>(m_impl->placementYaw)) +
                    " DEGREES";
                state.miningActive = false;
                state.miningFeedback.clear();
                return true;
            }
            const auto* kiln = m_impl->TraceFurnace(6.0f);
            if (kiln)
            {
                state.hasTarget=true;
                state.targetMaterial=kiln->fuelSeconds>0.0f ?
                    "CLAY FURNACE - HEATING" : "CLAY FURNACE";
                state.targetIsOre=true;
                state.miningActive=false;
                state.miningFeedback.clear();
                state.targetAssay = std::string(PrimitiveMetalName(kiln->metalId)) +
                    " " + std::to_string(static_cast<int>(kiln->readyCopperKg*1000.0f)) +
                    "g ready | Fuel " + std::to_string(static_cast<int>(kiln->fuelSeconds)) + "s";
                if (kiln->castCount != 0)
                    state.targetAssay += " | " + std::to_string(kiln->castCount) +
                        (kiln->castingCoolingSeconds > 0.0f ? " cooling" : " ingots ready");
                else if (kiln->chargeCopperKg > 0.0f)
                    state.targetAssay += " | Smelting " + kiln->chargeName;
                else if (kiln->moldCapacity != 0)
                    state.targetAssay += " | " + std::to_string(kiln->moldCapacity) +
                        "-slot mold installed";
                const auto& batchList = m_impl->inventory.OreBatches();
                if (!batchList.empty())
                {
                    const std::size_t index=m_impl->selectedFurnaceOre%batchList.size();
                    const auto& selected=batchList[index];
                    std::string name=selected.displayName;
                    if (name.size()>21)
                        name.resize(21);
                    state.targetAssay += " | V [" +
                        std::to_string(index+1) + "/" +
                        std::to_string(batchList.size()) + "] " + name;
                }
                else
                    state.targetAssay += " | V: NO ORE";
                return true;
            }
            const auto* built=m_impl->TraceConstruction(6.0f);
            if (built)
            {
                state.hasTarget = true;
                state.targetMaterial = std::string(BuildPieceName(built->kind));
                state.targetIsOre = true;
                state.miningActive = false;
                state.miningFeedback.clear();
                if (built->kind == BuildKind::Chest)
                    state.targetAssay = "B: OPEN CHEST | " +
                        std::to_string(built->contents.items.size()+
                            built->contents.oreBatches.size()) + " STACKS";
                else
                    state.targetAssay = "SNAPPED CABIN PIECE";
                return true;
            }
            const auto* fire = m_impl->TraceCampfire(5.5f);
            if (fire)
            {
                const bool lit = fire->fuelSeconds > 0.0f;
                state.hasTarget = true;
                state.targetMaterial = fire->covered ?
                    "CHARCOAL CLAMP - SMOULDERING" :
                    (lit ? "CAMPFIRE - BURNING" : "CAMPFIRE - UNLIT");
                state.targetIsOre = true;
                state.miningActive = false;
                state.miningFeedback.clear();
                state.targetAssay = "Fuel " +
                    std::to_string(static_cast<int>(std::ceil(fire->fuelSeconds))) +
                    "s | Charcoal " + std::to_string(fire->charcoalReady);
                if (!fire->covered && !lit && fire->charcoalReady == 0)
                    state.targetAssay += " | Ready to dismantle";
                return true;
            }
        }

        const std::uint64_t now = SDL_GetTicks();
        if (m_impl->targetLastUpdate == 0 ||
            now - m_impl->targetLastUpdate >= 90)
        {
            m_impl->targetLastUpdate = now == 0 ? 1 : now;
            m_impl->cachedTargetHit = false;
            m_impl->cachedTargetMaterial = TerrainMaterial::None;
            m_impl->cachedTargetDistance = 0.0f;
            m_impl->cachedVeinName.clear();
            m_impl->cachedVeinAssay.clear();
            m_impl->cachedVeinMineralId.clear();
            m_impl->cachedVeinId = 0;
            m_impl->cachedOutcropTargetHit = false;
            m_impl->cachedOreAssay.clear();
            m_impl->cachedOutcropDistance = 0.0f;
            const auto current = m_impl->terrainWorker.GetVoxelTerrain();
            const VoxelTerrain* terrain = m_impl->visibleCollisionSnapshot
                ? m_impl->visibleCollisionSnapshot.get() : current.get();
            if (terrain && !m_impl->cinematicMode)
            {
                const Vec3 origin = m_impl->playerCameraPosition;
                const Vec3 direction = Normalize(m_impl->playerCameraForward);
                float previousDensity = terrain->SampleDensity(
                    origin.x, origin.y, origin.z);
                constexpr float reach = 8.0f;
                constexpr float step = 0.30f;
                for (float distance = step; distance <= reach; distance += step)
                {
                    const Vec3 point = Add(origin, Multiply(direction, distance));
                    const float density = terrain->SampleDensity(
                        point.x, point.y, point.z);
                    if (previousDensity <= 0.0f && density > 0.0f)
                    {
                        float nearDistance = distance - step;
                        float farDistance = distance;
                        for (int i = 0; i < 5; ++i)
                        {
                            const float midpoint = 0.5f * (nearDistance + farDistance);
                            const Vec3 mid = Add(origin, Multiply(direction, midpoint));
                            if (terrain->SampleDensity(mid.x, mid.y, mid.z) > 0.0f)
                            {
                                farDistance = midpoint;
                            }
                            else
                            {
                                nearDistance = midpoint;
                            }
                        }
                        const Vec3 hit = Add(origin, Multiply(direction, farDistance));
                        constexpr float e = 0.22f;
                        const float dx = terrain->SampleDensity(
                            hit.x - e, hit.y, hit.z) -
                            terrain->SampleDensity(hit.x + e, hit.y, hit.z);
                        const float dy = terrain->SampleDensity(
                            hit.x, hit.y - e, hit.z) -
                            terrain->SampleDensity(hit.x, hit.y + e, hit.z);
                        const float dz = terrain->SampleDensity(
                            hit.x, hit.y, hit.z - e) -
                            terrain->SampleDensity(hit.x, hit.y, hit.z + e);
                        const Vec3 normal = Normalize({dx, dy, dz});
                        m_impl->cachedTargetHit = true;
                        m_impl->cachedTargetDistance = farDistance;
                        m_impl->cachedTargetMaterial = terrain->GetMaterialAt(
                            hit.x, hit.y, hit.z, normal.y);
                        if (m_impl->cachedTargetMaterial != TerrainMaterial::Bedrock &&
                            terrain->GetGeology() &&
                            terrain->GetSurfaceHeight(hit.x, hit.z) - hit.y > 2.5f &&
                            hit.y > VoxelTerrain::BedrockSurfaceY + 2.0f)
                        {
                            const auto veins = terrain->GetGeology()->FindUndergroundVeins(
                                hit.x - 0.25f, hit.x + 0.25f,
                                hit.y - 0.25f, hit.y + 0.25f,
                                hit.z - 0.25f, hit.z + 0.25f);
                            float strongest = 0.04f;
                            for (const GeologicalDeposit& vein : veins)
                            {
                                const float influence = GeologyGenerator::VeinInfluence(
                                    vein, hit.x, hit.y, hit.z);
                                if (influence > strongest)
                                {
                                    strongest = influence;
                                    const MineralDefinition* mineral =
                                        terrain->GetGeology()->FindMineral(vein.mineralId);
                                    m_impl->cachedVeinName = mineral
                                        ? mineral->name : vein.mineralId;
                                    m_impl->cachedVeinAssay = FormatOreAssay(
                                        vein, *terrain->GetGeology());
                                    m_impl->cachedVeinMineralId = vein.mineralId;
                                    m_impl->cachedVeinId = vein.id;
                                    m_impl->cachedVeinPoint = hit;
                                }
                            }
                        }
                        break;
                    }
                    previousDensity = density;
                }
                float oreDistance = 0.0f;
                const OutcropCollisionShape* outcrop =
                    m_impl->TraceOutcrop(8.0f, oreDistance);
                if (outcrop && (!m_impl->cachedTargetHit ||
                    oreDistance < m_impl->cachedTargetDistance))
                {
                    m_impl->cachedOreId = outcrop->id;
                    m_impl->cachedOreAssay = outcrop->assayLabel;
                    m_impl->cachedOreName = outcrop->mineralName +
                        (outcrop->kind == DepositKind::PermanentNode
                            ? " [PERMANENT NODE]" : " [FINITE DEPOSIT]");
                    m_impl->cachedOutcropDistance = oreDistance;
                    m_impl->cachedOutcropTargetHit = true;
                }
            }
        }

        state.targetIsOre = false;
        if (m_impl->cachedOutcropTargetHit)
        {
            state.hasTarget = true;
            state.targetDistance = m_impl->cachedOutcropDistance;
            state.targetMaterial = m_impl->cachedOreName;
            std::transform(state.targetMaterial.begin(),
                state.targetMaterial.end(), state.targetMaterial.begin(),
                [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            state.targetIsOre = true;
            state.targetAssay = m_impl->cachedOreAssay;
        }
        state.miningActive = m_impl->miningId != 0 ||
            m_impl->treeHarvestId != 0;
        state.miningName = m_impl->miningName;
        state.miningHits = m_impl->miningHits;
        state.miningHitsRequired = m_impl->miningHitsRequired;
        state.miningProgress = state.miningHitsRequired > 0
            ? std::clamp((static_cast<float>(m_impl->miningHits) +
                m_impl->miningElapsed / 0.72f) /
                static_cast<float>(state.miningHitsRequired), 0.0f, 1.0f)
            : 0.0f;
        if (m_impl->treeHarvestId != 0)
        {
            state.miningName = m_impl->treeHarvestName;
            state.miningHits = m_impl->treeHarvestHits;
            state.miningHitsRequired = 4;
            state.miningProgress = std::clamp(
                (static_cast<float>(state.miningHits) +
                    m_impl->treeHarvestElapsed / 0.65f) / 4.0f, 0.0f, 1.0f);
        }
        if (m_impl->completedFeedbackTimer > 0.0f)
        {
            state.miningFeedback = m_impl->completedFeedback;
        }
        if (state.targetIsOre)
        {
            ForestTree tree{};
            ForestPebble pebble{};
            float treeDistance = 0.0f;
            float pebbleDistance = 0.0f;
            if (m_impl->TraceTree(8.0f, tree, treeDistance) &&
                treeDistance < state.targetDistance)
            {
                state.targetIsOre = false;
                state.targetAssay.clear();
                state.targetDistance = treeDistance;
                state.targetMaterial = m_impl->TreeName(tree.species) +
                    "";
            }
            if (m_impl->TracePebble(8.0f, pebble, pebbleDistance) &&
                pebbleDistance < state.targetDistance)
            {
                state.targetIsOre = false;
                state.targetAssay.clear();
                state.targetDistance = pebbleDistance;
                state.targetMaterial = "LOOSE PEBBLE";
            }
            return true;
        }
        if (m_impl->cachedTargetHit && !m_impl->cachedVeinName.empty())
        {
            state.hasTarget = true;
            state.targetDistance = m_impl->cachedTargetDistance;
            state.targetMaterial = m_impl->cachedVeinName;
            std::transform(state.targetMaterial.begin(),
                state.targetMaterial.end(), state.targetMaterial.begin(),
                [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            state.targetIsOre = true;
            state.targetAssay = m_impl->cachedVeinAssay;
            return true;
        }
        state.hasTarget = m_impl->cachedTargetHit;
        state.targetDistance = m_impl->cachedTargetDistance;
        switch (m_impl->cachedTargetMaterial)
        {
            case TerrainMaterial::Grass: state.targetMaterial = "GRASS"; break;
            case TerrainMaterial::DirtClay: state.targetMaterial = "CLAY SOIL"; break;
            case TerrainMaterial::Stone: state.targetMaterial = "STONE"; break;
            case TerrainMaterial::Snow: state.targetMaterial = "SNOW"; break;
            case TerrainMaterial::Bedrock: state.targetMaterial = "BEDROCK"; break;
            default: state.targetMaterial = "NONE"; break;
        }
        ForestTree tree{};
        ForestPebble pebble{};
        float treeDistance = 0.0f;
        float pebbleDistance = 0.0f;
        const bool hasTree = m_impl->TraceTree(8.0f, tree, treeDistance);
        const bool hasPebble = m_impl->TracePebble(8.0f, pebble, pebbleDistance);
        float visibleDistance = state.hasTarget ? state.targetDistance : 8.0f;
        if (hasTree && treeDistance < visibleDistance &&
            !m_impl->OreBlocks(treeDistance))
        {
            state.hasTarget = true;
            state.targetDistance = treeDistance;
            state.targetMaterial = m_impl->TreeName(tree.species) +
                "";
            visibleDistance = treeDistance;
        }
        if (hasPebble && pebbleDistance < visibleDistance &&
            !m_impl->OreBlocks(pebbleDistance))
        {
            state.hasTarget = true;
            state.targetDistance = pebbleDistance;
            state.targetMaterial = "LOOSE PEBBLE";
        }
        return true;
    }

    bool Renderer::GetInventoryState(InventoryState& state) const
    {
        if (!m_impl || !m_impl->initialized)
        {
            return false;
        }
        state = m_impl->inventory.Capture();
        return true;
    }

    bool Renderer::DiscardInventorySlot(std::size_t slot)
    {
        if (!m_impl || !m_impl->initialized ||
            slot >= m_impl->inventory.UsedSlots())
        {
            return false;
        }
        const auto oreCount = m_impl->inventory.OreBatches().size();
        if (slot >= oreCount)
            return m_impl->inventory.RemoveItemSlot(slot - oreCount);
        const float mass = m_impl->inventory.OreBatches()[slot].massKg;
        return m_impl->inventory.RemoveOre(slot, mass);
    }

    bool Renderer::TransferStorageItem(PlayerInventory& storage,
        std::size_t logicalSlot, bool fromStorage, StorageTransferMode mode)
    {
        if (!m_impl || !m_impl->initialized)
            return false;
        PlayerInventory nextBag = m_impl->inventory;
        PlayerInventory nextStorage = storage;
        PlayerInventory& source = fromStorage ? nextStorage : nextBag;
        PlayerInventory& destination = fromStorage ? nextBag : nextStorage;
        if (!TransferInventoryStack(source, destination, logicalSlot, mode))
        {
            m_impl->Notify("TRANSFER BLOCKED - CHECK WEIGHT, SLOTS OR STACK LIMIT");
            return false;
        }
        m_impl->inventory = std::move(nextBag);
        storage = std::move(nextStorage);
        return true;
    }

    bool Renderer::ProcessInventorySlot(std::size_t slot,
        ProcessingOperation operation, std::string& message)
    {
        if (!m_impl || !m_impl->initialized ||
            slot >= m_impl->inventory.OreBatches().size())
        {
            message = "No ore batch is selected";
            return false;
        }
        const InventoryOreBatch original = m_impl->inventory.OreBatches()[slot];
        const ProcessingResult result = OreProcessing::Execute(original, operation);
        message = result.message;
        if (!result.success)
        {
            return false;
        }

        PlayerInventory next;
        const InventoryState old = m_impl->inventory.Capture();
        if (!next.Restore(old) || !next.RemoveOre(slot, original.massKg))
        {
            message = "Could not remove the original batch";
            return false;
        }
        for (const auto& output : result.outputs)
        {
            if (!next.AddOre(output))
            {
                message = "Not enough inventory slots for processing outputs";
                return false;
            }
        }
        if (!m_impl->inventory.Restore(next.Capture()))
        {
            message = "Inventory could not accept processing results";
            return false;
        }
        return true;
    }

    bool Renderer::SetCarryCapacityBonus(float bonusKg)
    {
        return m_impl && m_impl->initialized &&
            m_impl->inventory.SetCarryCapacityBonus(bonusKg);
    }

    bool Renderer::CaptureWorld(WorldSave& save) const
    {
        if (!m_impl || !m_impl->initialized)
            return false;
        const auto voxel = m_impl->terrainWorker.GetVoxelTerrain();
        if (!voxel)
            return false;
        save.seed = m_impl->terrainWorld.GetSettings().seed;
        save.x = m_impl->player.GetX();
        save.y = m_impl->player.GetY();
        save.z = m_impl->player.GetZ();
        save.yaw = m_impl->player.GetYaw();
        save.pitch = m_impl->player.GetPitch();
        save.edits = voxel->GetEdits();
        save.depletedDeposits = voxel->GetDepletedDeposits();
        save.felledTrees.assign(m_impl->felledTreeIds.begin(),
            m_impl->felledTreeIds.end());
        save.strippedTrees.assign(m_impl->strippedTreeIds.begin(),
            m_impl->strippedTreeIds.end());
        save.collectedPebbles.assign(m_impl->gatheredPebbleIds.begin(),
            m_impl->gatheredPebbleIds.end());
        save.campfires = m_impl->campfires.Fires();
        save.furnaces = m_impl->furnaces.Furnaces();
        save.structures = m_impl->constructions.Structures();
        save.moldCoolingSeconds = m_impl->moldCoolingSeconds;
        save.inventory = m_impl->inventory.Capture();
        return true;
    }

    bool Renderer::StartFreshWorld(int seed)
    {
        if (!m_impl || !m_impl->initialized)
            return false;
        m_impl->terrainWorker.Stop();
        m_impl->ClearGPUChunks();
        m_impl->hasSafePlayerCameraPosition = false;
        m_impl->hasSafePlayerFeetPosition = false;
        m_impl->hasCheckedCameraPosition = false;
        m_impl->targetLastUpdate = 0;
        m_impl->nearbyOutcrops.clear();
        m_impl->lastOutcropX = std::numeric_limits<float>::max();
        m_impl->lastOutcropZ = std::numeric_limits<float>::max();
        m_impl->lastOutcropRevision = std::numeric_limits<std::uint64_t>::max();
        TerrainSettings settings = m_impl->terrainWorld.GetSettings();
        settings.seed = seed;
        const float chunkSize = m_impl->terrainWorld.GetChunkSize();
        const float spacing = m_impl->terrainWorld.GetVertexSpacing();
        m_impl->terrainWorld = TerrainWorld(settings, chunkSize, spacing);
        m_impl->ResetMining();
        m_impl->felledTreeIds.clear();
        m_impl->strippedTreeIds.clear();
        m_impl->gatheredPebbleIds.clear();
        m_impl->campfires.Clear();
        m_impl->furnaces.Clear();
        m_impl->constructions.Clear();
        m_impl->constructionsDirty = true;
        m_impl->moldCoolingSeconds=0.0f;
        m_impl->furnacePropsDirty=true;
        m_impl->furnaceParticlesDirty=true;
        m_impl->inventory.Clear();
        m_impl->player.Initialize(m_impl->terrainWorld.GetGenerator());
        m_impl->UpdatePlayerCameraPose();
        return m_impl->StartTerrainGeneration();
    }

    bool Renderer::RestoreWorld(const WorldSave& save)
    {
        if (!m_impl || !m_impl->initialized)
            return false;
        m_impl->terrainWorker.Stop();
        m_impl->ClearGPUChunks();
        m_impl->hasSafePlayerCameraPosition = false;
        m_impl->hasSafePlayerFeetPosition = false;
        m_impl->hasCheckedCameraPosition = false;
        m_impl->targetLastUpdate = 0;
        m_impl->nearbyOutcrops.clear();
        m_impl->lastOutcropX = std::numeric_limits<float>::max();
        m_impl->lastOutcropZ = std::numeric_limits<float>::max();
        m_impl->lastOutcropRevision = std::numeric_limits<std::uint64_t>::max();
        TerrainSettings settings = m_impl->terrainWorld.GetSettings();
        settings.seed = save.seed;
        const float chunkSize = m_impl->terrainWorld.GetChunkSize();
        const float spacing = m_impl->terrainWorld.GetVertexSpacing();
        m_impl->terrainWorld = TerrainWorld(settings, chunkSize, spacing);
        m_impl->ResetMining();
        m_impl->felledTreeIds = {save.felledTrees.begin(), save.felledTrees.end()};
        m_impl->strippedTreeIds = {save.strippedTrees.begin(), save.strippedTrees.end()};
        m_impl->gatheredPebbleIds = {save.collectedPebbles.begin(),
            save.collectedPebbles.end()};
        if (!m_impl->campfires.Restore(save.campfires) ||
            !m_impl->furnaces.Restore(save.furnaces) ||
            !m_impl->constructions.Restore(save.structures))
            return false;
        m_impl->moldCoolingSeconds=save.moldCoolingSeconds;
        m_impl->furnacePropsDirty=true;
        m_impl->furnaceParticlesDirty=true;
        m_impl->campfirePropsDirty = true;
        m_impl->campfireParticlesDirty = true;
        if (!m_impl->inventory.Restore(save.inventory))
        {
            return false;
        }
        m_impl->terrainWorker.Start(m_impl->terrainWorld.GetSettings(),
            m_impl->terrainWorld.GetChunkSize(),
            m_impl->terrainWorld.GetVertexSpacing());
        if (!m_impl->terrainWorker.IsRunning() ||
            !m_impl->terrainWorker.ReplaceEdits(save.edits) ||
            !m_impl->terrainWorker.ReplaceDepletedDeposits(save.depletedDeposits))
            return false;
        m_impl->player.Initialize(m_impl->terrainWorld.GetGenerator());

        float restoredY = save.y;
        if (restoredY <= VoxelTerrain::BedrockSurfaceY + 0.25f)
        {
            const auto restoredTerrain = m_impl->terrainWorker.GetVoxelTerrain();
            const float surfaceY = restoredTerrain
                ? restoredTerrain->GetSurfaceHeight(save.x, save.z)
                : m_impl->terrainWorld.GetGenerator().GetHeight(save.x, save.z);
            restoredY = surfaceY + 3.0f;
            std::cout << "Loaded position was below bedrock; "
                      << "moving player to the surface.\n";
        }

        m_impl->player.SetPosition(save.x, restoredY, save.z);
        m_impl->player.SetViewRotation(save.yaw, save.pitch);
        m_impl->UpdatePlayerCameraPose();
        m_impl->UpdateActiveCamera();
        m_impl->UpdateStreamingCenter();
        m_impl->RequestNearbyChunks();
        return true;
    }

    void Renderer::SetCinematicMode(bool enabled)
    {
        if (!m_impl || !m_impl->initialized)
        {
            return;
        }

        if (m_impl->cinematicMode == enabled)
        {
            return;
        }

        m_impl->cinematicMode = enabled;

        m_impl->UpdatePlayerCameraPose();
        m_impl->UpdateActiveCamera();
    }

    void Renderer::UpdateCinematic(
        float deltaTime,
        bool introduction,
        float progress)
    {
        if (!m_impl ||
            !m_impl->initialized ||
            !m_impl->cinematicMode)
        {
            return;
        }

        m_impl->UpdateCinematicPose(
            deltaTime,
            introduction,
            std::clamp(progress, 0.0f, 1.0f)
        );
    }

    void Renderer::Render(int width, int height)
    {
        if (!m_impl ||
            !m_impl->initialized ||
            width <= 0 ||
            height <= 0)
        {
            return;
        }

        m_impl->StreamTerrain();
        m_impl->UpdateCampfireMeshes();
        m_impl->UpdateFurnaceMeshes();
        m_impl->UpdateConstructionMeshes();
        m_impl->UpdatePreviewGeometry();
        m_impl->UpdateActiveCamera();
        if (m_impl->equippedItem=="hot_copper_mold" ||
            m_impl->equippedItem=="cooled_copper_mold")
        {
            VoxelMesh held;
            ClayFurnaceVisuals::BuildHeldMold(
                m_impl->playerCameraPosition.x,
                m_impl->playerCameraPosition.y,
                m_impl->playerCameraPosition.z,
                m_impl->playerCameraForward.x,
                m_impl->playerCameraForward.y,
                m_impl->playerCameraForward.z,
                m_impl->equippedItem=="hot_copper_mold",held);
            m_impl->UploadCampfireMesh(held,m_impl->heldMold);
        }
        else if (m_impl->heldMold.indexCount>0)
        {
            VoxelMesh empty;
            m_impl->UploadCampfireMesh(empty,m_impl->heldMold);
        }

        glViewport(
            0,
            0,
            width,
            height
        );

        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT
        );

        const Mat4 projection = Perspective(
            70.0f * DegreesToRadians,
            static_cast<float>(width) /
                static_cast<float>(height),
            m_impl->cinematicMode ? 0.10f : 0.075f,
            2000.0f
        );

        const Mat4 view = LookAt(
            m_impl->cameraPosition,
            m_impl->cameraTarget,
            {0.0f, 1.0f, 0.0f}
        );

        const Mat4 model = Identity();

        const Mat4 viewProjection =
            MultiplyMatrices(projection, view);

        const Mat4 mvp =
            MultiplyMatrices(viewProjection, model);

        m_impl->terrainShader.Use();

        m_impl->terrainShader.SetMat4(
            "uMVP",
            mvp.values
        );

        m_impl->terrainShader.SetMat4(
            "uModel",
            model.values
        );

        m_impl->SetLighting();

        glDisable(GL_CULL_FACE);

        for (const auto& entry : m_impl->gpuChunks)
        {
            const Impl::GPUChunk& chunk =
                entry.second;

            m_impl->BindVertexArray(
                chunk.vertexArray
            );

            m_impl->DrawElements(
                GL_TRIANGLES,
                chunk.indexCount,
                GL_UNSIGNED_INT,
                nullptr
            );
        }

        for (const auto& entry : m_impl->forestChunks)
        {
            const Impl::GPUChunk& mesh = entry.second;
            m_impl->BindVertexArray(mesh.vertexArray);
            m_impl->DrawElements(
                GL_TRIANGLES,
                mesh.indexCount,
                GL_UNSIGNED_INT,
                nullptr
            );
        }

        if (m_impl->campfireProps.indexCount > 0)
        {
            m_impl->BindVertexArray(m_impl->campfireProps.vertexArray);
            m_impl->DrawElements(GL_TRIANGLES,m_impl->campfireProps.indexCount,
                GL_UNSIGNED_INT,nullptr);
        }
        for (const auto* mesh : {&m_impl->furnaceProps,
            &m_impl->constructionProps,&m_impl->heldMold})
        {
            if (mesh->indexCount>0)
            {
                m_impl->BindVertexArray(mesh->vertexArray);
                m_impl->DrawElements(GL_TRIANGLES,mesh->indexCount,
                    GL_UNSIGNED_INT,nullptr);
            }
        }
        if (m_impl->previewMesh.indexCount > 0)
        {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            glLineWidth(1.5f);
            m_impl->BindVertexArray(m_impl->previewMesh.vertexArray);
            m_impl->DrawElements(GL_TRIANGLES,
                m_impl->previewMesh.indexCount, GL_UNSIGNED_INT, nullptr);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glLineWidth(1.0f);
        }
        m_impl->BindVertexArray(0);

        if (m_impl->campfireFlames.indexCount>0 ||
            m_impl->campfireSmoke.indexCount>0)
        {
            m_impl->campfireParticleShader.Use();
            m_impl->campfireParticleShader.SetMat4("uMVP",mvp.values);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            if (m_impl->campfireSmoke.indexCount>0)
            {
                m_impl->campfireParticleShader.SetFloat("uOpacity",0.23f);
                m_impl->BindVertexArray(m_impl->campfireSmoke.vertexArray);
                m_impl->DrawElements(GL_TRIANGLES,m_impl->campfireSmoke.indexCount,
                    GL_UNSIGNED_INT,nullptr);
            }
            if (m_impl->campfireFlames.indexCount>0)
            {
                m_impl->campfireParticleShader.SetFloat("uOpacity",0.82f);
                m_impl->BindVertexArray(m_impl->campfireFlames.vertexArray);
                m_impl->DrawElements(GL_TRIANGLES,m_impl->campfireFlames.indexCount,
                    GL_UNSIGNED_INT,nullptr);
            }
            m_impl->BindVertexArray(0);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
        if (m_impl->furnaceFlames.indexCount>0 ||
            m_impl->furnaceSmoke.indexCount>0)
        {
            m_impl->campfireParticleShader.Use();
            m_impl->campfireParticleShader.SetMat4("uMVP",mvp.values);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            glDepthMask(GL_FALSE);
            if (m_impl->furnaceSmoke.indexCount>0)
            {
                m_impl->campfireParticleShader.SetFloat("uOpacity",0.23f);
                m_impl->BindVertexArray(m_impl->furnaceSmoke.vertexArray);
                m_impl->DrawElements(GL_TRIANGLES,m_impl->furnaceSmoke.indexCount,
                    GL_UNSIGNED_INT,nullptr);
            }
            if (m_impl->furnaceFlames.indexCount>0)
            {
                m_impl->campfireParticleShader.SetFloat("uOpacity",0.83f);
                m_impl->BindVertexArray(m_impl->furnaceFlames.vertexArray);
                m_impl->DrawElements(GL_TRIANGLES,m_impl->furnaceFlames.indexCount,
                    GL_UNSIGNED_INT,nullptr);
            }
            m_impl->BindVertexArray(0);
            glDepthMask(GL_TRUE);
            glDisable(GL_BLEND);
        }
        glEnable(GL_CULL_FACE);
    }

    void Renderer::Shutdown()
    {
        if (!m_impl)
        {
            return;
        }

        m_impl->StopTerrainGeneration();
        m_impl->ClearGPUChunks();
        m_impl->terrainWorld.Clear();
        m_impl->terrainShader.Shutdown();
        m_impl->campfireParticleShader.Shutdown();

        m_impl->initialized = false;
        m_impl->cinematicMode = false;
        m_impl->cinematicTime = 0.0f;
    }
}
