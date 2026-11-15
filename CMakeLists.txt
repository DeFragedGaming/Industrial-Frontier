
cmake_minimum_required(VERSION 3.25)

set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)

project(IndustrialFrontier
    VERSION 0.1.0
    DESCRIPTION "Industrial Frontier - Mining and Industrial Automation Simulation"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL3
    URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.18/SDL3-3.4.18.zip
    URL_HASH SHA256=9cd42377704398796071b8597cd7e21da254a43bad98c4199739647adc13fa6f
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(SDL3)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDLTTF_INSTALL OFF CACHE BOOL "" FORCE)

# Use isolated dependency directories to avoid the earlier failed
# git clone into SDL3_ttf's own source tree.
FetchContent_Declare(
    industrial_frontier_sdl3_ttf
    URL https://github.com/libsdl-org/SDL_ttf/releases/download/release-3.2.2/SDL3_ttf-3.2.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _industrial_frontier_skip_automatic_configuration
)
FetchContent_MakeAvailable(industrial_frontier_sdl3_ttf)
FetchContent_GetProperties(industrial_frontier_sdl3_ttf
    SOURCE_DIR INDUSTRIAL_FRONTIER_SDL3_TTF_SOURCE_DIR
    BINARY_DIR INDUSTRIAL_FRONTIER_SDL3_TTF_BINARY_DIR
)

# Fetch FreeType independently; never clone into external/freetype.
FetchContent_Declare(
    industrial_frontier_freetype_vendor
    URL https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR _industrial_frontier_skip_automatic_configuration
)
FetchContent_MakeAvailable(industrial_frontier_freetype_vendor)
FetchContent_GetProperties(industrial_frontier_freetype_vendor
    SOURCE_DIR INDUSTRIAL_FRONTIER_FREETYPE_SOURCE_DIR
)

if(NOT EXISTS "${INDUSTRIAL_FRONTIER_FREETYPE_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "FreeType source archive is incomplete. Check network access and CMake FetchContent output.")
endif()

file(MAKE_DIRECTORY "${INDUSTRIAL_FRONTIER_SDL3_TTF_SOURCE_DIR}/external/freetype")
file(COPY "${INDUSTRIAL_FRONTIER_FREETYPE_SOURCE_DIR}/"
    DESTINATION "${INDUSTRIAL_FRONTIER_SDL3_TTF_SOURCE_DIR}/external/freetype"
)

if(NOT EXISTS "${INDUSTRIAL_FRONTIER_SDL3_TTF_SOURCE_DIR}/external/freetype/CMakeLists.txt")
    message(FATAL_ERROR "Could not stage FreeType for SDL3_ttf.")
endif()

add_subdirectory(
    "${INDUSTRIAL_FRONTIER_SDL3_TTF_SOURCE_DIR}"
    "${INDUSTRIAL_FRONTIER_SDL3_TTF_BINARY_DIR}"
)


find_package(OpenGL REQUIRED)

add_executable(IndustrialFrontier
    src/main.cpp

    src/core/Application.cpp
    src/core/GameFlow.cpp

    src/player/PlayerController.cpp

    src/renderer/Renderer.cpp
    src/renderer/Shader.cpp

    src/ui/UIRenderer.cpp
    src/ui/MainMenu.cpp
    src/ui/GameHUD.cpp
    src/ui/SaveOverlay.cpp

    src/save/SaveManager.cpp

    src/resources/MineralDatabase.cpp
    src/resources/GeologyGenerator.cpp
    src/resources/OreAssay.cpp
    src/resources/PlayerInventory.cpp
    src/resources/Campfire.cpp
    src/renderer/CampfireVisuals.cpp
    src/renderer/BuildStructureVisuals.cpp
    src/resources/FieldGuideCatalog.cpp
    src/resources/OreProcessing.cpp
    src/ui/InventoryOverlay.cpp
    src/ui/ItemWorkspaceOverlay.cpp
    src/ui/FieldGuideOverlay.cpp
    src/ui/ProcessingOverlay.cpp
    src/ui/SeedOverlay.cpp

    src/world/TerrainGenerator.cpp
    src/world/TerrainChunk.cpp
    src/world/TerrainWorld.cpp
    src/world/TerrainGenerationWorker.cpp
    src/world/VoxelTerrain.cpp
    src/world/VoxelMesher.cpp
    src/forest/ForestGenerator.cpp
)

target_include_directories(IndustrialFrontier
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(IndustrialFrontier
    PRIVATE
        SDL3::SDL3
        SDL3_ttf::SDL3_ttf
        OpenGL::GL
)

if(MSVC)
    target_compile_options(IndustrialFrontier
        PRIVATE
            /W4
            /permissive-
    )
else()
    target_compile_options(IndustrialFrontier
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

add_custom_command(
    TARGET IndustrialFrontier
    POST_BUILD

    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:IndustrialFrontier>/assets"

    COMMENT "Copying Industrial Frontier assets"
)

if(WIN32)
    add_custom_command(
        TARGET IndustrialFrontier
        POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3>"
            "$<TARGET_FILE_DIR:IndustrialFrontier>"

        COMMENT "Copying SDL3 runtime"
    )
endif()

option(INDUSTRIAL_FRONTIER_BUILD_DATABASE_TESTS "Build mineral database smoke tests" ON)

if(INDUSTRIAL_FRONTIER_BUILD_DATABASE_TESTS)
    enable_testing()

    add_executable(IndustrialFrontierMineralDatabaseTest
        tests/MineralDatabaseSmoke.cpp
        src/resources/MineralDatabase.cpp
    )

    target_include_directories(IndustrialFrontierMineralDatabaseTest
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    add_test(
        NAME IndustrialFrontier_MineralDatabase
        COMMAND IndustrialFrontierMineralDatabaseTest
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/data/minerals.tsv"
    )
endif()

add_executable(IndustrialFrontierGeologyTest
    tests/GeologyGeneratorSmoke.cpp
    src/resources/GeologyGenerator.cpp
    src/resources/MineralDatabase.cpp
    src/resources/OreAssay.cpp
    src/world/TerrainGenerator.cpp
    src/world/VoxelTerrain.cpp
    src/world/VoxelMesher.cpp
)

target_include_directories(IndustrialFrontierGeologyTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()
add_test(NAME IndustrialFrontier_GeologyGenerator
    COMMAND IndustrialFrontierGeologyTest
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/data/minerals.tsv"
)

add_executable(IndustrialFrontierOreInteractionTest
    tests/OreInteractionSmoke.cpp
    src/player/PlayerController.cpp
    src/resources/MineralDatabase.cpp
    src/resources/GeologyGenerator.cpp
    src/resources/OreAssay.cpp
    src/world/TerrainGenerator.cpp
    src/world/VoxelTerrain.cpp
)

target_include_directories(IndustrialFrontierOreInteractionTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME IndustrialFrontier_OreInteraction
    COMMAND IndustrialFrontierOreInteractionTest
)

add_executable(IndustrialFrontierOreAssayTest
    tests/OreAssaySmoke.cpp
    src/resources/OreAssay.cpp
    src/resources/GeologyGenerator.cpp
    src/resources/MineralDatabase.cpp
    src/world/TerrainGenerator.cpp
    src/world/VoxelTerrain.cpp
)

target_include_directories(IndustrialFrontierOreAssayTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_test(NAME IndustrialFrontier_OreAssay
    COMMAND IndustrialFrontierOreAssayTest
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/data/minerals.tsv"
)

add_executable(IndustrialFrontierInventoryTest
    tests/PlayerInventorySmoke.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierInventoryTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME IndustrialFrontier_PlayerInventory
    COMMAND IndustrialFrontierInventoryTest
)

add_executable(IndustrialFrontierCaveDistributionTest
    tests/CaveDistributionSmoke.cpp
    src/resources/GeologyGenerator.cpp
    src/resources/MineralDatabase.cpp
    src/world/TerrainGenerator.cpp
    src/world/VoxelTerrain.cpp
)
target_include_directories(IndustrialFrontierCaveDistributionTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME IndustrialFrontier_CaveDistribution
    COMMAND IndustrialFrontierCaveDistributionTest
)

add_executable(IndustrialFrontierFieldGuideTest
    tests/FieldGuideSmoke.cpp
    src/resources/FieldGuideCatalog.cpp
    src/resources/MineralDatabase.cpp
    src/ui/FieldGuideOverlay.cpp
)
target_include_directories(IndustrialFrontierFieldGuideTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(IndustrialFrontierFieldGuideTest
    PRIVATE SDL3::SDL3
)
add_test(NAME IndustrialFrontier_FieldGuide
    COMMAND IndustrialFrontierFieldGuideTest
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/data/minerals.tsv"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/data/guide_processing.tsv"
)

add_executable(IndustrialFrontierOreProcessingTest
    tests/OreProcessingSmoke.cpp
    src/resources/OreProcessing.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierOreProcessingTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME IndustrialFrontier_OreProcessing
    COMMAND IndustrialFrontierOreProcessingTest
)

add_executable(IndustrialFrontierPrimitiveGatheringTest
    tests/PrimitiveGatheringSmoke.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierPrimitiveGatheringTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME IndustrialFrontier_PrimitiveGathering
    COMMAND IndustrialFrontierPrimitiveGatheringTest
)

add_executable(IndustrialFrontierForestTest
    tests/ForestGeneratorSmoke.cpp
    src/forest/ForestGenerator.cpp
    src/world/TerrainGenerator.cpp
    src/world/VoxelTerrain.cpp
    src/resources/GeologyGenerator.cpp
    src/resources/MineralDatabase.cpp
)
target_include_directories(IndustrialFrontierForestTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME IndustrialFrontier_ForestGenerator
    COMMAND IndustrialFrontierForestTest
)

add_executable(IndustrialFrontierItemWorkspaceTest
    tests/ItemWorkspaceSmoke.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierItemWorkspaceTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_ItemWorkspace
    COMMAND IndustrialFrontierItemWorkspaceTest)

add_executable(IndustrialFrontierHotbarDurabilityTest
    tests/HotbarDurabilitySmoke.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierHotbarDurabilityTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_HotbarDurability
    COMMAND IndustrialFrontierHotbarDurabilityTest)

add_executable(IndustrialFrontierCampfireTest
    tests/CampfireSmoke.cpp
    src/resources/Campfire.cpp
)
target_include_directories(IndustrialFrontierCampfireTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME IndustrialFrontier_Campfire
    COMMAND IndustrialFrontierCampfireTest
)

add_executable(IndustrialFrontierCampfireVisualTest
    tests/CampfireVisualSmoke.cpp
    src/renderer/CampfireVisuals.cpp
)
target_include_directories(IndustrialFrontierCampfireVisualTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME IndustrialFrontier_CampfireVisuals
    COMMAND IndustrialFrontierCampfireVisualTest
)

add_executable(IndustrialFrontierCampfireCraftTest
    tests/CampfireCraftSmoke.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierCampfireCraftTest
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME IndustrialFrontier_CampfireCraft
    COMMAND IndustrialFrontierCampfireCraftTest
)

add_executable(IndustrialFrontierCampfireDismantleTest
    tests/CampfireDismantleSmoke.cpp
    src/resources/Campfire.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierCampfireDismantleTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
add_test(NAME IndustrialFrontier_CampfireDismantle
    COMMAND IndustrialFrontierCampfireDismantleTest
)

target_sources(IndustrialFrontier PRIVATE
    src/resources/EarlyMetallurgy.cpp
    src/renderer/ClayFurnaceVisuals.cpp
)

add_executable(IndustrialFrontierEarlyMetallurgyTest
    tests/EarlyMetallurgySmoke.cpp
    src/resources/EarlyMetallurgy.cpp
    src/resources/OreAssay.cpp
    src/resources/MineralDatabase.cpp
    src/resources/GeologyGenerator.cpp
    src/world/TerrainGenerator.cpp
    src/world/VoxelTerrain.cpp
    src/world/VoxelMesher.cpp
)
target_include_directories(IndustrialFrontierEarlyMetallurgyTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_EarlyMetallurgy
    COMMAND IndustrialFrontierEarlyMetallurgyTest
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/data/minerals.tsv")

add_executable(IndustrialFrontierEarlyMetallurgyCraftTest
    tests/EarlyMetallurgyCraftSmoke.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierEarlyMetallurgyCraftTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_EarlyMetallurgyCraft
    COMMAND IndustrialFrontierEarlyMetallurgyCraftTest)

add_executable(IndustrialFrontierClayFurnaceVisualTest
    tests/ClayFurnaceVisualSmoke.cpp
    src/renderer/ClayFurnaceVisuals.cpp
)
target_include_directories(IndustrialFrontierClayFurnaceVisualTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_ClayFurnaceVisual
    COMMAND IndustrialFrontierClayFurnaceVisualTest)

add_executable(IndustrialFrontierStorageTransferTest
    tests/StorageTransferPrimitiveSmoke.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierStorageTransferTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_StorageTransfer
    COMMAND IndustrialFrontierStorageTransferTest)

add_executable(IndustrialFrontierCopperTierTest
    tests/CopperTierSmoke.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierCopperTierTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_CopperTier
    COMMAND IndustrialFrontierCopperTierTest)

add_executable(IndustrialFrontierPrimitiveCastingTest
    tests/PrimitiveBatchCastingSmoke.cpp
    src/resources/EarlyMetallurgy.cpp
    src/resources/OreAssay.cpp
    src/resources/MineralDatabase.cpp
    src/resources/GeologyGenerator.cpp
    src/world/TerrainGenerator.cpp
    src/world/VoxelTerrain.cpp
    src/world/VoxelMesher.cpp
)
target_include_directories(IndustrialFrontierPrimitiveCastingTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_PrimitiveCasting
    COMMAND IndustrialFrontierPrimitiveCastingTest
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/data/minerals.tsv")

add_executable(IndustrialFrontierBuildStructuresTest
    tests/BuildStructuresSmoke.cpp
    src/resources/PlayerInventory.cpp
)
target_include_directories(IndustrialFrontierBuildStructuresTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_BuildStructures
    COMMAND IndustrialFrontierBuildStructuresTest)

add_executable(IndustrialFrontierBuildCollisionTest
    tests/BuildCollisionSmoke.cpp
)
target_include_directories(IndustrialFrontierBuildCollisionTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_BuildCollision
    COMMAND IndustrialFrontierBuildCollisionTest)

add_executable(IndustrialFrontierCraftingLayoutTest
    tests/CraftingLayoutSmoke.cpp
)
target_include_directories(IndustrialFrontierCraftingLayoutTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
add_test(NAME IndustrialFrontier_CraftingLayout
    COMMAND IndustrialFrontierCraftingLayoutTest)
